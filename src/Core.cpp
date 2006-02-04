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
#include "sbnc.h"

#ifdef USESSL
#include <openssl/err.h>
#endif

#ifndef _WIN32
	#include <sys/stat.h>

	#define mkdir(X) mkdir(X, 0700)
#else
	#include <direct.h>
	#define mkdir _mkdir
#endif

extern bool g_Debug;
extern bool g_Freeze;
extern loaderparams_s *g_LoaderParameters;

const char* g_ErrorFile;
unsigned int g_ErrorLine;

CHashtable<command_t, false, 16> *g_Commands = NULL;

#ifdef USESSL
int SSLVerifyCertificate(int preverify_ok, X509_STORE_CTX *x509ctx);
int g_SSLCustomIndex;
#endif

time_t g_LastReconnect = 0;
#ifdef _DEBUG
extern int g_TimerStats;
#endif

void AcceptHelper(SOCKET Client, sockaddr_in PeerAddress, bool SSL) {
	unsigned long lTrue = 1;

	ioctlsocket(Client, FIONBIO, &lTrue);

	// destruction is controlled by the main loop
	new CClientConnection(Client, PeerAddress, SSL);

	g_Bouncer->Log("Bouncer client connected...");
}

IMPL_SOCKETLISTENER(CClientListener, CCore) {
public:
	CClientListener(unsigned int Port, const char *BindIp = NULL) : CListenerBase<CCore>(Port, BindIp, NULL) { }
	CClientListener(SOCKET Listener, CCore *EventClass = NULL) : CListenerBase<CCore>(Listener, EventClass) { }

	virtual void Accept(SOCKET Client, sockaddr_in PeerAddress) {
		AcceptHelper(Client, PeerAddress, false);
	}
};

IMPL_SOCKETLISTENER(CSSLClientListener, CCore) {
public:
	CSSLClientListener(unsigned int Port, const char *BindIp = NULL) : CListenerBase<CCore>(Port, BindIp, NULL) { }
	CSSLClientListener(SOCKET Listener, CCore *EventClass = NULL) : CListenerBase<CCore>(Listener, EventClass) { }

	virtual void Accept(SOCKET Client, sockaddr_in PeerAddress) {
		AcceptHelper(Client, PeerAddress, true);
	}
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCore::CCore(CConfig* Config, int argc, char** argv) {
	int i;

	m_Running = false;

	m_Log = new CLog("sbnc.log");

	if (m_Log == NULL) {
		printf("Log system could not be initialized. Shutting down.");

		exit(1);
	}

	m_Log->Clear();
	m_Log->WriteLine("Log system initialized.");

	g_Bouncer = this;

	m_Config = Config;

	m_Args.SetList(argv, argc);

	m_Ident = new CIdentSupport();

	const char *Users;
	CUser *User;

	while ((Users = Config->ReadString("system.users")) == NULL) {
		if (MakeConfig() == false) {
			LOGERROR("Configuration file could not be created.");

			Fatal();
		}

		Config->Reload();
	}

	const char* Args;
	int Count;

	Args = ArgTokenize(Users);

	if (Args == NULL) {
		LOGERROR("ArgTokenize() failed.");

		Fatal();
	}

	Count = ArgCount(Args);

	for (i = 0; i < Count; i++) {
		const char *Name = ArgGet(Args, i + 1);
		User = new CUser(Name);

		if (User == NULL) {
			LOGERROR("Could not create user object");

			Fatal();
		}

		m_Users.Add(Name, User);
	}

	i = 0;
	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		User->Value->LoadEvent();
	}

	ArgFree(Args);

	m_Listener = NULL;
	m_SSLListener = NULL;

	m_Startup = time(NULL);

	m_SendQSizeCache = -1;

#ifdef _DEBUG
	if (Config->ReadInteger("system.debug"))
		g_Debug = true;
#endif
}

CCore::~CCore() {
	int a, c, d, i;
/*	if (m_Listener != NULL)
		delete m_Listener;

	if (m_SSLListener != NULL)
		delete m_SSLListener;*/

	for (a = m_Modules.GetLength() - 1; a >= 0; a--) {
		if (m_Modules[a])
			delete m_Modules[a];

		m_Modules.Remove(a);
	}

	for (c = m_OtherSockets.GetLength() - 1; c >= 0; c--) {
		if (m_OtherSockets[c].Socket != INVALID_SOCKET) {
			m_OtherSockets[c].Events->Destroy();
		}
	}

	i = 0;
	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		delete User->Value;
	}

	for (d = m_Timers.GetLength() - 1; d >= 0 ; d--) {
		if (m_Timers[d])
			delete m_Timers[d];
	}

	delete m_Log;
	delete m_Ident;

	g_Bouncer = NULL;
}

void CCore::StartMainLoop(void) {
	unsigned int i;
	bool b_DontDetach = false;

	puts("shroudBNC" BNCVERSION " - an object-oriented IRC bouncer");

	int argc = m_Args.GetLength();
	char** argv = m_Args.GetList();

	for (int a = 1; a < argc; a++) {
		if (strcmp(argv[a], "-n") == 0 || strcmp(argv[a], "/n") == 0)
			b_DontDetach = true;
		if (strcmp(argv[a], "--help") == 0 || strcmp(argv[a], "/?") == 0) {
			puts("");
			printf("Syntax: %s [OPTION]", argv[0]);
			puts("");
			puts("Options:");
#ifndef _WIN32
			puts("\t-n\tdon't detach");
			puts("\t--help\tdisplay this help and exit");
#else
			puts("\t/n\tdon't detach");
			puts("\t/?\tdisplay this help and exit");
#endif

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

	if (m_Listener == NULL) {
		if (Port != 0)
			m_Listener = new CClientListener(Port, BindIp);
		else
			m_Listener = NULL;
	}

#ifdef USESSL
	if (m_SSLListener == NULL) {
		if (SSLPort != 0)
			m_SSLListener = new CSSLClientListener(SSLPort, BindIp);
		else
			m_SSLListener = NULL;
	}

	SSL_library_init();
	SSL_load_error_strings();

	SSL_METHOD* SSLMethod = SSLv23_method();
	m_SSLContext = SSL_CTX_new(SSLMethod);
	m_SSLClientContext = SSL_CTX_new(SSLMethod);

	SSL_CTX_set_mode(m_SSLContext, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
	SSL_CTX_set_mode(m_SSLClientContext, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

	g_SSLCustomIndex = SSL_get_ex_new_index(0, (void *)"CConnection*", NULL, NULL, NULL);

	if (!SSL_CTX_use_PrivateKey_file(m_SSLContext, "sbnc.key", SSL_FILETYPE_PEM)) {
		if (SSLPort != 0) {
			Log("Could not load private key (sbnc.key)."); ERR_print_errors_fp(stdout);
			return;
		} else {
			SSL_CTX_free(m_SSLContext);
			m_SSLContext = NULL;
		}
	} else {
		SSL_CTX_set_verify(m_SSLContext, SSL_VERIFY_PEER, SSLVerifyCertificate);
	}

	if (!SSL_CTX_use_certificate_chain_file(m_SSLContext, "sbnc.crt")) {
		if (SSLPort != 0) {
			Log("Could not load public key (sbnc.crt)."); ERR_print_errors_fp(stdout);
			return;
		} else {
			SSL_CTX_free(m_SSLContext);
			m_SSLContext = NULL;
		}
	} else {
		SSL_CTX_set_verify(m_SSLClientContext, SSL_VERIFY_PEER, SSLVerifyCertificate);
	}

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

	WritePidFile();

	/* Note: We need to load the modules after using fork() as otherwise tcl cannot be cleanly unloaded */
	m_LoadingModules = true;

	char *Out;

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
			LoadModule(File, NULL);
		else
			break;
	}

	m_LoadingModules = false;

	m_Running = true;
	int m_ShutdownLoop = 5;

	if (g_LoaderParameters->SigEnable)
		g_LoaderParameters->SigEnable();

	time_t Last = 0;
	time_t LastCheck = 0;

	while ((m_Running || --m_ShutdownLoop) && !g_Freeze) {
		time_t Now = time(NULL);
		time_t Best = 0;
		time_t SleepInterval = 0;

		for (int c = m_Timers.GetLength() - 1; c >= 0; c--) {
			time_t NextCall = m_Timers[c]->GetNextCall();

			if (Now >= NextCall) {
				if (Now - 5 > NextCall) {
					Log("Timer drift for timer %p: %d seconds", m_Timers[c], Now - NextCall);
				}

				m_Timers[c]->Call(Now);
			}
		}

		Best = time(NULL) + 60;

		for (int c = m_Timers.GetLength() - 1; c >= 0; c--) {
			time_t NextCall = m_Timers[c]->GetNextCall();

			if (NextCall < Best) {
				Best = NextCall;
			}
		}

		if (Best != 0) {
			SleepInterval = Best - Now;
		}

		FD_ZERO(&FDRead);
		FD_ZERO(&FDWrite);

		i = 0;
		while (hash_t<CUser *> *UserHash = m_Users.Iterate(i++)) {
			CIRCConnection* IRC;

			if (UserHash->Value) {
				if ((IRC = UserHash->Value->GetIRCConnection()) != NULL) {
					if (m_Running == false && IRC->IsLocked() == false) {
						Log("Closing connection for %s", UserHash->Name);
						IRC->Kill("Shutting down.");

						UserHash->Value->SetIRCConnection(NULL);
					}

					if (IRC->ShouldDestroy())
						IRC->Destroy();
				}

				if (LastCheck + 5 < Now && UserHash->Value->ShouldReconnect()) {
					UserHash->Value->ScheduleReconnect();

					LastCheck = Now;
				}
			}
		}

		for (int a = m_OtherSockets.GetLength() - 1; a >= 0; a--) {
			if (m_OtherSockets[a].Socket != INVALID_SOCKET) {
				if (m_OtherSockets[a].Events->DoTimeout())
					continue;
				else if (m_OtherSockets[a].Events->ShouldDestroy())
					m_OtherSockets[a].Events->Destroy();
			}
		}

		for (i = 0; i < m_OtherSockets.GetLength(); i++) {
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

		if (m_Running == false && SleepInterval > 5)
			interval.tv_sec = 5;

		// &FDError was 'NULL'
		adns_beforeselect(g_adns_State, &nfds, &FDRead, &FDWrite, &FDError, &tvp, &interval, NULL);

		Last = time(NULL);

		int ready = select(FD_SETSIZE - 1, &FDRead, &FDWrite, &FDError, &interval);

		adns_afterselect(g_adns_State, nfds, &FDRead, &FDWrite, &FDError, NULL);

#ifdef _DEBUG
		//printf("slept %d seconds\n", SleepInterval - interval.tv_sec);
#endif

		if (ready > 0) {
			//printf("%d socket(s) ready\n", ready);

			for (int a = m_OtherSockets.GetLength() - 1; a >= 0; a--) {
				SOCKET Socket = m_OtherSockets[a].Socket;
				CSocketEvents* Events = m_OtherSockets[a].Events;

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

			for (int a = m_OtherSockets.GetLength() - 1; a >= 0; a--) {
				SOCKET Socket = m_OtherSockets[a].Socket;

				if (Socket != INVALID_SOCKET) {
					FD_ZERO(&set);
					FD_SET(Socket, &set);

					timeval zero = { 0, 0 };
					int code = select(FD_SETSIZE - 1, &set, NULL, NULL, &zero);

					if (code == -1) {
						m_OtherSockets[a].Events->Error();
						m_OtherSockets[a].Events->Destroy();
					}
				}
			}
		}

		CDnsEvents *Ctx;
		adns_query query;

		adns_forallqueries_begin(g_adns_State);

		while ((query = adns_forallqueries_next(g_adns_State, (void **)&Ctx)) != NULL) {
			adns_answer* reply = NULL;

			int retval = adns_check(g_adns_State, &query, &reply, (void **)&Ctx);

			switch (retval) {
				case 0:
					Ctx->AsyncDnsFinished(&query, reply);
					Ctx->Destroy();

					 break;
				case EAGAIN:
					break;
				default:
					Ctx->AsyncDnsFinished(&query, NULL);
					Ctx->Destroy();

					break;
			}
		}
	}

#ifdef USESSL
	SSL_CTX_free(m_SSLContext);
	SSL_CTX_free(m_SSLClientContext);
#endif
}

void CCore::HandleConnectingClient(SOCKET Client, sockaddr_in Remote, bool SSL) {
	unsigned long lTrue = 1;
	ioctlsocket(Client, FIONBIO, &lTrue);

	// destruction is controlled by the main loop
	new CClientConnection(Client, Remote, SSL);

	Log("Bouncer client connected...");
}

CUser* CCore::GetUser(const char* Name) {
	if (!Name)
		return NULL;

	return m_Users.Get(Name);
}

void CCore::GlobalNotice(const char* Text, bool AdminOnly) {
	int i = 0;
	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		if (!AdminOnly || User->Value->IsAdmin())
			User->Value->Notice(Text);
	}
}

CHashtable<CUser *, false, 64> *CCore::GetUsers(void) {
	return &m_Users;
}

int CCore::GetUserCount(void) {
	return m_Users.GetLength();
}

void CCore::SetIdent(const char* Ident) {
	if (m_Ident)
		m_Ident->SetIdent(Ident);
}

const char* CCore::GetIdent(void) {
	if (m_Ident)
		return m_Ident->GetIdent();
	else
		return NULL;
}

CVector<CModule *> *CCore::GetModules(void) {
	return &m_Modules;
}

CModule* CCore::LoadModule(const char* Filename, const char **Error) {
	CModule* Module = new CModule(Filename);

	if (Module == NULL) {
		LOGERROR("new operator failed. Could not load module %s", Filename);

		if (Error)
			*Error = "new operator failed.";

		return NULL;
	}

	for (unsigned int i = 0; i < m_Modules.GetLength(); i++) {
		if (m_Modules[i]->GetHandle() == Module->GetHandle()) {
			delete Module;

			if (Error)
				*Error = "This module is already loaded.";

			return NULL;
		}
	}

	if (Module->GetError() == NULL) {
		if (!m_Modules.Insert(Module)) {
			delete Module;

			LOGERROR("realloc() failed. Could not load module");

			if (Error)
				*Error = "realloc() failed.";

			return NULL;
		}

		Log("Loaded module: %s", Module->GetFilename());

		Module->Init(this);

		if (!m_LoadingModules)
			UpdateModuleConfig();

		return Module;
	} else {
		static char *ErrorMessage = NULL;

		free(ErrorMessage);

		if (Error) {
			ErrorMessage = strdup(Module->GetError());
			*Error = ErrorMessage;
		}

		Log("Module %s could not be loaded: %s", Filename, Module->GetError());

		delete Module;

		return NULL;
	}
}

bool CCore::UnloadModule(CModule* Module) {
	if (m_Modules.Remove(Module)) {
		Log("Unloaded module: %s", Module->GetFilename());

		delete Module;

		UpdateModuleConfig();

		return true;
	} else
		return false;
}

void CCore::UpdateModuleConfig(void) {
	char* Out;
	int a = 0;

	for (unsigned int i = 0; i < m_Modules.GetLength(); i++) {
		asprintf(&Out, "system.modules.mod%d", a++);

		if (Out == NULL) {
			LOGERROR("asprintf() failed.");

			Fatal();
		}

		m_Config->WriteString(Out, m_Modules[i]->GetFilename());

		free(Out);
	}

	asprintf(&Out, "system.modules.mod%d", a);

	if (Out == NULL) {
		LOGERROR("asprintf() failed.");

		Fatal();
	}

	m_Config->WriteString(Out, NULL);

	free(Out);
}

void CCore::RegisterSocket(SOCKET Socket, CSocketEvents* EventInterface) {
	socket_s s = { Socket, EventInterface };

	UnregisterSocket(Socket);
	
	/* TODO: can we safely recover from this situation? return value maybe? */
	if (!m_OtherSockets.Insert(s)) {
		LOGERROR("realloc() failed.");

		Fatal();
	}
}


void CCore::UnregisterSocket(SOCKET Socket) {
	for (unsigned int i = 0; i < m_OtherSockets.GetLength(); i++) {
		if (m_OtherSockets[i].Socket == Socket) {
			m_OtherSockets.Remove(i);

			return;
		}
	}
}

SOCKET CCore::CreateListener(unsigned short Port, const char* BindIp) {
	return ::CreateListener(Port, BindIp);
}

void CCore::Log(const char* Format, ...) {
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

	m_Log->WriteLine("%s", Out);

	free(Out);
}

/* TODO: should we rely on asprintf/vasprintf here? */
void CCore::InternalLogError(const char* Format, ...) {
	char *Format2, *Out;
	const char* P = g_ErrorFile;
	va_list marker;

	while (*P++)
		if (*P == '\\')
			g_ErrorFile = P + 1;

	asprintf(&Format2, "Error (in %s:%d): %s", g_ErrorFile, g_ErrorLine, Format);

	if (Format2 == NULL) {
		printf("CCore::InternalLogError: asprintf() failed.");

		return;
	}

	va_start(marker, Format);
	vasprintf(&Out, Format2, marker);
	va_end(marker);

	free(Format2);

	if (Out == NULL) {
		printf("CCore::InternalLogError: vasprintf() failed.");

		free(Format2);

		return;
	}

	m_Log->WriteLine("%s", Out);

	free(Out);
}

void CCore::InternalSetFileAndLine(const char* Filename, unsigned int Line) {
	g_ErrorFile = Filename;
	g_ErrorLine = Line;
}

CConfig* CCore::GetConfig(void) {
	return m_Config;
}

CLog* CCore::GetLog(void) {
	return m_Log;
}

void CCore::Shutdown(void) {
	g_Bouncer->GlobalNotice("Shutdown requested.");
	g_Bouncer->Log("Shutdown requested.");

	m_Running = false;
}

CUser* CCore::CreateUser(const char* Username, const char* Password) {
	CUser* User = GetUser(Username);
	char* Out;

	if (User) {
		if (Password)
			User->SetPassword(Password);

		return User;
	}

	if (!IsValidUsername(Username))
		return NULL;

	User = new CUser(Username);

	if (!m_Users.Add(Username, User)) {
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

	for (unsigned int i = 0; i < m_Modules.GetLength(); i++) {
		CModule* Module = m_Modules[i];

		if (Module) {
			Module->UserCreate(Username);
		}
	}

	User->LoadEvent();

	return User;
}

bool CCore::RemoveUser(const char* Username, bool RemoveConfig) {
	char *Out;

	CUser *User = GetUser(Username);

	if (User == NULL)
		return false;

	for (unsigned int i = 0; i < m_Modules.GetLength(); i++) {
		CModule *Module = m_Modules[i];

		Module->UserDelete(Username);
	}

	if (RemoveConfig) {
		unlink(User->GetConfig()->GetFilename());
		unlink(User->GetLog()->GetFilename());
	}

	delete User;
	m_Users.Remove(Username);

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

bool CCore::IsValidUsername(const char* Username) {
	for (unsigned int i = 0; i < strlen(Username); i++) {
		if (!isalnum(Username[i]))
			return false;
	}

	if (strlen(Username) == 0)
		return false;
	else
		return true;
}

void CCore::UpdateUserConfig(void) {
	int i;
	char* Out = NULL;

	i = 0;
	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		bool WasNull = false;

		if (Out == NULL)
			WasNull = true;

		Out = (char*)realloc(Out, (Out ? strlen(Out) : 0) + strlen(User->Name) + 10);

		if (Out == NULL) {
			LOGERROR("realloc() failed. Userlist in sbnc.conf might be out of date.");

			return;
		}

		if (WasNull)
			*Out = '\0';

		if (*Out) {
			strcat(Out, " ");
			strcat(Out, User->Name);
		} else {
			strcpy(Out, User->Name);
		}
	}

	if (m_Config)
		m_Config->WriteString("system.users", Out);

	free(Out);
}

time_t CCore::GetStartup(void) {
	return m_Startup;
}

bool CCore::Daemonize(void) {
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

void CCore::WritePidFile(void) {
#ifndef _WIN32
	pid_t pid = getpid();
#else
	DWORD pid = GetCurrentProcessId();
#endif


	if (pid) {
		FILE* pidFile = fopen("sbnc.pid", "w");

		if (pidFile) {
			fprintf(pidFile, "%d", pid);
			fclose(pidFile);
		}
	}
}

const char* CCore::MD5(const char* String) {
	return UtilMd5(String);
}


int CCore::GetArgC(void) {
	return m_Args.GetLength();
}

char** CCore::GetArgV(void) {
	return m_Args.GetList();
}

CConnection* CCore::WrapSocket(SOCKET Socket, bool SSL, connection_role_e Role) {
	CConnection* Wrapper = new CConnection(Socket, SSL, Role);

	Wrapper->m_Wrapper = true;

	return Wrapper;
}

void CCore::DeleteWrapper(CConnection* Wrapper) {
	delete Wrapper;
}

void CCore::Free(void* Pointer) {
	free(Pointer);
}

void* CCore::Alloc(size_t Size) {
	return malloc(Size);
}

bool CCore::IsRegisteredSocket(CSocketEvents* Events) {
	for (unsigned int i = 0; i < m_OtherSockets.GetLength(); i++) {
		if (m_OtherSockets[i].Events == Events)
			return true;
	}

	return false;
}

SOCKET CCore::SocketAndConnect(const char* Host, unsigned short Port, const char* BindIp) {
	return ::SocketAndConnect(Host, Port, BindIp);
}

socket_t* CCore::GetSocketByClass(const char* Class, int Index) {
	int a = 0;

	for (unsigned int i = 0; i < m_OtherSockets.GetLength(); i++) {
		socket_t Socket = m_OtherSockets[i];

		if (Socket.Socket == INVALID_SOCKET)
			continue;

		if (strcmp(Socket.Events->GetClassName(), Class) == 0)
			a++;

		if (a - 1 == Index)
			return &m_OtherSockets[i];
	}

	return NULL;
}

CTimer* CCore::CreateTimer(unsigned int Interval, bool Repeat, TimerProc Function, void* Cookie) {
	return new CTimer(Interval, Repeat, Function, Cookie);
}

void CCore::RegisterTimer(CTimer* Timer) {
	m_Timers.Insert(Timer);
}

void CCore::UnregisterTimer(CTimer* Timer) {
	m_Timers.Remove(Timer);
}

int CCore::GetTimerStats(void) {
#ifdef _DEBUG
	return g_TimerStats;
#else
	return 0;
#endif
}

bool CCore::Match(const char *Pattern, const char *String) {
	return (match(Pattern, String) == 0);
}

int CCore::GetSendQSize(void) {
	if (m_SendQSizeCache != -1)
		return m_SendQSizeCache;

	int Size = m_Config->ReadInteger("system.sendq");

	if (Size == 0)
		return DEFAULT_SENDQ;
	else
		return Size;
}

void CCore::SetSendQSize(int NewSize) {
	m_Config->WriteInteger("system.sendq", NewSize);
	m_SendQSizeCache = NewSize;
}

const char *CCore::GetMotd(void) {
	return m_Config->ReadString("system.motd");
}

void CCore::SetMotd(const char* Motd) {
	m_Config->WriteString("system.motd", Motd);
}

void CCore::Fatal(void) {
	Log("Fatal error occured.");

	exit(1);
}

SSL_CTX *CCore::GetSSLContext(void) {
	return m_SSLContext;
}

SSL_CTX *CCore::GetSSLClientContext(void) {
	return m_SSLClientContext;
}

int CCore::GetSSLCustomIndex(void) {
#ifdef USESSL
	return g_SSLCustomIndex;
#else
	return 0;
#endif
}

#ifdef USESSL
int SSLVerifyCertificate(int preverify_ok, X509_STORE_CTX *x509ctx) {
	SSL *ssl = (SSL *)X509_STORE_CTX_get_ex_data(x509ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
	CConnection *Ptr = (CConnection *)SSL_get_ex_data(ssl, g_SSLCustomIndex);

	if (Ptr != NULL)
		return Ptr->SSLVerify(preverify_ok, x509ctx);
	else
		return 0;
}
#endif

const char *CCore::DebugImpulse(int impulse) {
	char *Out;
	int i;
	CUser *User;

	if (impulse == 5) {
		InitializeFreeze();

		return "1";
	}

	if (impulse == 6) {
		int a = 23, b = 0, c;
		
		c = a / b;
	}

	if (impulse == 7) {
		_exit(0);
	}

	if (impulse == 8) {
		for (i = 0; i < 200; i++) {
			asprintf(&Out, "test%d", i);
			User = CreateUser(Out, "eris");

			User->SetServer("85.25.15.190");
			User->SetPort(6667);

			User->SetConfigChannels("#test,#test3,#test4");

			User->Reconnect();

			free(Out);
		}
	}

	if (impulse == 9) {
		for (i = 0; i < 200; i++) {
			asprintf(&Out, "test%d", i);
			RemoveUser(Out);
			free(Out);
		}
	}

	return NULL;
}

bool CCore::Freeze(CAssocArray *Box) {
	if (m_Listener) {
		Box->AddInteger("~listener", m_Listener->GetSocket());
		m_Listener->SetSocket(INVALID_SOCKET);
	} else {
		Box->AddInteger("~listener", INVALID_SOCKET);
	}

	if (m_SSLListener) {
		Box->AddInteger("~ssllistener", m_SSLListener->GetSocket());
		m_SSLListener->SetSocket(INVALID_SOCKET);
	} else {
		Box->AddInteger("~ssllistener", INVALID_SOCKET);
	}

	int i = 0;
	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		char *Username = strdup(User->Name);
		CIRCConnection *IRC = User->Value->GetIRCConnection();
		CClientConnection *Client = User->Value->GetClientConnection();

		if (IRC) {
			CAssocArray *IrcBox = Box->Create();

			if (IRC->Freeze(IrcBox))
				Box->AddBox(Username, IrcBox);
			else
				IrcBox->Destroy();
		}

		if (Client) {
			CAssocArray *ClientBox;
			CAssocArray *ClientsBox = Box->ReadBox("~clients");

			if (ClientsBox == NULL) {
				ClientsBox = Box->Create();
				Box->AddBox("~clients", ClientsBox);
			}

			ClientBox = Box->Create();
			
			if (Client->Freeze(ClientBox))
				ClientsBox->AddBox(Username, ClientBox);
			else
				ClientBox->Destroy();
		}

		free(Username);
	}

	delete this;

	return true;
}

bool CCore::Unfreeze(CAssocArray *Box) {
	SOCKET Listener, SSLListener;
	CAssocArray *ClientsBox;

	Listener = Box->ReadInteger("~listener");

	if (Listener != INVALID_SOCKET)
		m_Listener = new CClientListener(Listener, (CCore*)NULL);

	SSLListener = Box->ReadInteger("~ssllistener");

	if (SSLListener != INVALID_SOCKET)
		m_SSLListener = new CSSLClientListener(SSLListener, (CCore*)NULL);

	ClientsBox = Box->ReadBox("~clients");

	int i = 0;
	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		CIRCConnection *IRC;

		CAssocArray *IrcBox = Box->ReadBox(User->Name);

		if (IrcBox) {
			IRC = CIRCConnection::Unfreeze(IrcBox, User->Value);

			User->Value->SetIRCConnection(IRC);
		}

		if (ClientsBox) {
			CAssocArray *ClientBox = ClientsBox->ReadBox(User->Name);

			if (ClientBox) {
				CClientConnection *Client = new CClientConnection((SOCKET)ClientBox->ReadInteger("client.fd"), ClientBox, User->Value);

				User->Value->SetClientConnection(Client);

				if (User->Value->IsAdmin())
					User->Value->Notice("shroudBNC was reloaded.");
			}
		}
	}

	return true;
}

bool CCore::InitializeFreeze(void) {
	if (!m_Running)
		return false;

	g_Freeze = true;

	return true;
}

const loaderparams_s *CCore::GetLoaderParameters(void) {
	return g_LoaderParameters;
}

const utility_t *CCore::GetUtilities(void) {
	static utility_t *Utils = NULL;

	if (Utils == NULL) {
		Utils = (utility_t *)malloc(sizeof(utility_t));

		Utils->ArgParseServerLine = ArgParseServerLine;
		Utils->ArgTokenize = ArgTokenize;
		Utils->ArgToArray = ArgToArray;
		Utils->ArgRejoinArray = ArgRejoinArray;
		Utils->ArgDupArray = ArgDupArray;
		Utils->ArgFree = ArgFree;
		Utils->ArgFreeArray = ArgFreeArray;
		Utils->ArgGet = ArgGet;
		Utils->ArgCount = ArgCount;

		Utils->FlushCommands = FlushCommands;
		Utils->AddCommand = AddCommand;
		Utils->DeleteCommand = DeleteCommand;
		Utils->CmpCommandT = CmpCommandT;

		Utils->asprintf = asprintf;
	}

	return Utils;
}

bool CCore::MakeConfig(void) {
	int Port;
	char User[81], Password[81];
	char *File;
	CConfig *MainConfig, *UserConfig;

	printf("No valid configuration file has been found. A basic\n"
		"configuration file can be created for you automatically. Please\n"
		"answer the following questions:\n");

	while (true) {
		printf("1. Which port should the bouncer listen on (valid ports are in the range 1025 - 65535): ");
		scanf("%d", &Port);

		if (Port == 0)
			return false;

		if (Port <= 1024 || Port >= 65536)
			printf("You did not enter a valid port. Try again. Use 0 to abort.\n");
		else
			break;
	}

	while (true) {
		printf("2. What should the first user's name be? ");
		scanf("%s", User);
	
		if (strlen(User) == 0)
			return false;

		if (IsValidUsername(User) == false)
			printf("Sorry, this is not a valid username. Try again.\n");
		else
			break;
	}

	printf("3. Please enter a password for the first user: ");
	scanf("%s", Password);

	if (strlen(Password) == 0)
		return false;

	asprintf(&File, "users/%s.conf", User);

	rename("sbnc.conf", "sbnc.conf.old");
	mkdir("users");

	MainConfig = new CConfig("sbnc.conf");

	MainConfig->WriteInteger("system.port", Port);
	MainConfig->WriteInteger("system.md5", 1);
	MainConfig->WriteString("system.users", User);

#ifdef _WIN32
	MainConfig->WriteString("system.modules.mod0", "bnctcl.dll");
#else
	MainConfig->WriteString("system.modules.mod0", "./libbnctcl.la");
#endif

	printf("Writing main configuration file...");

	delete MainConfig;

	printf(" DONE\n");

	UserConfig = new CConfig(File);

	UserConfig->WriteString("user.password", UtilMd5(Password));
	UserConfig->WriteInteger("user.admin", 1);

	printf("Writing first user's configuration file...");

	delete UserConfig;

	printf(" DONE\n");

	free(File);

	return true;
}

const char *CCore::GetTagString(const char *Tag) {
	char *Setting;

	if (Tag == NULL) {
		return NULL;
	}

	asprintf(&Setting, "tag.%s", Tag);

	if (Setting == NULL) {
		LOGERROR("asprintf() failed. Global tag could not be retrieved.");

		return NULL;
	}

	return m_Config->ReadString(Setting);
}

int CCore::GetTagInteger(const char *Tag) {
	const char *Value = GetTagString(Tag);

	if (Value != NULL) {
		return atoi(Value);
	} else {
		return 0;
	}
}

bool CCore::SetTagString(const char *Tag, const char *Value) {
	bool ReturnValue;
	char *Setting;

	if (Tag == NULL) {
		return false;
	}

	asprintf(&Setting, "tag.%s", Tag);

	if (Setting == NULL) {
		LOGERROR("asprintf() failed. Could not store global tag.");

		return false;
	}

	ReturnValue = m_Config->WriteString(Setting, Value);

	for (unsigned int i = 0; i < m_Modules.GetLength(); i++) {
		m_Modules[i]->TagModified(Tag, Value);
	}

	return ReturnValue;
}

bool CCore::SetTagInteger(const char *Tag, int Value) {
	bool ReturnValue;
	char *StringValue;

	asprintf(&StringValue, "%d", Value);

	if (StringValue == NULL) {
		LOGERROR("asprintf() failed. Could not store global tag.");

		return false;
	}

	ReturnValue = SetTagString(Tag, StringValue);

	free(StringValue);

	return ReturnValue;
}

const char *CCore::GetTagName(int Index) {
	int Skip = 0;
	int Count = m_Config->GetLength();

	for (int i = 0; i < Count; i++) {
		hash_t<char *> *Item = m_Config->Iterate(i);

		if (strstr(Item->Name, "tag.") == Item->Name) {
			if (Skip == Index) {
				return Item->Name + 4;
			}

			Skip++;
		}
	}

	return NULL;
}
