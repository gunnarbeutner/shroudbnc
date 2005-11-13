/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005 Gunnar Beutner                                           *
 *                                                                             *
 * This program is free software; you can redistribute it and/or               *
 * modify it under the terms of the GNU General Public License                 *
 * as published by the Free Software Foundation; either version 2              *
 * of the License, or (at your option) any later version.                      *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. *
 *******************************************************************************/

#include "StdAfx.h"

extern "C" {
	#include "md5-c/global.h"
	#include "md5-c/md5.h"
}

extern bool g_Debug;

const char* g_ErrorFile;
unsigned int g_ErrorLine;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

SOCKET g_last_sock = 0;
time_t g_LastReconnect = 0;
extern int g_TimerStats;

CBouncerCore::CBouncerCore(CBouncerConfig* Config, int argc, char** argv) {
	int i;
	char* Out;

	m_Log = new CBouncerLog("sbnc.log");
	m_Log->Clear();
	m_Log->WriteLine("Log system initialized.");

	if (m_Log == NULL) {
		printf("Log system could not be initialized. Shutting down.");

		exit(1);
	}

	m_TimerChain.next = NULL;
	m_TimerChain.ptr = NULL;

	g_Bouncer = this;

	m_Config = Config;

	m_Users = NULL;
	m_UserCount = 0;
	
	m_Modules = NULL;
	m_ModuleCount = 0;

	m_argc = argc;
	m_argv = argv;

	m_Ident = new CIdentSupport();

	const char* Users = Config->ReadString("system.users");

	if (Users) {
		const char* Args;
		int Count;

		Args = ArgTokenize(Users);

		if (Args == NULL) {
			LOGERROR("ArgTokenize() failed.");

			Fatal();
		}

		Count = ArgCount(Args);

		m_Users = (CBouncerUser**)malloc(sizeof(CBouncerUser*) * Count);
		m_UserCount = Count;

		if (m_Users == NULL) {
			LOGERROR("malloc() failed. could not allocate user array");

			Fatal();
		}

		for (i = 0; i < Count; i++) {
			m_Users[i] = new CBouncerUser(ArgGet(Args, i + 1));

			if (m_Users[i] == NULL) {
				LOGERROR("Could not create user object");

				Fatal();
			}
		}

		for (i = 0; i < Count; i++)
			m_Users[i]->LoadEvent();

		ArgFree(Args);
	} else {
		Log("No users were found in the config file.");

		Fatal();
	}

	m_OtherSockets = NULL;
	m_OtherSocketCount = 0;

	m_Listener = INVALID_SOCKET;

	m_Startup = time(NULL);

	m_LoadingModules = true;

	i = 0;
	while (true) {
		asprintf(&Out, "system.modules.mod%d", i++);

		if (Out == NULL) {
			LOGERROR("asprintf() failed. Module could not be loaded.");

			continue;
		}

		const char* File = m_Config->ReadString(Out);

		free(Out);

		if (File)
			LoadModule(File);
		else
			break;
	}

	m_SendQSizeCache = -1;

	m_LoadingModules = false;

#if defined(_DEBUG) && defined(_WIN32)
	if (Config->ReadInteger("system.debug"))
		g_Debug = true;
#endif
}

CBouncerCore::~CBouncerCore() {
	if (m_Listener != INVALID_SOCKET)
		closesocket(m_Listener);

	for (int a = 0; a < m_ModuleCount; a++) {
		if (m_Modules[a])
			delete m_Modules[a];
	}

	free(m_Modules);

	m_ModuleCount = 0;

	for (int i = 0; i < m_UserCount; i++) {
		if (m_Users[i])
			delete m_Users[i];
	}

	free(m_Users);

	for (int c = 0; c < m_OtherSocketCount; c++) {
		if (m_OtherSockets[c].Socket != INVALID_SOCKET) {
			m_OtherSockets[c].Events->Destroy();
			closesocket(m_OtherSockets[c].Socket);
		}
	}

	free(m_OtherSockets);

	delete m_Log;
	delete m_Ident;

	if (m_TimerChain.next) {
		timerchain_t* current = m_TimerChain.next;

		while (current) {
			timerchain_t* p = current;
			current = current->next;

			free(p);
		}
	}
}

void CBouncerCore::StartMainLoop(void) {
	bool b_DontDetach = false;

	puts("shroudBNC" BNCVERSION " - an object-oriented IRC bouncer");

	int argc = m_argc;
	char** argv = m_argv;

	for (int a = 1; a < argc; a++) {
		if (strcmp(argv[a], "-n") == 0)
			b_DontDetach = true;
		if (strcmp(argv[a], "--help") == 0) {
			puts("");
			printf("Syntax: %s [OPTION]", argv[0]);
			puts("");
			puts("Options:");
			puts("\t-n\tdon't detach");
			puts("\t--help\tdisplay this help and exit");

			return;
		}
	}

	int Port = m_Config->ReadInteger("system.port");

	if (Port == 0)
		Port = 9000;

	const char* BindIp = g_Bouncer->GetConfig()->ReadString("system.ip");

	m_Listener = CreateListener(Port, BindIp);

	if (m_Listener == INVALID_SOCKET) {
		Log("Could not create listener port");
		return;
	}

	Log("Created main listener.");

	fd_set FDRead, FDWrite;

	Log("Starting main loop.");

	if (!b_DontDetach)
		Daemonize();

	m_Running = true;
	int m_ShutdownLoop = 5;

	time_t Last = 0;
	time_t LastCheck = 0;

	while (m_Running || --m_ShutdownLoop) {
		timerchain_t* current = &m_TimerChain;
		time_t Now = time(NULL);
		time_t Best = 0;
		int SleepInterval = 0;

		while (current && current->ptr) {
			timerchain_t* next = current->next;

			time_t NextCall = current->ptr->GetNextCall();

			if (Now >= NextCall && Now > Last) {
				if (Now - 5 > NextCall)
					Log("Timer drift for timer %p: %d seconds", current->ptr, Now - NextCall);

				current->ptr->Call(Now);
				Best = Now + 1;
			} else if (Best == 0 || NextCall < Best) {
				Best = NextCall;
			}

			current = next;
		}

		if (Best)
			SleepInterval = Best - Now;

		FD_ZERO(&FDRead);
		FD_SET(m_Listener, &FDRead);

		FD_ZERO(&FDWrite);

		int i;

		for (i = 0; i < m_UserCount; i++) {
			CIRCConnection* IRC;

			if (m_Users[i] && (IRC = m_Users[i]->GetIRCConnection())) {
				if (!m_Running && !IRC->IsLocked()) {
					Log("Closing connection for %s", m_Users[i]->GetUsername());
					IRC->InternalWriteLine("QUIT :Shutting down.");
					IRC->Lock();
				}

				if (IRC->ShouldDestroy())
					IRC->Destroy();
			}
		}

		if (LastCheck + 5 < Now) {
			for (i = 0; i < m_UserCount; i++) {
				if (m_Users[i] && m_Users[i]->ShouldReconnect()) {
					m_Users[i]->ScheduleReconnect();

					break;
				}
			}

			LastCheck = Now;
		}

		for (i = 0; i < m_OtherSocketCount; i++) {
			if (m_OtherSockets[i].Socket != INVALID_SOCKET) {
				if (m_OtherSockets[i].Events->DoTimeout())
					m_OtherSockets[i].Socket = INVALID_SOCKET;
			}
		}

		for (i = 0; i < m_OtherSocketCount; i++) {
			if (m_OtherSockets[i].Socket != INVALID_SOCKET) {
//				if (m_OtherSockets[i].Socket > nfds)
//					nfds = m_OtherSockets[i].Socket;

				FD_SET(m_OtherSockets[i].Socket, &FDRead);

				if (m_OtherSockets[i].Events->HasQueuedData())
					FD_SET(m_OtherSockets[i].Socket, &FDWrite);
			}
		}

		if (SleepInterval <= 0 || !m_Running)
			SleepInterval = 1;

		timeval interval = { SleepInterval, 0 };

		int nfds = 0;
		timeval tv;
		timeval* tvp = &tv;
		fd_set FDError;

		memset(tvp, 0, sizeof(timeval));

		FD_ZERO(&FDError);

		// &FDError was 'NULL'
		adns_beforeselect(g_adns_State, &nfds, &FDRead, &FDWrite, &FDError, &tvp, &interval, NULL);

		Last = time(NULL);

		int ready = select(MAX_SOCKETS, &FDRead, &FDWrite, &FDError, &interval);

		adns_afterselect(g_adns_State, nfds, &FDRead, &FDWrite, &FDError, NULL);

		if (ready > 0) {
			//printf("%d socket(s) ready\n", ready);

			if (FD_ISSET(m_Listener, &FDRead)) {
				sockaddr_in sin_remote;
				socklen_t sin_size = sizeof(sin_remote);

				SOCKET Client = accept(m_Listener, (sockaddr*)&sin_remote, &sin_size);
				HandleConnectingClient(Client, sin_remote);
			}

			for (i = 0; i < m_OtherSocketCount; i++) {
				SOCKET Socket = m_OtherSockets[i].Socket;
				CSocketEvents* Events = m_OtherSockets[i].Events;

				if (Socket != INVALID_SOCKET) {
					if (FD_ISSET(Socket, &FDRead)) {
						if (!Events->Read()) {
							Events->Destroy();
							shutdown(Socket, SD_BOTH); 
							closesocket(Socket);
						}
					}

					if (m_OtherSockets[i].Events && FD_ISSET(Socket, &FDWrite)) {
						Events->Write();
					}
				}
			}
		} else if (ready == -1) {
			//printf("select() failed :/\n");

			fd_set set;

			for (i = 0; i < m_OtherSocketCount; i++) {
				SOCKET Socket = m_OtherSockets[i].Socket;

				if (Socket != INVALID_SOCKET) {
					FD_ZERO(&set);
					FD_SET(Socket, &set);

					timeval zero = { 0, 0 };
					int code = select(MAX_SOCKETS, &set, NULL, NULL, &zero);

					if (code == -1) {
						m_OtherSockets[i].Events->Error();
						m_OtherSockets[i].Events->Destroy();

						shutdown(Socket, SD_BOTH);
						closesocket(Socket);

						m_OtherSockets[i].Socket = INVALID_SOCKET;
					}
				}
			}
		}

		void* context;
		adns_query query;

		for (adns_forallqueries_begin(g_adns_State); (query = adns_forallqueries_next(g_adns_State, &context));) {
			adns_answer* reply = NULL;

			adns_check(g_adns_State, &query, &reply, &context);

			CDnsEvents* Ctx = (CDnsEvents*)context;

			if (reply)
				Ctx->AsyncDnsFinished(&query, reply);
		}
	}
}

void CBouncerCore::HandleConnectingClient(SOCKET Client, sockaddr_in Remote) {
	if (Client > g_last_sock)
		g_last_sock = Client;

	// destruction is controlled by the main loop
	new CClientConnection(Client, Remote);

	Log("Bouncer client connected...");
}

CBouncerUser* CBouncerCore::GetUser(const char* Name) {
	if (!Name)
		return NULL;

	for (int i = 0; i < m_UserCount; i++) {
		if (m_Users[i] && strcmpi(m_Users[i]->GetUsername(), Name) == 0) {
			return m_Users[i];
		}
	}

	return NULL;
}

void CBouncerCore::GlobalNotice(const char* Text, bool AdminOnly) {
	for (int i = 0; i < m_UserCount; i++) {
		if (m_Users[i] && (!AdminOnly || m_Users[i]->IsAdmin()))
			m_Users[i]->Notice(Text);
	}
}

CBouncerUser** CBouncerCore::GetUsers(void) {
	return m_Users;
}

int CBouncerCore::GetUserCount(void) {
	return m_UserCount;
}


void CBouncerCore::SetIdent(const char* Ident) {
	if (m_Ident)
		m_Ident->SetIdent(Ident);
}

const char* CBouncerCore::GetIdent(void) {
	if (m_Ident)
		return m_Ident->GetIdent();
	else
		return NULL;
}

CModule** CBouncerCore::GetModules(void) {
	return m_Modules;
}

int CBouncerCore::GetModuleCount(void) {
	return m_ModuleCount;
}

CModule* CBouncerCore::LoadModule(const char* Filename) {
	CModule* Mod = new CModule(Filename);
	CModule** Modules;

	if (Mod == NULL) {
		LOGERROR("new operator failed. Could not load module %s", Filename);

		return NULL;
	}

	for (int i = 0; i < m_ModuleCount; i++) {
		if (m_Modules[i] && m_Modules[i]->GetHandle() == Mod->GetHandle()) {
			delete Mod;

			return NULL;
		}
	}

	if (Mod->GetModule()) {
		Modules = (CModule**)realloc(m_Modules, sizeof(CModule*) * ++m_ModuleCount);

		if (Modules == NULL) {
			--m_ModuleCount;

			delete Mod;

			LOGERROR("realloc() failed. Could not load module");

			return NULL;
		}

		m_Modules = Modules;
		m_Modules[m_ModuleCount - 1] = Mod;

		Log("Loaded module: %s", Mod->GetFilename());

		Mod->Init(this);

		if (!m_LoadingModules)
			UpdateModuleConfig();

		return Mod;
	} else {
		delete Mod;

		return NULL;
	}
}

bool CBouncerCore::UnloadModule(CModule* Module) {
	for (int i = 0; i < m_ModuleCount; i++) {
		if (m_Modules[i] == Module) {
			Log("Unloaded module: %s", Module->GetFilename());

			delete Module;
			m_Modules[i] = NULL;

			UpdateModuleConfig();

			return true;
		}
	}

	return false;
}

void CBouncerCore::UpdateModuleConfig(void) {
	char* Out;
	int a = 0;

	for (int i = 0; i < m_ModuleCount; i++) {
		if (m_Modules[i]) {
			asprintf(&Out, "system.modules.mod%d", a++);

			if (Out == NULL) {
				LOGERROR("asprintf() failed.");

				Fatal();
			}

			m_Config->WriteString(Out, m_Modules[i]->GetFilename());

			free(Out);
		}
	}

	asprintf(&Out, "system.modules.mod%d", a);

	if (Out == NULL) {
		LOGERROR("asprintf() failed.");

		Fatal();
	}

	m_Config->WriteString(Out, NULL);

	free(Out);
}

void CBouncerCore::RegisterSocket(SOCKET Socket, CSocketEvents* EventInterface) {
	m_OtherSockets = (socket_s*)realloc(m_OtherSockets, sizeof(socket_s) * ++m_OtherSocketCount);

	/* TODO: can we safely recover from this situation? return value maybe? */
	if (m_OtherSockets == NULL) {
		LOGERROR("realloc() failed.");

		Fatal();
	}

	m_OtherSockets[m_OtherSocketCount - 1].Socket = Socket;
	m_OtherSockets[m_OtherSocketCount - 1].Events = EventInterface;
}


void CBouncerCore::UnregisterSocket(SOCKET Socket) {
	for (int i = 0; i < m_OtherSocketCount; i++) {
		if (m_OtherSockets[i].Socket == Socket) {
			m_OtherSockets[i].Events = NULL;
			m_OtherSockets[i].Socket = INVALID_SOCKET;
		}
	}
}

SOCKET CBouncerCore::CreateListener(unsigned short Port, const char* BindIp) {
	return ::CreateListener(Port, BindIp);
}

void CBouncerCore::Log(const char* Format, ...) {
	char *Out;
	int Ret;
	va_list marker;

	va_start(marker, Format);
	Ret = vasprintf(&Out, Format, marker);
	va_end(marker);

	if (Ret == -1) {
		LOGERROR("vasprintf() failed.");

		return;
	}

	m_Log->InternalWriteLine(Out);

	free(Out);
}

/* TODO: should we rely on asprintf/vasprintf here? */
void CBouncerCore::InternalLogError(const char* Format, ...) {
	char *Format2, *Out;
	const char* P = g_ErrorFile;
	va_list marker;

	while (*P++)
		if (*P == '\\')
			g_ErrorFile = P + 1;

	asprintf(&Format2, "Error (in %s:%d): %s", g_ErrorFile, g_ErrorLine, Format);

	if (Format2 == NULL) {
		printf("CBouncerCore::InternalLogError: asprintf() failed.");

		return;
	}

	va_start(marker, Format);
	vasprintf(&Out, Format2, marker);
	va_end(marker);

	free(Format2);

	if (Out == NULL) {
		printf("CBouncerCore::InternalLogError: vasprintf() failed.");

		free(Format2);

		return;
	}

	m_Log->InternalWriteLine(Out);

	free(Out);
}

void CBouncerCore::InternalSetFileAndLine(const char* Filename, unsigned int Line) {
	g_ErrorFile = Filename;
	g_ErrorLine = Line;
}

CBouncerConfig* CBouncerCore::GetConfig(void) {
	return m_Config;
}

CBouncerLog* CBouncerCore::GetLog(void) {
	return m_Log;
}

void CBouncerCore::Shutdown(void) {
	g_Bouncer->GlobalNotice("Shutdown requested.");
	g_Bouncer->Log("Shutdown requested.");

	m_Running = false;
}

CBouncerUser* CBouncerCore::CreateUser(const char* Username, const char* Password) {
	CBouncerUser* U = GetUser(Username);
	CBouncerUser** Users;
	char* Out;

	if (U) {
		if (Password)
			U->SetPassword(Password);

		return U;
	}

	if (!IsValidUsername(Username))
		return NULL;

	Users = (CBouncerUser**)realloc(m_Users, sizeof(CBouncerUser*) * ++m_UserCount);

	if (Users == NULL) {
		LOGERROR("realloc() failed. could not create user");

		return NULL;
	}

	m_Users = Users;
	m_Users[m_UserCount - 1] = new CBouncerUser(Username);

	if (Password)
		m_Users[m_UserCount - 1]->SetPassword(Password);

	asprintf(&Out, "New user created: %s", Username);

	if (Out == NULL) {
		LOGERROR("asprintf() failed.");
	} else {
		Log("%s", Out);
		GlobalNotice(Out, true);

		free(Out);
	}

	UpdateUserConfig();

	for (int i = 0; i < g_Bouncer->GetModuleCount(); i++) {
		CModule* M = g_Bouncer->GetModules()[i];

		if (M) {
			M->UserCreate(Username);
		}
	}

	m_Users[m_UserCount - 1]->LoadEvent();

	return m_Users[m_UserCount - 1];
}

bool CBouncerCore::RemoveUser(const char* Username, bool RemoveConfig) {
	char *Out;

	for (int i = 0; i < m_UserCount; i++) {
		if (m_Users[i] && strcmpi(m_Users[i]->GetUsername(), Username) == 0) {
			for (int a = 0; a < g_Bouncer->GetModuleCount(); a++) {
				CModule* Module = g_Bouncer->GetModules()[a];

				if (Module) {
					Module->UserDelete(Username);
				}
			}

			if (RemoveConfig)
				unlink(m_Users[i]->GetConfig()->GetFilename());

			delete m_Users[i];

			m_Users[i] = NULL;


			asprintf(&Out, "User removed: %s", Username);

			if (Out == NULL) {
				LOGERROR("asprintf() failed.");
			} else {
				m_Log->WriteLine(Out);

				GlobalNotice(Out, true);

				free(Out);
			}

			UpdateUserConfig();

			return true;
		}
	}

	return false;
}

bool CBouncerCore::IsValidUsername(const char* Username) {
	for (unsigned int i = 0; i < strlen(Username); i++) {
		if (!isalnum(Username[i]))
			return false;
	}

	if (strlen(Username) == 0)
		return false;
	else
		return true;
}

void CBouncerCore::UpdateUserConfig(void) {
	char* Out = NULL;

	for (int i = 0; i < m_UserCount; i++) {
		if (m_Users[i]) {
			bool WasNull = false;

			if (Out == NULL)
				WasNull = true;

			Out = (char*)realloc(Out, (Out ? strlen(Out) : 0) + strlen(m_Users[i]->GetUsername()) + 10);

			if (Out == NULL) {
				LOGERROR("realloc() failed. Userlist in sbnc.conf might be out of date.");

				return;
			}

			if (WasNull)
				*Out = '\0';

			if (*Out) {
				strcat(Out, " ");
				strcat(Out, m_Users[i]->GetUsername());
			} else {
				strcpy(Out, m_Users[i]->GetUsername());
			}
		}
	}

	if (m_Config)
		m_Config->WriteString("system.users", Out);

	free(Out);
}

time_t CBouncerCore::GetStartup(void) {
	return m_Startup;
}

bool CBouncerCore::Daemonize(void) {
#ifndef _WIN32
	pid_t pid;
	pid_t sid;
	int fd;

	printf("Daemonizing... ");

	pid = fork();
	if (pid == -1) {
		Log("fork() returned -1 (failure)");

		return false;
	}

	if (pid) {
		FILE* pidFile = fopen("sbnc.pid", "w");

		if (pidFile) {
			fprintf(pidFile, "%d", pid);
			fclose(pidFile);
		}

		printf("DONE\n");
		exit(0);
	}

	fd = open("/dev/null", O_RDWR);
	if (fd) {
		if (fd != 0)
			dup2(fd, 0);
		if (fd != 1)
			dup2(fd, 1);
		if (fd != 2)
			dup2(fd, 2);
		if (fd > 2)
			close(fd);
	}

	sid=setsid();
	if (sid==-1)
		return false;
#endif

	return true;
}

const char* CBouncerCore::MD5(const char* String) {
	MD5_CTX context;
	static char Result[33];
	unsigned char digest[16];
	unsigned int len = strlen(String);

	MD5Init (&context);
	MD5Update (&context, (unsigned char*)String, len);
	MD5Final (digest, &context);

#undef sprintf

	for (int i = 0; i < 16; i++) {
		sprintf(Result + i * 2, "%02x", digest[i]);
	}

	return Result;
}


int CBouncerCore::GetArgC(void) {
	return m_argc;
}

char** CBouncerCore::GetArgV(void) {
	return m_argv;
}

CConnection* CBouncerCore::WrapSocket(SOCKET Socket) {
	CConnection* Wrapper = new CConnection(Socket);

	Wrapper->m_Wrapper = true;

	return Wrapper;
}

void CBouncerCore::DeleteWrapper(CConnection* Wrapper) {
	delete Wrapper;
}

void CBouncerCore::Free(void* Pointer) {
	free(Pointer);
}

void* CBouncerCore::Alloc(size_t Size) {
	return malloc(Size);
}

bool CBouncerCore::IsRegisteredSocket(CSocketEvents* Events) {
	for (int i = 0; i < m_OtherSocketCount; i++) {
		if (m_OtherSockets[i].Events == Events)
			return true;
	}

	return false;
}

SOCKET CBouncerCore::SocketAndConnect(const char* Host, unsigned short Port, const char* BindIp) {
	return ::SocketAndConnect(Host, Port, BindIp);
}

socket_t* CBouncerCore::GetSocketByClass(const char* Class, int Index) {
	int a = 0;

	for (int i = 0; i < m_OtherSocketCount; i++) {
		socket_t Socket = m_OtherSockets[i];

		if (Socket.Socket == INVALID_SOCKET)
			continue;

		if (strcmp(Socket.Events->ClassName(), Class) == 0)
			a++;

		if (a - 1 == Index)
			return &m_OtherSockets[i];
	}

	return NULL;
}

CTimer* CBouncerCore::CreateTimer(unsigned int Interval, bool Repeat, timerproc Function, void* Cookie) {
	return new CTimer(Interval, Repeat, Function, Cookie);
}

void CBouncerCore::RegisterTimer(CTimer* Timer) {
	timerchain_t* last = &m_TimerChain;

	while (last->next)
		last = last->next;

	last->next = (timerchain_t*)malloc(sizeof(timerchain_t));

	if (last->next == NULL) {
		LOGERROR("malloc() failed. Timer could not be registered.");

		return;
	}

	last->ptr = Timer;

	last->next->next = NULL;
	last->next->ptr = NULL;
}

void CBouncerCore::UnregisterTimer(CTimer* Timer) {
	timerchain_t* current = &m_TimerChain;

	if (current->ptr == Timer && current->next) {
		current->ptr = current->next->ptr;
		current->next = current->next->next;

		return;
	}

	while (current) {
		if (current->next && current->next->ptr == Timer) {
			timerchain_t* old = current->next;
			current->next = current->next->next;

			free(old);
		}

		current = current->next;
	}
}

int CBouncerCore::GetTimerStats(void) {
	return g_TimerStats;
}

bool CBouncerCore::Match(const char* Pattern, const char* String) {
	return (match(Pattern, String) == 0);
}

int CBouncerCore::GetSendQSize(void) {
	if (m_SendQSizeCache != -1)
		return m_SendQSizeCache;

	int Size = m_Config->ReadInteger("system.sendq");

	if (Size == 0)
		return DEFAULT_SENDQ;
	else
		return Size;
}

void CBouncerCore::SetSendQSize(int NewSize) {
	m_Config->WriteInteger("system.sendq", NewSize);
	m_SendQSizeCache = NewSize;
}

const char* CBouncerCore::GetMotd(void) {
	return m_Config->ReadString("system.motd");
}

void CBouncerCore::SetMotd(const char* Motd) {
	m_Config->WriteString("system.motd", Motd);
}

void CBouncerCore::Fatal(void) {
	Log("Fatal error occured. Please send this log to gb@shroudbnc.org for further analysis.");

	exit(1);
}
