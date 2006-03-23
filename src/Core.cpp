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

#ifdef USESSL
#include <openssl/err.h>
#endif

extern loaderparams_s *g_LoaderParameters;

const char* g_ErrorFile;
unsigned int g_ErrorLine;
time_t g_CurrentTime;

CHashtable<command_t, false, 16> *g_Commands = NULL;

#ifdef USESSL
int SSLVerifyCertificate(int preverify_ok, X509_STORE_CTX *x509ctx);
int g_SSLCustomIndex;
#endif

time_t g_LastReconnect = 0;
#ifdef _DEBUG
extern int g_TimerStats;
#endif

IMPL_SOCKETLISTENER(CClientListener) {
	bool m_SSL;
public:
	CClientListener(unsigned int Port, const char *BindIp = NULL, int Family = AF_INET, bool SSL = false) : CListenerBase<CClientListener>(Port, BindIp, Family) {
		m_SSL = SSL;
	}

	CClientListener(void) { }

	virtual void Accept(SOCKET Client, const sockaddr *PeerAddress) {
		CClientConnection *ClientObject;
		unsigned long lTrue = 1;

		ioctlsocket(Client, FIONBIO, &lTrue);

		// destruction is controlled by the main loop
		ClientObject = new CClientConnection(Client, m_SSL);

		//g_Bouncer->Log("Client connected from %s", IpToString(ClientObject->GetRemoteAddress()));
	}

	void SetSSL(bool SSL) {
		m_SSL = SSL;
	}

	bool GetSSL(void) {
		return m_SSL;
	}
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CCore::CCore(CConfig* Config, int argc, char** argv) {
	int i;
	char *Out;
	const char *Hostmask;

	m_Log = new CLog("sbnc.log");

	if (m_Log == NULL) {
		printf("Log system could not be initialized. Shutting down.");

		exit(1);
	}

	m_Log->Clear();
	Log("Log system initialized.");

	g_Bouncer = this;

	m_Config = Config;

	m_Args.SetList(argv, argc);

	m_Ident = new CIdentSupport();

	const char *Users;
	CUser *User;

	while ((Users = Config->ReadString("system.users")) == NULL) {
		if (!MakeConfig()) {
			LOGERROR("Configuration file could not be created.");

			Fatal();
		}

		Config->Reload();
	}

	const char* Args;
	int Count;

	Args = ArgTokenize(Users);

	CHECK_ALLOC_RESULT(Args, ArgTokenize) {
		Fatal();
	} CHECK_ALLOC_RESULT_END;

	Count = ArgCount(Args);

	for (i = 0; i < Count; i++) {
		const char *Name = ArgGet(Args, i + 1);
		User = new CUser(Name);

		CHECK_ALLOC_RESULT(User, new) {
			Fatal();
		} CHECK_ALLOC_RESULT_END;

		m_Users.Add(Name, User);
	}

	ArgFree(Args);

	m_Listener = NULL;
	m_ListenerV6 = NULL;
	m_SSLListener = NULL;
	m_SSLListenerV6 = NULL;

	time(&m_Startup);

	m_SendqSizeCache = -1;

	i = 0;
	while (true) {
		asprintf(&Out, "user.hosts.host%d", i++);

		CHECK_ALLOC_RESULT(Out, asprintf) {
			Fatal();
		} CHECK_ALLOC_RESULT_END;

		Hostmask = m_Config->ReadString(Out);

		free(Out);

		if (Hostmask != NULL) {
			AddHostAllow(Hostmask, false);
		} else {
			break;
		}
	}

	m_Status = STATUS_RUN;

	g_NextCommand = 0;
}

CCore::~CCore() {
	int a, c, d;
	unsigned int i;

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

	for (i = 0; i < m_Zones.GetLength(); i++) {
		m_Zones.Get(i)->PerformLeakCheck();
	}

	for (i = 0; i < m_HostAllows.GetLength(); i++) {
		free(m_HostAllows.Get(i));
	}
}

void CCore::StartMainLoop(void) {
	unsigned int i;
	bool b_DontDetach = false;

	time(&g_CurrentTime);

	printf("shroudBNC %s - an object-oriented IRC bouncer\n", GetBouncerVersion());

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
		if (Port != 0) {
			m_Listener = new CClientListener(Port, BindIp, AF_INET);
		} else {
			m_Listener = NULL;
		}
	}

	if (m_ListenerV6 == NULL) {
		if (Port != 0) {
			m_ListenerV6 = new CClientListener(Port, BindIp, AF_INET6);

			if (m_ListenerV6->IsValid() == false) {
				delete m_ListenerV6;

				m_ListenerV6 = NULL;
			}
		} else {
			m_ListenerV6 = NULL;
		}
	}

#ifdef USESSL
	if (m_SSLListener == NULL) {
		if (SSLPort != 0) {
			m_SSLListener = new CClientListener(SSLPort, BindIp, AF_INET, true);
		} else {
			m_SSLListener = NULL;
		}
	}

	if (m_SSLListenerV6 == NULL) {
		if (SSLPort != 0) {
			m_SSLListenerV6 = new CClientListener(SSLPort, BindIp, AF_INET6, true);

			if (m_SSLListenerV6->IsValid() == false) {
				delete m_SSLListenerV6;

				m_SSLListenerV6 = NULL;
			}
		} else {
			m_SSLListenerV6 = NULL;
		}
	}

	SSL_library_init();
	SSL_load_error_strings();

	SSL_METHOD* SSLMethod = SSLv23_method();
	m_SSLContext = SSL_CTX_new(SSLMethod);
	m_SSLClientContext = SSL_CTX_new(SSLMethod);

	SSL_CTX_set_mode(m_SSLContext, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
	SSL_CTX_set_mode(m_SSLClientContext, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

	g_SSLCustomIndex = SSL_get_ex_new_index(0, (void *)"CConnection*", NULL, NULL, NULL);

	if (!SSL_CTX_use_PrivateKey_file(m_SSLContext, BuildPath("sbnc.key"), SSL_FILETYPE_PEM)) {
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

	if (!SSL_CTX_use_certificate_chain_file(m_SSLContext, BuildPath("sbnc.crt"))) {
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

	sfd_set FDRead, FDWrite, FDError;

	Log("Starting main loop.");

	if (!b_DontDetach)
		Daemonize();

	WritePidFile();

	/* Note: We need to load the modules after using fork() as otherwise tcl cannot be cleanly unloaded */
	m_LoadingModules = true;

#ifdef _MSC_VER
#ifndef _DEBUG
	LoadModule("bnctcl.dll");
#else
	LoadModule("..\\bnctcl\\Debug\\bnctcl.dll");
#endif
#else
	LoadModule("./libbnctcl.la");
#endif

	char *Out;

	i = 0;
	while (true) {
		asprintf(&Out, "system.modules.mod%d", i++);

		CHECK_ALLOC_RESULT(Out, asprintf) {
			Fatal();
		} CHECK_ALLOC_RESULT_END;

		const char* File = m_Config->ReadString(Out);

		free(Out);

		if (File != NULL) {
			LoadModule(File);
		} else {
			break;
		}
	}

	m_LoadingModules = false;

	i = 0;
	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		User->Value->LoadEvent();
	}

	int m_ShutdownLoop = 5;

	if (g_LoaderParameters->SigEnable)
		g_LoaderParameters->SigEnable();

	time_t Last = 0;
	time_t LastCheck = 0;

	while ((GetStatus() == STATUS_RUN || GetStatus() == STATUS_PAUSE || --m_ShutdownLoop) && GetStatus() != STATUS_FREEZE) {
		time_t Now, Best = 0, SleepInterval = 0;

		time(&Now);

		for (int c = m_Timers.GetLength() - 1; c >= 0; c--) {
			CTimer *Timer;
			time_t NextCall;
			
			Timer = m_Timers[c];
			NextCall = Timer->GetNextCall();

			if (Now >= NextCall) {
				if (Now - 5 > NextCall) {
					Log("Timer drift for timer %p: %d seconds", Timer, Now - NextCall);
				}

				Timer->Call(Now);
			}
		}

		if (g_NextCommand > Now) {
			Best = g_NextCommand;
		} else {
			Best = Now + 60;
		}

		for (int c = m_Timers.GetLength() - 1; c >= 0; c--) {
			time_t NextCall = m_Timers[c]->GetNextCall();

			if (NextCall < Best) {
				Best = NextCall;
			}
		}

		SleepInterval = Best - Now;

		SFD_ZERO(&FDRead);
		SFD_ZERO(&FDWrite);
		SFD_ZERO(&FDError);

		CUser *ReconnectUser = NULL;

		i = 0;
		while (hash_t<CUser *> *UserHash = m_Users.Iterate(i++)) {
			CIRCConnection* IRC;

			if (UserHash->Value) {
				if ((IRC = UserHash->Value->GetIRCConnection()) != NULL) {
					if (GetStatus() != STATUS_RUN && GetStatus() != STATUS_PAUSE && IRC->IsLocked() == false) {
						Log("Closing connection for %s", UserHash->Name);
						IRC->Kill("Shutting down.");

						UserHash->Value->SetIRCConnection(NULL);
					}

					if (IRC->ShouldDestroy())
						IRC->Destroy();
				}

				if ((GetStatus() == STATUS_RUN || GetStatus() == STATUS_PAUSE) && LastCheck + 5 < Now && UserHash->Value->ShouldReconnect()) {
					if (ReconnectUser == NULL || (!ReconnectUser->IsAdmin() && UserHash->Value->IsAdmin())) {
						ReconnectUser = UserHash->Value;
					}

					LastCheck = Now;
				}
			}
		}

		if (ReconnectUser != NULL) {
			ReconnectUser->ScheduleReconnect();
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

				SFD_SET(m_OtherSockets[i].Socket, &FDRead);
				SFD_SET(m_OtherSockets[i].Socket, &FDError);

				if (m_OtherSockets[i].Events->HasQueuedData()) {
					SFD_SET(m_OtherSockets[i].Socket, &FDWrite);
				}
			}
		}

		if (SleepInterval <= 0 || (GetStatus() != STATUS_RUN && GetStatus() != STATUS_PAUSE)) {
			SleepInterval = 1;
		}

		timeval interval = { SleepInterval, 0 };

		int nfds = 0;
		timeval tv;
		timeval* tvp = &tv;

		memset(tvp, 0, sizeof(timeval));

		if (GetStatus() != STATUS_RUN && GetStatus() != STATUS_PAUSE && SleepInterval > 3)
			interval.tv_sec = 3;

		for (unsigned int i = 0; i < m_DnsQueries.GetLength(); i++) {
			ares_channel Channel = m_DnsQueries[i]->GetChannel();

			ares_fds(Channel, &FDRead, &FDWrite);
			tvp = ares_timeout(Channel, NULL, &tv);
		}

		time(&Last);

#ifdef _DEBUG
		printf("select/poll: %d seconds\n", SleepInterval);
#endif

#if defined(HAVE_POLL) || !defined(_WIN32)
		unsigned int FDCount;
		pollfd *foo = FdSetToPollFd(&FDRead, &FDWrite, &FDError, &FDCount);

		int ready = poll(foo, FDCount, SleepInterval * 1000);

		PollFdToFdSet(foo, FDCount, &FDRead, &FDWrite, &FDError);
#else
		int ready = select(FD_SETSIZE - 1, &FDRead, &FDWrite, &FDError, &interval);
#endif

#ifdef LEAKLEAK
		CHECK_LEAKS();
#endif

		time(&g_CurrentTime);

		for (unsigned int i = 0; i < m_DnsQueries.GetLength(); i++) {
			ares_channel Channel = m_DnsQueries[i]->GetChannel();

			ares_process(Channel, &FDRead, &FDWrite);
		}

		if (ready > 0) {
			//printf("%d socket(s) ready\n", ready);

			for (int a = m_OtherSockets.GetLength() - 1; a >= 0; a--) {
				SOCKET Socket = m_OtherSockets[a].Socket;
				CSocketEvents* Events = m_OtherSockets[a].Events;

				if (Socket != INVALID_SOCKET) {
					if (FD_ISSET(Socket, &FDError)) {
						Events->Error();
						Events->Destroy();

						continue;
					}

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
#ifndef _WIN32
			if (errno != EBADF) {
#else
			if (WSAGetLastError() != WSAENOTSOCK) {
#endif
				continue;
			}

#if !defined(HAVE_POLL) && defined(_WIN32)

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
#else
			pollfd pfd;

			for (int a = m_OtherSockets.GetLength() - 1; a >= 0; a--) {
				SOCKET Socket = m_OtherSockets[a].Socket;

				if (Socket != INVALID_SOCKET) {
					pfd.fd = Socket;

					int code = poll(&pfd, 1, 0);

					if (code == -1) {
						m_OtherSockets[a].Events->Error();
						m_OtherSockets[a].Events->Destroy();
					}
				}
			}
#endif
		}
	}

#ifdef USESSL
	SSL_CTX_free(m_SSLContext);
	SSL_CTX_free(m_SSLClientContext);
#endif
}

CUser *CCore::GetUser(const char* Name) {
	if (Name == NULL) {
		return NULL;
	} else {
		return m_Users.Get(Name);
	}
}

void CCore::GlobalNotice(const char *Text) {
	unsigned int i = 0;

	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		User->Value->Notice(Text);
	}
}

CHashtable<CUser *, false, 64> *CCore::GetUsers(void) {
	return &m_Users;
}

void CCore::SetIdent(const char* Ident) {
	if (m_Ident)
		m_Ident->SetIdent(Ident);
}

const char* CCore::GetIdent(void) const {
	if (m_Ident != NULL)
		return m_Ident->GetIdent();
	else
		return NULL;
}

const CVector<CModule *> *CCore::GetModules(void) const {
	return &m_Modules;
}

RESULT<CModule *> CCore::LoadModule(const char *Filename) {
	char *CorePath;
	RESULT<bool> Result;

	CorePath = strdup(g_LoaderParameters->GetModulePath());

	CHECK_ALLOC_RESULT(CorePath, GetModule) {
		THROW(CModule *, Generic_OutOfMemory, "strdup() failed.");
	} CHECK_ALLOC_RESULT_END;

	for (size_t i = strlen(CorePath) - 1; i >= 0; i--) {
		if (CorePath[i] == '/' || CorePath[i] == '\\') {
			CorePath[i] = '\0';

			break;
		}
	}

#if !defined(_WIN32) || defined(__MINGW32__)
	lt_dlsetsearchpath(CorePath);
#endif

	CModule *Module = new CModule(BuildPath(Filename, CorePath));

	free(CorePath);

	CHECK_ALLOC_RESULT(Module, new) {
		THROW(CModule *, Generic_OutOfMemory, "new operator failed.");
	} CHECK_ALLOC_RESULT_END;

	Result = Module->GetError();

	if (!IsError(Result)) {
		Result = m_Modules.Insert(Module);

		if (IsError(Result)) {
			delete Module;

			LOGERROR("Insert() failed. Could not load module");

			THROWRESULT(CModule *, Result);
		}

		Log("Loaded module: %s", Module->GetFilename());

		Module->Init(this);

		if (!m_LoadingModules) {
			UpdateModuleConfig();
		}

		RETURN(CModule *, Module);
	} else {
		Log("Module %s could not be loaded: %s", Filename, GETDESCRIPTION(Result));

		delete Module;

		THROWRESULT(CModule *, Result);
	}

	THROW(CModule *, Generic_Unknown, NULL);
}

bool CCore::UnloadModule(CModule* Module) {
	if (m_Modules.Remove(Module)) {
		Log("Unloaded module: %s", Module->GetFilename());

		delete Module;

		UpdateModuleConfig();

		return true;
	} else {
		return false;
	}
}

void CCore::UpdateModuleConfig(void) {
	char* Out;
	int a = 0;

	for (unsigned int i = 0; i < m_Modules.GetLength(); i++) {
		asprintf(&Out, "system.modules.mod%d", a++);

		CHECK_ALLOC_RESULT(Out, asprintf) {
			Fatal();
		} CHECK_ALLOC_RESULT_END;

		m_Config->WriteString(Out, m_Modules[i]->GetFilename());

		free(Out);
	}

	asprintf(&Out, "system.modules.mod%d", a);

	CHECK_ALLOC_RESULT(Out, asprintf) {
		Fatal();
	} CHECK_ALLOC_RESULT_END;

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

SOCKET CCore::CreateListener(unsigned short Port, const char *BindIp, int Family) const {
	return ::CreateListener(Port, BindIp, Family);
}

void CCore::Log(const char *Format, ...) {
	char *Out;
	int Ret;
	va_list marker;

	va_start(marker, Format);
	Ret = vasprintf(&Out, Format, marker);
	va_end(marker);

	CHECK_ALLOC_RESULT(Out, vasprintf) {
		return;
	} CHECK_ALLOC_RESULT_END;

	m_Log->WriteLine(NULL, "%s", Out);

	for (unsigned int i = 0; i < m_Users.GetLength(); i++) {
		CUser *User = m_Users.Iterate(i)->Value;
		if (User->IsAdmin() && User->GetSystemNotices()) {
			User->Notice(Out);
		}
	}

	free(Out);
}

void CCore::InternalLogError(const char *Format, ...) {
	char Format2[512];
	char Out[512];
	const char* P = g_ErrorFile;
	va_list marker;

	while (*P++) {
		if (*P == '\\') {
			g_ErrorFile = P + 1;
		}
	}

	snprintf(Format2, sizeof(Format2), "Error (in %s:%d): %s", g_ErrorFile, g_ErrorLine, Format);

	va_start(marker, Format);
	vsnprintf(Out, sizeof(Out), Format2, marker);
	va_end(marker);

	m_Log->WriteUnformattedLine(NULL, Out);
}

void CCore::InternalSetFileAndLine(const char *Filename, unsigned int Line) {
	g_ErrorFile = Filename;
	g_ErrorLine = Line;
}

CConfig *CCore::GetConfig(void) {
	return m_Config;
}

CLog *CCore::GetLog(void) {
	return m_Log;
}

void CCore::Shutdown(void) {
	g_Bouncer->Log("Shutdown requested.");

	SetStatus(STATUS_SHUTDOWN);
}

RESULT<CUser *> CCore::CreateUser(const char* Username, const char* Password) {
	CUser *User;
	RESULT<bool> Result;

	User = GetUser(Username);

	if (User != NULL) {
		if (Password != NULL) {
			User->SetPassword(Password);
		}

		RETURN(CUser *, User);
	}

	if (!IsValidUsername(Username)) {
		THROW(CUser *, Generic_Unknown, "The username you specified is not valid.");
	}

	User = new CUser(Username);

	Result = m_Users.Add(Username, User);

	if (IsError(Result)) {
		delete User;

		THROWRESULT(CUser *, Result);
	}

	if (Password != NULL) {
		User->SetPassword(Password);
	}

	Log("New user created: %s", Username);

	UpdateUserConfig();

	for (unsigned int i = 0; i < m_Modules.GetLength(); i++) {
		m_Modules[i]->UserCreate(Username);
	}

	User->LoadEvent();

	RETURN(CUser *, User);
}

RESULT<bool> CCore::RemoveUser(const char* Username, bool RemoveConfig) {
	RESULT<bool> Result;
	CUser *User;
	
	User = GetUser(Username);

	if (User == NULL) {
		THROW(bool, Generic_Unknown, "There is no such user.");
	}

	for (unsigned int i = 0; i < m_Modules.GetLength(); i++) {
		m_Modules[i]->UserDelete(Username);
	}

	if (RemoveConfig) {
		unlink(User->GetConfig()->GetFilename());
		unlink(User->GetLog()->GetFilename());
	}

	delete User;
	
	Result = m_Users.Remove(Username);

	THROWIFERROR(bool, Result);

	Log("User removed: %s", Username);

	UpdateUserConfig();

	RETURN(bool, true);
}

bool CCore::IsValidUsername(const char *Username) const {
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

time_t CCore::GetStartup(void) const {
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
#else
	char *Title;

	asprintf(&Title, "shroudBNC %s", GetBouncerVersion());

	if (Title != NULL) {
		SetConsoleTitle(Title);

		free(Title);
	}
#endif

	return true;
}

void CCore::WritePidFile(void) const {
#ifndef _WIN32
	pid_t pid = getpid();
#else
	DWORD pid = GetCurrentProcessId();
#endif


	if (pid) {
		FILE* pidFile;

		pidFile = fopen(BuildPath("sbnc.pid"), "w");

		SetPermissions(BuildPath("sbnc.pid"), S_IRUSR | S_IWUSR);

		if (pidFile) {
			fprintf(pidFile, "%d", pid);
			fclose(pidFile);
		}
	}
}

const char *CCore::MD5(const char* String) const {
	return UtilMd5(String);
}


int CCore::GetArgC(void) const {
	return m_Args.GetLength();
}

const char* const* CCore::GetArgV(void) const {
	return m_Args.GetList();
}

CConnection *CCore::WrapSocket(SOCKET Socket, bool SSL, connection_role_e Role) const {
	CConnection *Wrapper = new CConnection(Socket, SSL, Role);

	Wrapper->m_Wrapper = true;

	return Wrapper;
}

void CCore::DeleteWrapper(CConnection *Wrapper) const {
	delete Wrapper;
}

bool CCore::IsRegisteredSocket(CSocketEvents *Events) const {
	for (unsigned int i = 0; i < m_OtherSockets.GetLength(); i++) {
		if (m_OtherSockets[i].Events == Events)
			return true;
	}

	return false;
}

SOCKET CCore::SocketAndConnect(const char *Host, unsigned short Port, const char *BindIp) {
	return ::SocketAndConnect(Host, Port, BindIp);
}

const socket_t *CCore::GetSocketByClass(const char *Class, int Index) const {
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

CTimer *CCore::CreateTimer(unsigned int Interval, bool Repeat, TimerProc Function, void *Cookie) const {
	return new CTimer(Interval, Repeat, Function, Cookie);
}

void CCore::RegisterTimer(CTimer *Timer) {
	m_Timers.Insert(Timer);
}

void CCore::UnregisterTimer(CTimer *Timer) {
	m_Timers.Remove(Timer);
}

void CCore::RegisterDnsQuery(CDnsQuery *DnsQuery) {
	m_DnsQueries.Insert(DnsQuery);
}

void CCore::UnregisterDnsQuery(CDnsQuery *DnsQuery) {
	m_DnsQueries.Remove(DnsQuery);
}

int CCore::GetTimerStats(void) const {
#ifdef _DEBUG
	return g_TimerStats;
#else
	return 0;
#endif
}

bool CCore::Match(const char *Pattern, const char *String) const {
	return (match(Pattern, String) == 0);
}

size_t CCore::GetSendqSize(void) const {
	int Size;

	if (m_SendqSizeCache != -1) {
		return m_SendqSizeCache;
	}

	Size = m_Config->ReadInteger("system.sendq");

	if (Size == 0) {
		return DEFAULT_SENDQ;
	} else {
		return Size;
	}
}

void CCore::SetSendqSize(size_t NewSize) {
	m_Config->WriteInteger("system.sendq", NewSize);
	m_SendqSizeCache = NewSize;
}

const char *CCore::GetMotd(void) const {
	return m_Config->ReadString("system.motd");
}

void CCore::SetMotd(const char *Motd) {
	m_Config->WriteString("system.motd", Motd);
}

void CCore::Fatal(void) {
	Log("Fatal error occured.");

	exit(EXIT_FAILURE);
}

SSL_CTX *CCore::GetSSLContext(void) {
	return m_SSLContext;
}

SSL_CTX *CCore::GetSSLClientContext(void) {
	return m_SSLClientContext;
}

int CCore::GetSSLCustomIndex(void) const {
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

	if (impulse == 10) {
		char *Name;

		for (unsigned int i = 0; i < 100; i++) {
			asprintf(&Name, "test%d", rand());

			CUser *User = CreateUser(Name, NULL);

//			if ((rand() + 1) % 2 == 0)
				User->SetServer("217.112.85.191");
//			else
//				User->SetServer("irc.saeder-krupp.org");

			User->SetLeanMode(2);

			User->Reconnect();

			free(Name);
		}
	}

	if (impulse == 11) {
		int i = 0;
		hash_t<CUser *> *User;

		while ((User = g_Bouncer->GetUsers()->Iterate(i++)) != NULL) {
			if (match("test*", User->Name) == 0) {
				g_Bouncer->RemoveUser(User->Name);
			}
		}
	}

	return NULL;
}

bool CCore::Freeze(CAssocArray *Box) {
	FreezeObject<CClientListener>(Box, "~listener", m_Listener);
	FreezeObject<CClientListener>(Box, "~ssllistener", m_SSLListener);
	FreezeObject<CClientListener>(Box, "~listenerv6", m_ListenerV6);
	FreezeObject<CClientListener>(Box, "~ssllistenerv6", m_SSLListenerV6);

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

bool CCore::Thaw(CAssocArray *Box) {
	CAssocArray *ClientsBox;

	m_Listener = ThawObject<CClientListener>(Box, "~listener");
	m_ListenerV6 = ThawObject<CClientListener>(Box, "~listenerv6");

	m_SSLListener = ThawObject<CClientListener>(Box, "~ssllistener");
	m_SSLListener->SetSSL(true);
	m_SSLListenerV6 = ThawObject<CClientListener>(Box, "~ssllistenerv6");
	m_SSLListenerV6->SetSSL(true);

	ClientsBox = Box->ReadBox("~clients");

	int i = 0;
	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		CIRCConnection *IRC;

		CAssocArray *IrcBox = Box->ReadBox(User->Name);

		if (IrcBox) {
			IRC = CIRCConnection::Thaw(IrcBox, User->Value);

			User->Value->SetIRCConnection(IRC);
		}

		if (ClientsBox != NULL) {
			CClientConnection *Client;

			Client = ThawObject<CClientConnection>(ClientsBox, User->Name);

			if (Client != NULL) {
				Client->SetOwner(User->Value);
				User->Value->SetClientConnection(Client);

				if (User->Value->IsAdmin()) {
					User->Value->Notice("shroudBNC was reloaded.");
				}
			}
		}
	}

	return true;
}

bool CCore::InitializeFreeze(void) {
	if (GetStatus() != STATUS_RUN && GetStatus() != STATUS_PAUSE) {
		return false;
	}

	SetStatus(STATUS_FREEZE);

	return true;
}

const loaderparams_s *CCore::GetLoaderParameters(void) const {
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

		Utils->Alloc = malloc;
		Utils->Free = free;

		Utils->IpToString = IpToString;
	}

	return Utils;
}

bool CCore::MakeConfig(void) {
	int Port;
	char User[81], Password[81], PasswordConfirm[81];
	char *File;
	CConfig *MainConfig, *UserConfig;
	bool term_succeeded;
#ifndef _WIN32
	termios term_old, term_new;
#else
	HANDLE StdInHandle;
	DWORD ConsoleModes, NewConsoleModes;
#endif

	printf("No valid configuration file has been found. A basic\n"
		"configuration file can be created for you automatically. Please\n"
		"answer the following questions:\n");

	while (true) {
		printf("1. Which port should the bouncer listen on (valid ports are in the range 1025 - 65535): ");
		scanf("%d", &Port);

		if (Port == 0) {
			return false;
		}

#ifdef _WIN32
		if (Port <= 1024 || Port >= 65536) {
#else
		if (Port <= 0 || Port >= 65536) {
#endif
			printf("You did not enter a valid port. Try again. Use 0 to abort.\n");
		} else {
			break;
		}
	}

	while (true) {
		printf("2. What should the first user's name be? ");
		scanf("%s", User);
	
		if (strlen(User) == 0) {
			return false;
		}

		if (IsValidUsername(User) == false) {
			printf("Sorry, this is not a valid username. Try again.\n");
		} else {
			break;
		}
	}

	while (true) {
		printf("Please note that passwords will not be echoed while you type them.\n");
		printf("3. Please enter a password for the first user: ");

		// disable terminal echo
#ifndef _WIN32
		if (tcgetattr(STDIN_FILENO, &term_old) == 0) {
			memcpy(&term_new, &term_old, sizeof(term_old));
			term_new.c_lflag &= ~ECHO;

			tcsetattr(STDIN_FILENO, TCSANOW, &term_new);

			term_succeeded = true;
		} else {
			term_succeeded = false;
		}
#else
	StdInHandle = GetStdHandle(STD_INPUT_HANDLE);

	if (StdInHandle != INVALID_HANDLE_VALUE) {
		if (GetConsoleMode(StdInHandle, &ConsoleModes)) {
			NewConsoleModes = ConsoleModes & ~ENABLE_ECHO_INPUT;

			SetConsoleMode(StdInHandle,NewConsoleModes);

			term_succeeded = true;
		} else {
			term_succeeded = false;
		}
	}
#endif

		scanf("%s", Password);

		if (strlen(Password) == 0) {
			if (term_succeeded) {
#ifndef _WIN32
				tcsetattr(STDIN_FILENO, TCSANOW, &term_old);
#else
				SetConsoleMode(StdInHandle, ConsoleModes);
#endif
			}

			return false;
		}

		printf("\n4. Please confirm your password by typing it again: ");

		scanf("%s", PasswordConfirm);

		// reset terminal echo
		if (term_succeeded) {
#ifndef _WIN32
			tcsetattr(STDIN_FILENO, TCSANOW, &term_old);
#else
			SetConsoleMode(StdInHandle, ConsoleModes);
#endif
		}

		printf("\n");

		if (strcmp(Password, PasswordConfirm) == 0) {
			break;
		} else {
			printf("The passwords you entered do not match. Please try again.\n");
		}
	}

	asprintf(&File, "users/%s.conf", User);

	// BuildPath is using a static buffer
	char *SourcePath = strdup(BuildPath("sbnc.conf"));
	rename(SourcePath, BuildPath("sbnc.conf.old"));
	free(SourcePath);
	mkdir(BuildPath("users"));
	SetPermissions(BuildPath("users"), S_IRUSR | S_IWUSR | S_IXUSR);

	MainConfig = new CConfig(BuildPath("sbnc.conf"));

	MainConfig->WriteInteger("system.port", Port);
	MainConfig->WriteInteger("system.md5", 1);
	MainConfig->WriteString("system.users", User);

	printf("Writing main configuration file...");

	delete MainConfig;

	printf(" DONE\n");

	UserConfig = new CConfig(BuildPath(File));

	UserConfig->WriteString("user.password", UtilMd5(Password));
	UserConfig->WriteInteger("user.admin", 1);

	printf("Writing first user's configuration file...");

	delete UserConfig;

	printf(" DONE\n");

	free(File);

	return true;
}

const char *CCore::GetTagString(const char *Tag) const {
	const char *Value;
	char *Setting;

	if (Tag == NULL) {
		return NULL;
	}

	asprintf(&Setting, "tag.%s", Tag);

	CHECK_ALLOC_RESULT(Setting, asprintf) {
		LOGERROR("asprintf() failed. Global tag could not be retrieved.");

		return NULL;
	} CHECK_ALLOC_RESULT_END;

	Value = m_Config->ReadString(Setting);

	free(Setting);

	return Value;
}

int CCore::GetTagInteger(const char *Tag) const {
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

	CHECK_ALLOC_RESULT(Setting, asprintf) {
		LOGERROR("asprintf() failed. Could not store global tag.");

		return false;
	} CHECK_ALLOC_RESULT_END;

	for (unsigned int i = 0; i < m_Modules.GetLength(); i++) {
		m_Modules[i]->TagModified(Tag, Value);
	}

	ReturnValue = m_Config->WriteString(Setting, Value);

	free(Setting);

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

const char *CCore::GetTagName(int Index) const {
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

const char *CCore::GetBasePath(void) const {
	return g_LoaderParameters->basepath;
}

const char *CCore::BuildPath(const char *Filename, const char *BasePath) const {
	return g_LoaderParameters->BuildPath(Filename, BasePath);
}

const char *CCore::GetBouncerVersion(void) const {
	return BNCVERSION;
}

void CCore::SetStatus(int NewStatus) {
	m_Status = NewStatus;
}

int CCore::GetStatus(void) const {
	return m_Status;
}

void CCore::RegisterZone(CZoneInformation *ZoneInformation) {
	m_Zones.Insert(ZoneInformation);
}

const CVector<CZoneInformation *> *CCore::GetZones(void) const {
	return &m_Zones;
}

const CVector<file_t> *CCore::GetAllocationInformation(void) const {
#ifndef _DEBUG
	return NULL;
#else
	unsigned int Count;
	static CVector<file_t> Summary;
	bool Found;

	Summary.Clear();

	Count = g_Allocations.GetLength();

	for (unsigned int i = 0; i < Count; i++) {
		file_t FileInfo;

		Found = false;

		for (unsigned int a = 0; a < Summary.GetLength(); a++) {
			if (strcasecmp(g_Allocations[i].File, Summary[a].File) == 0) {
				Summary[a].AllocationCount++;
				Summary[a].Bytes += g_Allocations[i].Size;
				Found = true;

				break;
			}
		}

		if (Found) {
			continue;
		}

		FileInfo.File = g_Allocations[i].File;
		FileInfo.AllocationCount = 1;
		FileInfo.Bytes = g_Allocations[i].Size;

		Summary.Insert(FileInfo);
	}

	return &Summary;
#endif
}

CFakeClient *CCore::CreateFakeClient(void) const {
	return new CFakeClient();
}

void CCore::DeleteFakeClient(CFakeClient *FakeClient) const {
	delete FakeClient;
}

/**
 * AddHostAllow
 *
 * Adds a new "host allow" entry. Only users are permitted to log in whose
 * IP address/host is matched by a host in this list.
 *
 * @param Mask the new mask
 * @param UpdateConfig whether to update the config files
 */
RESULT<bool> CCore::AddHostAllow(const char *Mask, bool UpdateConfig) {
	char *dupMask;
	RESULT<bool> Result;

	if (Mask == NULL) {
		THROW(bool, Generic_InvalidArgument, "Mask cannot be NULL.");
	}

	if (m_HostAllows.GetLength() > 0 && CanHostConnect(Mask)) {
		THROW(bool, Generic_Unknown, "This hostmask is already added or another hostmask supercedes it.");
	}

	if (!IsValidHostAllow(Mask)) {
		THROW(bool, Generic_Unknown, "The specified mask is not valid.");
	}

	if (m_HostAllows.GetLength() > 50) {
		THROW(bool, Generic_Unknown, "You cannot add more than 50 masks.");
	}

	dupMask = strdup(Mask);

	CHECK_ALLOC_RESULT(dupMask, strdup) {
		THROW(bool, Generic_OutOfMemory, "strdup() failed.");
	} CHECK_ALLOC_RESULT_END;

	Result = m_HostAllows.Insert(dupMask);

	if (IsError(Result)) {
		LOGERROR("Insert() failed. Host could not be added.");

		free(dupMask);

		THROWRESULT(bool, Result);
	}

	if (UpdateConfig) {
		UpdateHosts();
	}

	RETURN(bool, true);
}

/**
 * RemoveHostAllow
 *
 * Removes an item from the host allow list.
 *
 * @param Mask the mask
 * @param UpdateConfig whether to update the config files
 */
RESULT<bool> CCore::RemoveHostAllow(const char *Mask, bool UpdateConfig) {
	for (int i = m_HostAllows.GetLength() - 1; i >= 0; i--) {
		if (strcasecmp(m_HostAllows[i], Mask) == 0) {
			free(m_HostAllows[i]);
			m_HostAllows.Remove(i);

			if (UpdateConfig) {
				UpdateHosts();
			}

			RETURN(bool, true);
		}
	}

	THROW(bool, Generic_Unknown, "Host was not found.");
}

/**
 * IsValidHostAllow
 *
 * Checks whether the specified hostmask is valid for /sbnc hosts.
 *
 * @param Mask the mask
 */
bool CCore::IsValidHostAllow(const char *Mask) const {
	if (Mask == NULL || strchr(Mask, '!') != NULL || strchr(Mask, '@') != NULL) {
		return false;
	} else {
		return true;
	}
}

/**
 * GetHostAllows
 *
 * Returns a list of "host allow" masks.
 */
const CVector<char *> *CCore::GetHostAllows(void) const {
	return &m_HostAllows;
}

/**
 * CanHostConnect
 *
 * Checks whether the specified host can use the user's account.
 *
 * @param Host the host
 */
bool CCore::CanHostConnect(const char *Host) const {
	unsigned int Count = 0;

	for (unsigned int i = 0; i < m_HostAllows.GetLength(); i++) {
		if (mmatch(m_HostAllows[i], Host) == 0) {
			return true;
		} else {
			Count++;
		}
	}

	if (Count > 0) {
		return false;
	} else {
		return true;	
	}
}

/**
 * UpdateHosts
 *
 * Updates the "host allow" list in the config file.
 */
void CCore::UpdateHosts(void) {
	char *Out;
	int a = 0;

	for (unsigned int i = 0; i < m_HostAllows.GetLength(); i++) {
		asprintf(&Out, "user.hosts.host%d", a++);

		CHECK_ALLOC_RESULT(Out, asprintf) {
			g_Bouncer->Fatal();
		} CHECK_ALLOC_RESULT_END;

		m_Config->WriteString(Out, m_HostAllows[i]);
		free(Out);
	}

	asprintf(&Out, "user.hosts.host%d", a);

	CHECK_ALLOC_RESULT(Out, asprintf) {
		g_Bouncer->Fatal();
	} CHECK_ALLOC_RESULT_END;

	m_Config->WriteString(Out, NULL);
	free(Out);
}
