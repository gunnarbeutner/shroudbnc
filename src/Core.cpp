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

extern loaderparams_s *g_LoaderParameters; /**< loader parameters */

const char *g_ErrorFile; /**< name of the file where the last error occured */
unsigned int g_ErrorLine; /**< line where the last error occurred */
time_t g_CurrentTime; /**< current time (updated in main loop) */

#ifdef USESSL
int SSLVerifyCertificate(int preverify_ok, X509_STORE_CTX *x509ctx);
int g_SSLCustomIndex; /**< custom SSL index */
#endif

time_t g_LastReconnect = 0; /**< time of the last reconnect */

/**
 * CCore
 *
 * Creates a new shroudBNC application object.
 *
 * @param Config the main config object
 * @param argc argument counts
 * @param argv program arguments
 */
CCore::CCore(CConfig *Config, int argc, char **argv) {
	int i;
	char *Out;
	const char *Hostmask;

	char *SourcePath = strdup(BuildPath("sbnc.log"));
	rename(SourcePath, BuildPath("sbnc.log.old"));
	free(SourcePath);

	m_Log = new CLog("sbnc.log");

	if (m_Log == NULL) {
		printf("Log system could not be initialized. Shutting down.");

		exit(EXIT_FAILURE);
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

	const char *Args;
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
		asprintf(&Out, "system.hosts.host%d", i++);

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

	m_LoadingModules = false;
	m_LoadingListeners = false;
}

/**
 * ~CCore
 *
 * Destructs a CCore object.
 */
CCore::~CCore(void) {
	int a;
	unsigned int i;

	for (a = m_Modules.GetLength() - 1; a >= 0; a--) {
		delete m_Modules[a];
	}

	m_Modules.Clear();

	UninitializeAdditionalListeners();

	for (a = m_OtherSockets.GetLength() - 1; a >= 0; a--) {
		if (m_OtherSockets[a].Socket != INVALID_SOCKET) {
			m_OtherSockets[a].Events->Destroy();
		}
	}

	i = 0;
	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		delete User->Value;
	}

	for (a = m_Timers.GetLength() - 1; a >= 0 ; a--) {
		delete m_Timers[a];
	}

	delete m_Log;
	delete m_Ident;

	g_Bouncer = NULL;

	for (i = 0; i < m_Zones.GetLength(); i++) {
		m_Zones[i]->PerformLeakCheck();
	}

	for (i = 0; i < m_HostAllows.GetLength(); i++) {
		free(m_HostAllows[i]);
	}
}

/**
 * StartMainLoop
 *
 * Executes shroudBNC's main loop.
 */
void CCore::StartMainLoop(void) {
	unsigned int i;
	bool b_DontDetach = false;

	time(&g_CurrentTime);

	printf("shroudBNC %s - an object-oriented IRC bouncer\n", GetBouncerVersion());

	int argc = m_Args.GetLength();
	char **argv = m_Args.GetList();

	for (int a = 1; a < argc; a++) {
		if (strcmp(argv[a], "-n") == 0 || strcmp(argv[a], "/n") == 0) {
			b_DontDetach = true;
		}

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

	if (Port == 0 && SSLPort == 0) {
#else
	if (Port == 0) {
#endif
		Port = 9000;
	}

	const char *BindIp = g_Bouncer->GetConfig()->ReadString("system.ip");

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

	SSL_METHOD *SSLMethod = SSLv23_method();
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

	if (Port != 0 && m_Listener != NULL && m_Listener->IsValid()) {
		Log("Created main listener.");
	} else if (Port != 0) {
		Log("Could not create listener port");
		return;
	}

#ifdef USESSL
	if (SSLPort != 0 && m_SSLListener != NULL && m_SSLListener->IsValid()) {
		Log("Created ssl listener.");
	} else if (SSLPort != 0) {
		Log("Could not create ssl listener port");
		return;
	}
#endif

	InitializeAdditionalListeners();

	sfd_set FDRead, FDWrite, FDError;

	Log("Starting main loop.");

	if (!b_DontDetach) {
		Daemonize();
	}

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

		const char *File = m_Config->ReadString(Out);

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

	if (g_LoaderParameters->SigEnable != NULL) {
		g_LoaderParameters->SigEnable();
	}

	time_t Last = 0;
	time_t LastCheck = 0;

	while ((GetStatus() == STATUS_RUN || GetStatus() == STATUS_PAUSE || --m_ShutdownLoop) && GetStatus() != STATUS_FREEZE) {
		time_t Now, Best = 0, SleepInterval = 0;

		time(&Now);

		SFD_ZERO(&FDRead);
		SFD_ZERO(&FDWrite);
		SFD_ZERO(&FDError);

		CUser *ReconnectUser = NULL;

		i = 0;
		while (hash_t<CUser *> *UserHash = m_Users.Iterate(i++)) {
			CIRCConnection *IRC;

			if (UserHash->Value) {
				if ((IRC = UserHash->Value->GetIRCConnection()) != NULL) {
					if (GetStatus() != STATUS_RUN && GetStatus() != STATUS_PAUSE && IRC->IsLocked() == false) {
						Log("Closing connection for %s", UserHash->Name);
						IRC->Kill("Shutting down.");

						UserHash->Value->SetIRCConnection(NULL);
					}

					if (IRC->ShouldDestroy()) {
						IRC->Destroy();
					}
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
				if (m_OtherSockets[a].Events->DoTimeout()) {
					continue;
				} else if (m_OtherSockets[a].Events->ShouldDestroy()) {
					m_OtherSockets[a].Events->Destroy();
				}
			}
		}

		for (i = 0; i < m_OtherSockets.GetLength(); i++) {
			if (m_OtherSockets[i].Socket != INVALID_SOCKET) {
				SFD_SET(m_OtherSockets[i].Socket, &FDRead);
				SFD_SET(m_OtherSockets[i].Socket, &FDError);

				if (m_OtherSockets[i].Events->HasQueuedData()) {
					SFD_SET(m_OtherSockets[i].Socket, &FDWrite);
				}
			}
		}

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

		Best = Now + 60;

		for (int c = m_Timers.GetLength() - 1; c >= 0; c--) {
			time_t NextCall = m_Timers[c]->GetNextCall();

			if (NextCall < Best) {
				Best = NextCall;
			}
		}

		SleepInterval = Best - Now;

		if (SleepInterval <= 0 || (GetStatus() != STATUS_RUN && GetStatus() != STATUS_PAUSE)) {
			SleepInterval = 1;
		}

		timeval interval = { SleepInterval, 0 };

		int nfds = 0;
		timeval tv;
		timeval *tvp = &tv;

		memset(tvp, 0, sizeof(timeval));

		if (GetStatus() != STATUS_RUN && GetStatus() != STATUS_PAUSE && SleepInterval > 3) {
			interval.tv_sec = 3;
		}

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
		int ready = select(SFD_SETSIZE - 1, &FDRead, &FDWrite, &FDError, &interval);
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
				CSocketEvents *Events = m_OtherSockets[a].Events;

				if (Socket != INVALID_SOCKET) {
					if (SFD_ISSET(Socket, &FDError)) {
						Events->Error();
						Events->Destroy();

						continue;
					}

					if (SFD_ISSET(Socket, &FDRead)) {
						if (!Events->Read()) {
							Events->Destroy();

							continue;
						}
					}

					if (Events && SFD_ISSET(Socket, &FDWrite)) {
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
					int code = select(SFD_SETSIZE - 1, &set, NULL, NULL, &zero);

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

/**
 * GetUser
 *
 * Returns a user object for the specified username (or NULL
 * if there's no such user).
 *
 * @param Name the username
 */
CUser *CCore::GetUser(const char *Name) {
	if (Name == NULL) {
		return NULL;
	} else {
		return m_Users.Get(Name);
	}
}

/**
 * GlobalNotice
 *
 * Sends a message to all bouncer users who are currently
 * logged in.
 *
 * @param Text the text of the message
 */
void CCore::GlobalNotice(const char *Text) {
	unsigned int i = 0;

	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		User->Value->Privmsg(Text);
	}
}

/**
 * GetUsers
 *
 * Returns a hashtable which contains all bouncer users.
 */
CHashtable<CUser *, false, 512> *CCore::GetUsers(void) {
	return &m_Users;
}

/**
 * SetIdent
 *
 * Sets the ident which is returned for the next ident request.
 *
 * @param Ident the ident
 */
void CCore::SetIdent(const char *Ident) {
	if (m_Ident != NULL) {
		m_Ident->SetIdent(Ident);
	}
}

/**
 * GetIdent
 *
 * Returns the ident which is to be returned for the next ident
 * request.
 */
const char *CCore::GetIdent(void) const {
	if (m_Ident != NULL) {
		return m_Ident->GetIdent();
	} else {
		return NULL;
	}
}

/**
 * GetModules
 *
 * Returns a list of currently loaded modules.
 */
const CVector<CModule *> *CCore::GetModules(void) const {
	return &m_Modules;
}

/**
 * LoadModule
 *
 * Attempts to load a bouncer module.
 *
 * @param Filename the module's filename
 */
RESULT<CModule *> CCore::LoadModule(const char *Filename) {
	RESULT<bool> Result;

	CModule *Module = new CModule(Filename);

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
		static char *ErrorString = NULL;

		free(ErrorString);

		ErrorString = strdup(GETDESCRIPTION(Result));

		CHECK_ALLOC_RESULT(ErrorString, strdup) {
			delete Module;

			THROW(CModule *, Generic_OutOfMemory, "strdup() failed.");
		} CHECK_ALLOC_RESULT_END;

		Log("Module %s could not be loaded: %s", Filename, ErrorString);

		delete Module;

		THROW(CModule *, Generic_Unknown, ErrorString);
	}

	THROW(CModule *, Generic_Unknown, NULL);
}

/**
 * UnloadModule
 *
 * Unloads a module.
 *
 * @param Module the module
 */
bool CCore::UnloadModule(CModule *Module) {
	if (m_Modules.Remove(Module)) {
		Log("Unloaded module: %s", Module->GetFilename());

		delete Module;

		UpdateModuleConfig();

		return true;
	} else {
		return false;
	}
}

/**
 * UpdateModuleConfig
 *
 * Updates the module list in the main config.
 */
void CCore::UpdateModuleConfig(void) {
	char *Out;
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

/**
 * RegisterSocket
 *
 * Registers an event interface for a socket.
 *
 * @param Socket the socket
 * @param EventInterface the event interface for the socket
 */
void CCore::RegisterSocket(SOCKET Socket, CSocketEvents *EventInterface) {
	socket_s s = { Socket, EventInterface };

	UnregisterSocket(Socket);
	
	/* TODO: can we safely recover from this situation? return value maybe? */
	if (!m_OtherSockets.Insert(s)) {
		LOGERROR("realloc() failed.");

		Fatal();
	}
}

/**
 * UnregisterSocket
 *
 * Unregisters a socket and the associated event interface.
 *
 * @param Socket the socket
 */
void CCore::UnregisterSocket(SOCKET Socket) {
	for (unsigned int i = 0; i < m_OtherSockets.GetLength(); i++) {
		if (m_OtherSockets[i].Socket == Socket) {
			m_OtherSockets.Remove(i);

			return;
		}
	}
}

/**
 * CreateListener
 *
 * Creates a socket listener.
 *
 * @param Port the port for the listener
 * @param BindIp bind address (or NULL)
 * @param Family socket family (AF_INET or AF_INET6)
 */
SOCKET CCore::CreateListener(unsigned short Port, const char *BindIp, int Family) const {
	return ::CreateListener(Port, BindIp, Family);
}

/**
 * Log
 *
 * Logs something in the bouncer's main log.
 *
 * @param Format a format string
 * @param ... additional parameters which are used by the format string
 */
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

	for (unsigned int i = 0; i < m_AdminUsers.GetLength(); i++) {
		CUser *User = m_AdminUsers.Get(i);

		if (User->GetSystemNotices()) {
			User->Privmsg(Out);
		}
	}

	free(Out);
}

/**
 * InternalLogError
 *
 * Logs an internal error.
 *
 * @param Format format string
 * @param ... additional parameters which are used in the format string
 */
void CCore::InternalLogError(const char *Format, ...) {
	char Format2[512];
	char Out[512];
	const char *P = g_ErrorFile;
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

/**
 * InternalSetFileAndLine
 *
 * Used for keeping track of the current file/line in the source code.
 *
 * @param Filename the filename
 * @param Line the line
 */
void CCore::InternalSetFileAndLine(const char *Filename, unsigned int Line) {
	g_ErrorFile = Filename;
	g_ErrorLine = Line;
}

/**
 * GetConfig
 *
 * Returns the bouncer's main config object.
 */
CConfig *CCore::GetConfig(void) {
	return m_Config;
}

/**
 * GetLog
 *
 * Returns the bouncer's main log object.
 */
CLog *CCore::GetLog(void) {
	return m_Log;
}

/**
 * Shutdown
 *
 * Shuts down the bouncer.
 */
void CCore::Shutdown(void) {
	g_Bouncer->Log("Shutdown requested.");

	SetStatus(STATUS_SHUTDOWN);
}

/**
 * CreateUser
 *
 * Creates a new user. If the specified username is already
 * in use that user's password will be re-set instead.
 *
 * @param Username name of the new account
 * @param Password the new user's password
 */
RESULT<CUser *> CCore::CreateUser(const char *Username, const char *Password) {
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

/**
 * RemoveUser
 *
 * Removes a bouncer user.
 *
 * @param Username the name of the user
 * @param RemoveConfig whether the user's config file should be deleted
 */
RESULT<bool> CCore::RemoveUser(const char *Username, bool RemoveConfig) {
	RESULT<bool> Result;
	CUser *User;
	char *UsernameCopy;
	
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

	UsernameCopy = strdup(User->GetUsername());

	delete User;
	
	Result = m_Users.Remove(Username);

	if (IsError(Result)) {
		free(UsernameCopy);

		THROWRESULT(bool, Result);
	}

	if (UsernameCopy != NULL) {
		Log("User removed: %s", UsernameCopy);
		free(UsernameCopy);
	}

	UpdateUserConfig();

	RETURN(bool, true);
}

/**
 * IsValidUsername
 *
 * Checks whether the specified username is (theoretically) valid.
 *
 * @param Username the username
 */
bool CCore::IsValidUsername(const char *Username) const {
	for (unsigned int i = 0; i < strlen(Username); i++) {
		if (!isalnum(Username[i]) || (i == 0 && isdigit(Username[i]))) {
			return false;
		}
	}

	if (strlen(Username) == 0) {
		return false;
	} else {
		return true;
	}
}

/**
 * UpdateUserConfig
 *
 * Updates the user list in the main config file.
 */
void CCore::UpdateUserConfig(void) {
#define MEMORYBLOCKSIZE 4096
	size_t Size;
	int i;
	char *Out = NULL;
	size_t Blocks = 0, NewBlocks = 1, Length = 1;
	bool WasNull = true;

	i = 0;
	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		Length += strlen(User->Name) + 1;

		NewBlocks += Length / MEMORYBLOCKSIZE;
		Length -= (Length / MEMORYBLOCKSIZE) * MEMORYBLOCKSIZE;

		if (NewBlocks > Blocks) {
			Size = (NewBlocks + 1) * MEMORYBLOCKSIZE;
			Out = (char *)realloc(Out, Size);
		}

		Blocks = NewBlocks;

		if (Out == NULL) {
			LOGERROR("realloc() failed. Userlist in sbnc.conf might be out of date.");

			return;
		}

		if (!WasNull) {
			strlcat(Out, " ", Size);
			strlcat(Out, User->Name, Size);
		} else {
			strlcpy(Out, User->Name, Size);
			WasNull = false;
		}
	}

	if (m_Config != NULL) {
		m_Config->WriteString("system.users", Out);
	}

	free(Out);
}

/**
 * GetStartup
 *
 * Returns the TS when the bouncer was started.
 */
time_t CCore::GetStartup(void) const {
	return m_Startup;
}

/**
 * Daemonize
 *
 * Daemonizes the bouncer.
 */
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
		if (fd != 0) {
			dup2(fd, 0);
		}

		if (fd != 1) {
			dup2(fd, 1);
		}

		if (fd != 2) {
			dup2(fd, 2);
		}

		if (fd > 2) {
			close(fd);
		}
	}

	sid = setsid();
	if (sid == -1) {
		return false;
	}
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

/**
 * WritePidFile
 *
 * Updates the pid-file for the current bouncer instance.
 */
void CCore::WritePidFile(void) const {
#ifndef _WIN32
	pid_t pid = getpid();
#else
	DWORD pid = GetCurrentProcessId();
#endif

	if (pid) {
		FILE *pidFile;

		pidFile = fopen(BuildPath("sbnc.pid"), "w");

		SetPermissions(BuildPath("sbnc.pid"), S_IRUSR | S_IWUSR);

		if (pidFile) {
			fprintf(pidFile, "%d", pid);
			fclose(pidFile);
		}
	}
}

/**
 * MD5
 *
 * Calculates the MD5 hash for a string.
 *
 * @param String the string
 */
const char *CCore::MD5(const char *String) const {
	return UtilMd5(String);
}

/**
 * GetArgC
 *
 * Returns the argc parameter which was passed to main().
 */
int CCore::GetArgC(void) const {
	return m_Args.GetLength();
}

/**
 * GetArgV
 *
 * Returns the argv parameter which was passed to main().
 */
const char *const *CCore::GetArgV(void) const {
	return m_Args.GetList();
}

/**
 * WrapSocket
 *
 * Creates a wrapper object for a socket.
 *
 * @param Socket the socket
 * @param SSL whether the wrapper should use SSL
 * @param Role the role of the socket
 */
CConnection *CCore::WrapSocket(SOCKET Socket, bool SSL, connection_role_e Role) const {
	CConnection *Wrapper = new CConnection(Socket, SSL, Role);

	Wrapper->m_Wrapper = true;

	return Wrapper;
}

/**
 * DeleteWrapper
 *
 * Deletes a socket wrapper.
 *
 * @param Wrapper the wrapper object
 */
void CCore::DeleteWrapper(CConnection *Wrapper) const {
	delete Wrapper;
}

/**
 * IsRegisteredSocket
 *
 * Checks whether an event interface is registered in the socket list.
 *
 * @param Events the event interface
 */
bool CCore::IsRegisteredSocket(CSocketEvents *Events) const {
	for (unsigned int i = 0; i < m_OtherSockets.GetLength(); i++) {
		if (m_OtherSockets[i].Events == Events) {
			return true;
		}
	}

	return false;
}

/**
 * SocketAndConnect
 *
 * Creates a socket and connects to a remote host. This function might block.
 *
 * @param Host the hostname
 * @param Port the port
 * @param BindIp bind address (or NULL)
 */
SOCKET CCore::SocketAndConnect(const char *Host, unsigned short Port, const char *BindIp) {
	return ::SocketAndConnect(Host, Port, BindIp);
}

/**
 * GetSocketByClass
 *
 * Returns a socket object which belongs to a specific class.
 *
 * @param Class class name
 * @param Index index
 */
const socket_t *CCore::GetSocketByClass(const char *Class, int Index) const {
	int a = 0;

	for (unsigned int i = 0; i < m_OtherSockets.GetLength(); i++) {
		socket_t Socket = m_OtherSockets[i];

		if (Socket.Socket == INVALID_SOCKET) {
			continue;
		}

		if (strcmp(Socket.Events->GetClassName(), Class) == 0) {
			a++;
		}

		if (a - 1 == Index) {
			return &m_OtherSockets[i];
		}
	}

	return NULL;
}

/**
 * CreateTimer
 *
 * Creates a timer.
 *
 * @param Interval the interval for the timer
 * @param Repeat whether the timer should repeat itself periodically
 * @param Function the timer function
 * @param Cookie a timer-specific cookie
 */
CTimer *CCore::CreateTimer(unsigned int Interval, bool Repeat, TimerProc Function, void *Cookie) const {
	return new CTimer(Interval, Repeat, Function, Cookie);
}

/**
 * RegisterTimer
 *
 * Registers a timer object.
 *
 * @param Timer the timer
 */
void CCore::RegisterTimer(CTimer *Timer) {
	m_Timers.Insert(Timer);
}

/**
 * UnregisterTimer
 *
 * Unregisters a timer.
 *
 * @param Timer the timer
 */
void CCore::UnregisterTimer(CTimer *Timer) {
	m_Timers.Remove(Timer);
}

/**
 * RegisterDnsQuery
 *
 * Registers a DNS query.
 *
 * @param DnsQuery the DNS query
 */
void CCore::RegisterDnsQuery(CDnsQuery *DnsQuery) {
	m_DnsQueries.Insert(DnsQuery);
}

/**
 * UnregisterDnsQuery
 *
 * Unregisters a DNS query.
 *
 * @param DnsQuery the DNS query
 */
void CCore::UnregisterDnsQuery(CDnsQuery *DnsQuery) {
	m_DnsQueries.Remove(DnsQuery);
}

/**
 * Match
 *
 * Matches a string against a pattern.
 *
 * @param Pattern the pattern
 * @param String the string
 */
bool CCore::Match(const char *Pattern, const char *String) const {
	return (match(Pattern, String) == 0);
}

/**
 * GetSendqSize
 *
 * Returns the sendq size for non-admins.
 */
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

/**
 * SetSendqSize
 *
 * Sets the sendq size.
 *
 * @param NewSize new size of the send queue
 */
void CCore::SetSendqSize(size_t NewSize) {
	m_Config->WriteInteger("system.sendq", NewSize);
	m_SendqSizeCache = NewSize;
}

/**
 * GetMotd
 *
 * Returns the bouncer's motd (or NULL if there isn't any).
 */
const char *CCore::GetMotd(void) const {
	return m_Config->ReadString("system.motd");
}

/**
 * SetMotd
 *
 * Sets the bouncer's motd.
 *
 * @param Motd the new motd
 */
void CCore::SetMotd(const char *Motd) {
	m_Config->WriteString("system.motd", Motd);
}

/**
 * Fatal
 *
 * Logs a fatal error and terminates the bouncer.
 */
void CCore::Fatal(void) {
	Log("Fatal error occured.");

	exit(EXIT_FAILURE);
}

/**
 * GetSSLContext
 *
 * Returns the SSL context for bouncer clients.
 */
SSL_CTX *CCore::GetSSLContext(void) {
	return m_SSLContext;
}

/**
 * GetSSLClientContext
 *
 * Returns the SSL context for IRC connections.
 */
SSL_CTX *CCore::GetSSLClientContext(void) {
	return m_SSLClientContext;
}

/**
 * GetSSLCustomIndex
 *
 * Returns the custom index for SSL.
 */
int CCore::GetSSLCustomIndex(void) const {
#ifdef USESSL
	return g_SSLCustomIndex;
#else
	return 0;
#endif
}

#ifdef USESSL
/**
 * SSLVerifyCertificate
 *
 * Checks whether an SSL certificate is valid.
 *
 * @param preverify_ok was the preverification of the certificate successful
 * @param x509ctx X509 context
 */
int SSLVerifyCertificate(int preverify_ok, X509_STORE_CTX *x509ctx) {
	SSL *ssl = (SSL *)X509_STORE_CTX_get_ex_data(x509ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
	CConnection *Ptr = (CConnection *)SSL_get_ex_data(ssl, g_SSLCustomIndex);

	if (Ptr != NULL) {
		return Ptr->SSLVerify(preverify_ok, x509ctx);
	} else {
		return 0;
	}
}
#endif

/**
 * DebugImpulse
 *
 * Executes a debug command.
 *
 * @param impulse the command
 */
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

			User->SetServer("217.112.85.191");

			if ((rand() + 1) % 2 == 0) {
				User->SetPort(6667);
			} else {
				User->SetPort(6668);
			}

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

	if (impulse == 12) {
		int i = 0;
		hash_t<CUser *> *User;
		static char *Out = NULL;
		unsigned int diff;

		while ((User = g_Bouncer->GetUsers()->Iterate(i++)) != NULL) {
			if (User->Value->GetClientConnection() == NULL && User->Value->GetIRCConnection() != NULL) {
				CIRCConnection *IRC = User->Value->GetIRCConnection();

#ifndef _WIN32
				timeval start, end;

				gettimeofday(&start, NULL);
#else
				DWORD start, end;

				start = GetTickCount();
#endif

#define BENCHMARK_LINES 2000000

				for (int a = 0; a < BENCHMARK_LINES; a++) {
					IRC->ParseLine(":fakeserver.performance-test PRIVMSG #random-channel :abcdefghijklmnopqrstuvwxyz");
				}

#ifndef _WIN32
				gettimeofday(&end, NULL);
				diff = ((end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec) / 1000;
#else
				end = GetTickCount();
				diff = end - start;
#endif

				unsigned int lps = BENCHMARK_LINES / diff;

				free(Out);

				asprintf(&Out, "%d lines parsed in %d msecs, approximately %d lines/msec", BENCHMARK_LINES, diff, lps);

				return Out;
			}
		}
	}

	return NULL;
}

/**
 * Freeze
 *
 * Persists the main bouncer object.
 *
 * @param Box the box
 */
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

		if (IRC != NULL) {
			/* TODO: use FreezeObject<> */
			CAssocArray *IrcBox = Box->Create();

			if (IRC->Freeze(IrcBox)) {
				Box->AddBox(Username, IrcBox);
			} else {
				IrcBox->Destroy();
			}
		}

		if (Client != NULL) {
			CAssocArray *ClientBox;
			CAssocArray *ClientsBox = Box->ReadBox("~clients");

			if (ClientsBox == NULL) {
				ClientsBox = Box->Create();
				Box->AddBox("~clients", ClientsBox);
			}

			ClientBox = Box->Create();
			
			if (Client->Freeze(ClientBox)) {
				ClientsBox->AddBox(Username, ClientBox);
			} else {
				ClientBox->Destroy();
			}
		}

		free(Username);
	}

	delete this;

	return true;
}

/**
 * Thaw
 *
 * Depersists the main bouncer object.
 *
 * @param Box the box
 */
bool CCore::Thaw(CAssocArray *Box) {
	CAssocArray *ClientsBox;

	m_Listener = ThawObject<CClientListener>(Box, "~listener");
	m_ListenerV6 = ThawObject<CClientListener>(Box, "~listenerv6");

	m_SSLListener = ThawObject<CClientListener>(Box, "~ssllistener");
	if (m_SSLListener != NULL) {
		m_SSLListener->SetSSL(true);
	}

	m_SSLListenerV6 = ThawObject<CClientListener>(Box, "~ssllistenerv6");
	if (m_SSLListenerV6 != NULL) {
		m_SSLListenerV6->SetSSL(true);
	}

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
					User->Value->Privmsg("shroudBNC was reloaded.");
				}
			}
		}
	}

	return true;
}

/**
 * InitializeFreeze
 *
 * Prepares the bouncer for reloading itself.
 */
bool CCore::InitializeFreeze(void) {
	if (GetStatus() != STATUS_RUN && GetStatus() != STATUS_PAUSE) {
		return false;
	}

	SetStatus(STATUS_FREEZE);

	return true;
}

/**
 * GetLoaderParameters
 *
 * Returns the loader parameters which were passed to the bouncer.
 */
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

/**
 * MakeConfig
 *
 * Creates a simple configuration file (in case the user doesn't have one yet).
 */
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

/**
 * GetTagString
 *
 * Returns the value of a global tag (as a string).
 *
 * @param Tag name of the tag
 */
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

/**
 * GetTagInteger
 *
 * Returns the value of a global tag (as an integer).
 *
 * @param Tag name of the tag
 */
int CCore::GetTagInteger(const char *Tag) const {
	const char *Value = GetTagString(Tag);

	if (Value != NULL) {
		return atoi(Value);
	} else {
		return 0;
	}
}

/**
 * SetTagString
 *
 * Sets the value of a global tag (as a string).
 *
 * @param Tag the name of the tag
 * @param Value the new value
 */
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

/**
 * SetTagInteger
 *
 * Sets the value of a global tag (as an integer).
 *
 * @param Tag the name of the tag
 * @param Value the new value
 */
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

/**
 * GetTagName
 *
 * Returns the names of current global tags.
 *
 * @param Index the index of the tag which is to be returned
 */
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

/**
 * GetBasePath
 *
 * Returns the bouncer's pathname.
 */
const char *CCore::GetBasePath(void) const {
	return g_LoaderParameters->basepath;
}

/**
 * BuildPath
 *
 * Builds a path which is relative to BasePath or the bouncer's path if
 * BasePath is NULL.
 *
 * @param Filename the filename
 * @param BasePath base path
 */
const char *CCore::BuildPath(const char *Filename, const char *BasePath) const {
	return g_LoaderParameters->BuildPath(Filename, BasePath);
}

/**
 * GetBouncerVersion
 *
 * Returns the bouncer's version.
 */
const char *CCore::GetBouncerVersion(void) const {
	return BNCVERSION;
}

/**
 * SetStatus
 *
 * Sets the bouncer's status code.
 *
 * @param NewStatus the new status code
 */
void CCore::SetStatus(int NewStatus) {
	m_Status = NewStatus;
}

/**
 * GetStatus
 *
 * Returns the bouncer's status code.
 */
int CCore::GetStatus(void) const {
	return m_Status;
}

/**
 * RegisterZone
 *
 * Registers a memory zone.
 *
 * @param ZoneInformation zone information object
 */
void CCore::RegisterZone(CZoneInformation *ZoneInformation) {
	m_Zones.Insert(ZoneInformation);
}

/**
 * GetZones
 *
 * Returns the list of currently used memory allocation zones.
 */
const CVector<CZoneInformation *> *CCore::GetZones(void) const {
	return &m_Zones;
}

/**
 * GetAllocationInformation
 *
 * Returns a list containing information about current memory allocations.
 */
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

/**
 * CreateFakeClient
 *
 * Creates a fake client connection object.
 */
CFakeClient *CCore::CreateFakeClient(void) const {
	return new CFakeClient();
}

/** 
 * DeleteFakeClient
 *
 * Deletes a fake client object.
 *
 * @param FakeClient the object
 */
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

/**
 * GetAdminUsers
 *
 * Returns a list of users who are admins.
 */
CVector<CUser *> *CCore::GetAdminUsers(void) {
	return &m_AdminUsers;
}

/**
 * AddAdditionalListener
 *
 * Creates an additional socket listener.
 *
 * @param Port the port for the listener
 * @param BindAddress bind address (can be NULL)
 * @param SSL whether to use SSL
 */
RESULT<bool> CCore::AddAdditionalListener(unsigned short Port, const char *BindAddress, bool SSL) {
	additionallistener_t AdditionalListener;
	CClientListener *Listener, *ListenerV6;

	for (unsigned int i = 0; i < m_AdditionalListeners.GetLength(); i++) {
		if (m_AdditionalListeners[i].Port == Port) {
			THROW(bool, Generic_Unknown, "This port is already in use.");
		}
	}

	Listener = new CClientListener(Port, BindAddress, AF_INET, SSL);

	if (Listener == NULL || !Listener->IsValid()) {
		delete Listener;

		if (SSL) {
			Log("Failed to create additional SSL listener on port %d.", Port);
			THROW(bool, Generic_OutOfMemory, "Failed to create additional SSL listener on that port.");
		} else {
			Log("Failed to create additional listener on port %d.", Port);
			THROW(bool, Generic_OutOfMemory, "Failed to create additional listener on that port.");
		}
	}

	ListenerV6 = new CClientListener(Port, BindAddress, AF_INET6, SSL);

	if (ListenerV6 == NULL || !ListenerV6->IsValid()) {
		delete ListenerV6;
		ListenerV6 = NULL;
	}

	AdditionalListener.Port = Port;

	if (BindAddress != NULL) {
		AdditionalListener.BindAddress = strdup(BindAddress);
	} else {
		AdditionalListener.BindAddress = NULL;
	}

	AdditionalListener.SSL = SSL;
	AdditionalListener.Listener = Listener;
	AdditionalListener.ListenerV6 = ListenerV6;

	m_AdditionalListeners.Insert(AdditionalListener);

	UpdateAdditionalListeners();

	if (!SSL) {
		Log("Created additional listener on port %d.", Port);
	} else {
		Log("Created additional SSL listener on port %d.", Port);
	}

	RETURN(bool, true);
}

/**
 * RemoveAdditionalListener
 *
 * Removes a listener.
 *
 * @param Port the port of the listener
 */
RESULT<bool> CCore::RemoveAdditionalListener(unsigned short Port) {
	for (unsigned int i = 0; i < m_AdditionalListeners.GetLength(); i++) {
		if (m_AdditionalListeners[i].Port == Port) {
			if (m_AdditionalListeners[i].Listener != NULL) {
				m_AdditionalListeners[i].Listener->Destroy();
			}

			if (m_AdditionalListeners[i].ListenerV6 != NULL) {
				m_AdditionalListeners[i].ListenerV6->Destroy();
			}

			free(m_AdditionalListeners[i].BindAddress);

			RESULT<bool> Result = m_AdditionalListeners.Remove(i);
			THROWIFERROR(bool, Result);

			Log("Removed listener on port %d.", Port);

			UpdateAdditionalListeners();

			RETURN(bool, true);
		}
	}

	RETURN(bool, false);
}

/**
 * GetAdditionalListeners
 *
 * Returns a list of additional listeners.
 */
CVector<additionallistener_t> *CCore::GetAdditionalListeners(void) {
	return &m_AdditionalListeners;
}

/**
 * InitializeAdditionalListeners
 *
 * Initialized the additional listeners.
 */
void CCore::InitializeAdditionalListeners(void) {
	unsigned short Port;
	bool SSL;
	unsigned int i;
	char *Out;

	m_LoadingListeners = true;

	i = 0;
	while (true) {
		asprintf(&Out, "system.listeners.listener%d", i++);

		CHECK_ALLOC_RESULT(Out, asprintf) {
			Fatal();
		} CHECK_ALLOC_RESULT_END;

		const char *ListenerString = m_Config->ReadString(Out);

		free(Out);

		if (ListenerString != NULL) {
			const char *ListenerToks = ArgTokenize(ListenerString);
			const char *PortString = ArgGet(ListenerToks, 1);
			const char *SSLString = ArgGet(ListenerToks, 2);
			const char *Address = NULL;

			SSL = false;

			if (ArgCount(ListenerToks) > 0) {
				Port = atoi(PortString);

				if (ArgCount(ListenerToks) > 1) {
					SSL = (atoi(SSLString) != 0);

					if (ArgCount(ListenerToks) > 2) {
						Address = ArgGet(ListenerToks, 3);
					}
				}
			}

			AddAdditionalListener(Port, Address, SSL);
		} else {
			break;
		}
	}

	m_LoadingListeners = false;
}

/**
 * UninitializeAdditionalListeners
 *
 * Uninitialize the additional listeners.
 */
void CCore::UninitializeAdditionalListeners(void) {
	for (unsigned int i = 0; i < m_AdditionalListeners.GetLength(); i++) {
		if (m_AdditionalListeners[i].Listener != NULL) {
			m_AdditionalListeners[i].Listener->Destroy();
		}

		if (m_AdditionalListeners[i].ListenerV6 != NULL) {
			m_AdditionalListeners[i].ListenerV6->Destroy();
		}

		free(m_AdditionalListeners[i].BindAddress);
	}

	m_AdditionalListeners.Clear();
}

/**
 * UpdateAdditionalListeners
 *
 * Updates the list of additional listeners in the bouncer's
 * main config file.
 */
void CCore::UpdateAdditionalListeners(void) {
	char *Out, *Value;
	int a = 0;

	if (m_LoadingListeners) {
		return;
	}

	for (unsigned int i = 0; i < m_AdditionalListeners.GetLength(); i++) {
		asprintf(&Out, "system.listeners.listener%d", a++);

		CHECK_ALLOC_RESULT(Out, asprintf) {
			Fatal();
		} CHECK_ALLOC_RESULT_END;

		if (m_AdditionalListeners[i].BindAddress != NULL) {
			asprintf(&Value, "%d %d %s", m_AdditionalListeners[i].Port, m_AdditionalListeners[i].SSL, m_AdditionalListeners[i].BindAddress);
		} else {
			asprintf(&Value, "%d %d", m_AdditionalListeners[i].Port, m_AdditionalListeners[i].SSL);
		}

		CHECK_ALLOC_RESULT(Value, asprintf) {
			Fatal();
		} CHECK_ALLOC_RESULT_END;

		m_Config->WriteString(Out, Value);

		free(Out);
	}

	asprintf(&Out, "system.listeners.listener%d", a);

	CHECK_ALLOC_RESULT(Out, asprintf) {
		Fatal();
	} CHECK_ALLOC_RESULT_END;

	m_Config->WriteString(Out, NULL);

	free(Out);
}

/**
 * GetMainListener
 *
 * Returns the main IPv4 listener.
 */
CClientListener *CCore::GetMainListener(void) const {
	return m_Listener;
}

/**
 * GetMainListenerV6
 *
 * Returns the main IPv6 listener.
 */
CClientListener *CCore::GetMainListenerV6(void) const {
	return m_ListenerV6;
}

/**
 * GetMainSSLListener
 *
 * Returns the main IPv4+SSL listener.
 */
CClientListener *CCore::GetMainSSLListener(void) const {
	return m_SSLListener;
}

/**
 * GetMainSSLListenerV6
 *
 * Returns the main IPv6+SSL listener.
 */
CClientListener *CCore::GetMainSSLListenerV6(void) const {
	return m_SSLListenerV6;
}
