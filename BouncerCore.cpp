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

#include <openssl/err.h>

extern bool g_Debug;

const char* g_ErrorFile;
unsigned int g_ErrorLine;

#ifdef USESSL
int SSLVerifyCertificate(int preverify_ok, X509_STORE_CTX *x509ctx);
int g_SSLCustomIndex;
#endif

SOCKET g_last_sock = 0;
time_t g_LastReconnect = 0;
extern int g_TimerStats;

void AcceptHelper(SOCKET Client, sockaddr_in PeerAddress, bool SSL) {
	unsigned long lTrue = 1;

	if (Client > g_last_sock)
		g_last_sock = Client;

	ioctlsocket(Client, FIONBIO, &lTrue);

	// destruction is controlled by the main loop
	new CClientConnection(Client, PeerAddress, SSL);

	g_Bouncer->Log("Bouncer client connected...");
}

IMPL_SOCKETLISTENER(CClientListener, CBouncerCore) {
public:
	CClientListener(unsigned int Port, const char *BindIp = NULL) : CListenerBase<CBouncerCore>(Port, BindIp, NULL) { }

	virtual void Accept(SOCKET Client, sockaddr_in PeerAddress) {
		AcceptHelper(Client, PeerAddress, false);
	}
};

IMPL_SOCKETLISTENER(CSSLClientListener, CBouncerCore) {
public:
	CSSLClientListener(unsigned int Port, const char *BindIp = NULL) : CListenerBase<CBouncerCore>(Port, BindIp, NULL) { }

	virtual void Accept(SOCKET Client, sockaddr_in PeerAddress) {
		AcceptHelper(Client, PeerAddress, true);
	}
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

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

	g_Bouncer = this;

	m_Config = Config;

	m_Args.SetList(argv, argc);

	m_Ident = new CIdentSupport();

	const char *Users = Config->ReadString("system.users");
	CBouncerUser *User;

	if (Users) {
		const char* Args;
		int Count;

		Args = ArgTokenize(Users);

		if (Args == NULL) {
			LOGERROR("ArgTokenize() failed.");

			Fatal();
		}

		Count = ArgCount(Args);

		for (i = 0; i < Count; i++) {
			User = new CBouncerUser(ArgGet(Args, i + 1));

			if (User == NULL) {
				LOGERROR("Could not create user object");

				Fatal();
			}

			m_Users.Insert(User);
		}

		for (i = 0; i < Count; i++)
			m_Users[i]->LoadEvent();

		ArgFree(Args);
	} else {
		Log("No users were found in the config file.");

		Fatal();
	}

	m_Listener = NULL;
	m_SSLListener = NULL;

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
	if (m_Listener != NULL)
		delete m_Listener;

	if (m_SSLListener != NULL)
		delete m_SSLListener;

	for (int a = 0; a < m_Modules.Count(); a++) {
		if (m_Modules[a])
			delete m_Modules[a];
	}

	for (int i = 0; i < m_Users.Count(); i++) {
		if (m_Users[i])
			delete m_Users[i];
	}

	for (int c = 0; c < m_OtherSockets.Count(); c++) {
		if (m_OtherSockets[c].Socket != INVALID_SOCKET) {
			m_OtherSockets[c].Events->Destroy();
		}
	}

	for (int d = 0; d < m_Timers.Count(); d++) {
		if (m_Timers[d])
			delete m_Timers[d];
	}

	delete m_Log;
	delete m_Ident;
}

void CBouncerCore::StartMainLoop(void) {
	bool b_DontDetach = false;

	puts("shroudBNC" BNCVERSION " - an object-oriented IRC bouncer");

	int argc = m_Args.Count();
	char** argv = m_Args.GetList();

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
#ifdef USESSL
	int SSLPort = m_Config->ReadInteger("system.sslport");

	if (Port == 0 && SSLPort == 0)
#else
	if (Port == 0)
#endif
		Port = 9000;

	const char* BindIp = g_Bouncer->GetConfig()->ReadString("system.ip");

	if (Port != 0)
		m_Listener = new CClientListener(Port, BindIp);
	else
		m_Listener = NULL;

#ifdef USESSL
	if (SSLPort != 0)
		m_SSLListener = new CSSLClientListener(SSLPort, BindIp);
	else
		m_SSLListener = NULL;

	SSL_library_init();
	SSL_load_error_strings();

	SSL_METHOD* SSLMethod = SSLv23_method();
	m_SSLContext = SSL_CTX_new(SSLMethod);
	m_SSLClientContext = SSL_CTX_new(SSLMethod);

	SSL_CTX_set_mode(m_SSLContext, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
	SSL_CTX_set_mode(m_SSLClientContext, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

	g_SSLCustomIndex = SSL_get_ex_new_index(0, (void *)"CConnection*", NULL, NULL, NULL);

	if (!SSL_CTX_use_PrivateKey_file(m_SSLContext, "sbnc.key", SSL_FILETYPE_PEM)) {
		Log("Could not load private key (sbnc.key."); ERR_print_errors_fp(stdout);
		return;
	}

	if (!SSL_CTX_use_certificate_chain_file(m_SSLContext, "sbnc.crt")) {
		Log("Could not load public key (sbnc.crt)."); ERR_print_errors_fp(stdout);
		return;
	}

/* 	if (!SSL_CTX_load_verify_locations(m_SSLContext, "ca.crt", NULL)) {
		Log("Could not load CA certificate (ca.crt).");
		return;
	}

	STACK_OF(X509)* ClientCA = SSL_load_client_CA_file("ca.crt");

	if (ClientCA == NULL) {
		Log("Could not load CA certificate (ca.crt).");
		return;
	}*/

//	SSL_CTX_set_client_CA_list(m_SSLContext, ClientCA);

	SSL_CTX_set_verify(m_SSLContext, SSL_VERIFY_PEER, SSLVerifyCertificate);
	SSL_CTX_set_verify(m_SSLClientContext, SSL_VERIFY_PEER, SSLVerifyCertificate);
#endif

	if (Port != 0 && m_Listener != NULL && m_Listener->IsValid())
		Log("Created main listener.");
	else if (Port != 0) {
		Log("Could not create listener port");
		return;
	}

#ifdef USESSL
	if (SSLPort != 0 && m_SSLListener != NULL && m_SSLListener->IsValid())
		Log("Created ssl listener.");
	else if (SSLPort != 0) {
		Log("Could not create ssl listener port");
		return;
	}
#endif

	fd_set FDRead, FDWrite;

	Log("Starting main loop.");

	if (!b_DontDetach)
		Daemonize();

	m_Running = true;
	int m_ShutdownLoop = 5;

	time_t Last = 0;
	time_t LastCheck = 0;

	while (m_Running || --m_ShutdownLoop) {
		time_t Now = time(NULL);
		time_t Best = 0;
		time_t SleepInterval = 0;

		for (int c = m_Timers.Count() - 1; c >= 0; c--) {
			time_t NextCall = m_Timers[c]->GetNextCall();

			if (Now >= NextCall && Now > Last) {
				if (Now - 5 > NextCall)
					Log("Timer drift for timer %p: %d seconds", m_Timers[c], Now - NextCall);

				m_Timers[c]->Call(Now);
				Best = Now + 1;
			} else if (Best == 0 || NextCall < Best) {
				Best = NextCall;
			}
		}

		if (Best)
			SleepInterval = Best - Now;

		FD_ZERO(&FDRead);
		FD_ZERO(&FDWrite);

		int i;

		for (i = 0; i < m_Users.Count(); i++) {
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
			for (i = 0; i < m_Users.Count(); i++) {
				if (m_Users[i] && m_Users[i]->ShouldReconnect()) {
					m_Users[i]->ScheduleReconnect();

					break;
				}
			}

			LastCheck = Now;
		}

		for (i = m_OtherSockets.Count() - 1; i >= 0; i--) {
			if (m_OtherSockets[i].Socket != INVALID_SOCKET) {
				if (m_OtherSockets[i].Events->DoTimeout())
					continue;
				else if (m_OtherSockets[i].Events->ShouldDestroy())
					m_OtherSockets[i].Events->Destroy();
			}
		}

		for (i = 0; i < m_OtherSockets.Count(); i++) {
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

			for (i = m_OtherSockets.Count() - 1; i >= 0; i--) {
				SOCKET Socket = m_OtherSockets[i].Socket;
				CSocketEvents* Events = m_OtherSockets[i].Events;

				if (Socket != INVALID_SOCKET) {
					if (FD_ISSET(Socket, &FDRead)) {
						if (!Events->Read()) {
							Events->Destroy();

							continue;
						}
					}

					if (Events && FD_ISSET(Socket, &FDWrite)) {
						Events->Write();
					}
				}
			}
		} else if (ready == -1) {
			//printf("select() failed :/\n");

			fd_set set;

			for (i = m_OtherSockets.Count() - 1; i >= 0; i--) {
				SOCKET Socket = m_OtherSockets[i].Socket;

				if (Socket != INVALID_SOCKET) {
					FD_ZERO(&set);
					FD_SET(Socket, &set);

					timeval zero = { 0, 0 };
					int code = select(MAX_SOCKETS, &set, NULL, NULL, &zero);

					if (code == -1) {
						m_OtherSockets[i].Events->Error();
						m_OtherSockets[i].Events->Destroy();
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

			if (reply) {
				Ctx->AsyncDnsFinished(&query, reply);
				Ctx->Destroy();
			}
		}
	}

#ifdef USESSL
	SSL_CTX_free(m_SSLContext);
	SSL_CTX_free(m_SSLClientContext);
#endif
}

void CBouncerCore::HandleConnectingClient(SOCKET Client, sockaddr_in Remote, bool SSL) {
	if (Client > g_last_sock)
		g_last_sock = Client;

	unsigned long lTrue = 1;
	ioctlsocket(Client, FIONBIO, &lTrue);

	// destruction is controlled by the main loop
	new CClientConnection(Client, Remote, SSL);

	Log("Bouncer client connected...");
}

CBouncerUser* CBouncerCore::GetUser(const char* Name) {
	if (!Name)
		return NULL;

	for (int i = 0; i < m_Users.Count(); i++) {
		if (m_Users[i] && strcmpi(m_Users[i]->GetUsername(), Name) == 0) {
			return m_Users[i];
		}
	}

	return NULL;
}

void CBouncerCore::GlobalNotice(const char* Text, bool AdminOnly) {
	for (int i = 0; i < m_Users.Count(); i++) {
		if (m_Users[i] && (!AdminOnly || m_Users[i]->IsAdmin()))
			m_Users[i]->Notice(Text);
	}
}

CBouncerUser** CBouncerCore::GetUsers(void) {
	return m_Users.GetList();
}

int CBouncerCore::GetUserCount(void) {
	return m_Users.Count();
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
	return m_Modules.GetList();
}

int CBouncerCore::GetModuleCount(void) {
	return m_Modules.Count();
}

CModule* CBouncerCore::LoadModule(const char* Filename) {
	CModule* Mod = new CModule(Filename);

	if (Mod == NULL) {
		LOGERROR("new operator failed. Could not load module %s", Filename);

		return NULL;
	}

	for (int i = 0; i < m_Modules.Count(); i++) {
		if (m_Modules[i] && m_Modules[i]->GetHandle() == Mod->GetHandle()) {
			delete Mod;

			return NULL;
		}
	}

	if (Mod->GetModule()) {
		if (!m_Modules.Insert(Mod)) {
			delete Mod;

			LOGERROR("realloc() failed. Could not load module");

			return NULL;
		}

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
	if (m_Modules.Remove(Module)) {
		Log("Unloaded module: %s", Module->GetFilename());

		delete Module;

		UpdateModuleConfig();

		return true;
	} else
		return false;
}

void CBouncerCore::UpdateModuleConfig(void) {
	char* Out;
	int a = 0;

	for (int i = 0; i < m_Modules.Count(); i++) {
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
	socket_s s = { Socket, EventInterface };

	UnregisterSocket(Socket);
	
	/* TODO: can we safely recover from this situation? return value maybe? */
	if (!m_OtherSockets.Insert(s)) {
		LOGERROR("realloc() failed.");

		Fatal();
	}
}


void CBouncerCore::UnregisterSocket(SOCKET Socket) {
	for (int i = 0; i < m_OtherSockets.Count(); i++) {
		if (m_OtherSockets[i].Socket == Socket) {
			m_OtherSockets.Remove(i);

			return;
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
	CBouncerUser* User = GetUser(Username);
	char* Out;

	if (User) {
		if (Password)
			User->SetPassword(Password);

		return User;
	}

	if (!IsValidUsername(Username))
		return NULL;

	User = new CBouncerUser(Username);

	if (!m_Users.Insert(User)) {
		delete User;

		return NULL;
	}

	if (Password)
		User->SetPassword(Password);

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
		CModule* Module = g_Bouncer->GetModules()[i];

		if (Module) {
			Module->UserCreate(Username);
		}
	}

	User->LoadEvent();

	return User;
}

bool CBouncerCore::RemoveUser(const char* Username, bool RemoveConfig) {
	char *Out;

	for (int i = 0; i < m_Users.Count(); i++) {
		if (m_Users[i] && strcmpi(m_Users[i]->GetUsername(), Username) == 0) {
			for (int a = 0; a < g_Bouncer->GetModuleCount(); a++) {
				CModule* Module = g_Bouncer->GetModules()[a];

				if (Module) {
					Module->UserDelete(Username);
				}
			}

			if (RemoveConfig)
				unlink(m_Users[i]->GetConfig()->GetFilename());

			m_Users.Remove(i);

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

	for (int i = 0; i < m_Users.Count(); i++) {
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
	return UtilMd5(String);
}


int CBouncerCore::GetArgC(void) {
	return m_Args.Count();
}

char** CBouncerCore::GetArgV(void) {
	return m_Args.GetList();
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
	for (int i = 0; i < m_OtherSockets.Count(); i++) {
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

	for (int i = 0; i < m_OtherSockets.Count(); i++) {
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

CTimer* CBouncerCore::CreateTimer(unsigned int Interval, bool Repeat, TimerProc Function, void* Cookie) {
	return new CTimer(Interval, Repeat, Function, Cookie);
}

void CBouncerCore::RegisterTimer(CTimer* Timer) {
	m_Timers.Insert(Timer);
}

void CBouncerCore::UnregisterTimer(CTimer* Timer) {
	m_Timers.Remove(Timer);
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

SSL_CTX* CBouncerCore::GetSSLContext(void) {
	return m_SSLContext;
}

SSL_CTX* CBouncerCore::GetSSLClientContext(void) {
	return m_SSLClientContext;
}

int CBouncerCore::GetSSLCustomIndex(void) {
#ifdef USESSL
	return g_SSLCustomIndex;
#else
	return 0;
#endif
}

#ifdef USESSL
int SSLVerifyCertificate(int preverify_ok, X509_STORE_CTX *x509ctx) {
	SSL* ssl = (SSL*)X509_STORE_CTX_get_ex_data(x509ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
	CConnection* Ptr = (CConnection*)SSL_get_ex_data(ssl, g_SSLCustomIndex);

	if (Ptr != NULL)
		return Ptr->SSLVerify(preverify_ok, x509ctx);
	else
		return 0;
}
#endif
