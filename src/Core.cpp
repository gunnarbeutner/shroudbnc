/******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2014 Gunnar Beutner                                      *
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

const char *g_ErrorFile; /**< name of the file where the last error occured */
unsigned int g_ErrorLine; /**< line where the last error occurred */
time_t g_CurrentTime; /**< current time (updated in main loop) */

#ifdef HAVE_LIBSSL
int SSLVerifyCertificate(int preverify_ok, X509_STORE_CTX *x509ctx);
int g_SSLCustomIndex; /**< custom SSL index */
#endif

time_t g_LastReconnect = 0; /**< time of the last reconnect */

static struct reslimit_s {
	const char *Resource;
	unsigned int DefaultLimit;
} g_ResourceLimits[] = {
		{ "channels", 50 },
		{ "nicks", 5000 },
		{ "bans", 100 },
		{ "keys", 50 },
		{ "clients", 5 },
		{ NULL, 0 }
	};

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

	m_Log = NULL;

	m_PidFile = NULL;

	WritePidFile();

	m_Config = Config;

	m_SSLContext = NULL;
	m_SSLClientContext = NULL;

	m_Status = Status_Running; 

	CacheInitialize(m_ConfigCache, Config, "system.");

	char *SourcePath = strdup(BuildPathLog("sbnc.log"));
	rename(SourcePath, BuildPathLog("sbnc.log.old"));
	free(SourcePath);

	m_PollFds.Preallocate(SFD_SETSIZE);

	m_Log = new CLog("sbnc.log", true);

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

	m_Config = new CConfig("sbnc.conf", NULL);
	CacheInitialize(m_ConfigCache, m_Config, "system.");

	const char *Users;
	CUser *User;

	if ((Users = m_Config->ReadString("system.users")) == NULL) {
		if (!MakeConfig()) {
			Log("Configuration file could not be created.");

			Fatal();
		}

		printf("Configuration has been successfully saved. Please restart shroudBNC now.\n");
		exit(EXIT_SUCCESS);
	}

	const char *Args;
	int Count;

	Args = ArgTokenize(Users);

	if (AllocFailed(Args)) {
		Fatal();
	}

	Count = ArgCount(Args);

	for (i = 0; i < Count; i++) {
		const char *Name = ArgGet(Args, i + 1);

		User = new CUser(Name);

		if (AllocFailed(User)) {
			Fatal();
		}

		m_Users.Add(Name, User);
	}

	ArgFree(Args);

	m_Listener = NULL;
	m_ListenerV6 = NULL;
	m_SSLListener = NULL;
	m_SSLListenerV6 = NULL;

	time(&m_Startup);

	m_LoadingModules = false;
	m_LoadingListeners = false;

	InitializeSocket();

	m_Capabilities = new CVector<const char *>();
	m_Capabilities->Insert("multi-prefix");
	m_Capabilities->Insert("znc.in/server-time-iso");
}

/**
 * ~CCore
 *
 * Destructs a CCore object.
 */
CCore::~CCore(void) {
	int a, i;

	for (a = m_Modules.GetLength() - 1; a >= 0; a--) {
		delete m_Modules[a];
	}

	m_Modules.Clear();

	UninitializeAdditionalListeners();

	for (CListCursor<socket_t> SocketCursor(&m_OtherSockets); SocketCursor.IsValid(); SocketCursor.Proceed()) {
		if (SocketCursor->PollFd->fd != INVALID_SOCKET) {
			SocketCursor->Events->Destroy();
		}
	}

	i = 0;
	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		delete User->Value;
	}

	CTimer::DestroyAllTimers();

	delete m_Log;
	delete m_Ident;

	g_Bouncer = NULL;

	UnlockPidFile();

	UninitializeSocket();
}

/**
 * StartMainLoop
 *
 * Executes shroudBNC's main loop.
 */
void CCore::StartMainLoop(bool ShouldDaemonize) {
	unsigned int i;

	time(&g_CurrentTime);

	int Port = CacheGetInteger(m_ConfigCache, port);
#ifdef HAVE_LIBSSL
	int SSLPort = CacheGetInteger(m_ConfigCache, sslport);

	if (Port == 0 && SSLPort == 0) {
#else
	if (Port == 0) {
#endif
		Port = 9000;
	}

	const char *BindIp = CacheGetString(m_ConfigCache, ip);

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

#ifdef HAVE_LIBSSL
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

	SSL_METHOD *SSLMethod = (SSL_METHOD *)SSLv23_method();

	m_SSLContext = SSL_CTX_new(SSLMethod);
	SSL_CTX_set_passwd_cb(m_SSLContext);

	m_SSLClientContext = SSL_CTX_new(SSLMethod);
	SSL_CTX_set_passwd_cb(m_SSLClientContext);

	SSL_CTX_set_mode(m_SSLContext, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
	SSL_CTX_set_mode(m_SSLClientContext, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

	g_SSLCustomIndex = SSL_get_ex_new_index(0, (void *)"CConnection*", NULL, NULL, NULL);

	if (!SSL_CTX_use_PrivateKey_file(m_SSLContext, BuildPathConfig("sbnc.key"), SSL_FILETYPE_PEM)) {
		if (SSLPort != 0) {
			Log("Could not load private key (sbnc.key).");
			ERR_print_errors_fp(stdout);
			return;
		} else {
			SSL_CTX_free(m_SSLContext);
			m_SSLContext = NULL;
		}
	} else {
		SSL_CTX_set_verify(m_SSLContext, SSL_VERIFY_PEER, SSLVerifyCertificate);
	}

	if (!SSL_CTX_use_certificate_chain_file(m_SSLContext, BuildPathConfig("sbnc.crt"))) {
		if (SSLPort != 0) {
			Log("Could not load public key (sbnc.crt).");
			ERR_print_errors_fp(stdout);
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

#ifdef HAVE_LIBSSL
	if (SSLPort != 0 && m_SSLListener != NULL && m_SSLListener->IsValid()) {
		Log("Created ssl listener.");
	} else if (SSLPort != 0) {
		Log("Could not create ssl listener port");
		return;
	}
#endif

	InitializeAdditionalListeners();

	Log("Starting main loop.");

	if (ShouldDaemonize) {
#ifndef _WIN32
		fprintf(stderr, "Daemonizing... ");

		UnlockPidFile();

		if (Daemonize()) {
			fprintf(stderr, "DONE\n");
		} else {
			fprintf(stderr, "FAILED\n");
		}

		WritePidFile();
#endif
	}

	/* Note: We need to load the modules after using fork() as otherwise tcl cannot be cleanly unloaded */
	m_LoadingModules = true;

#ifdef _MSC_VER
	LoadModule("bnctcl.dll");
    LoadModule("bncidentd.dll");
#else
	LoadModule("libbnctcl.la");
#endif

	char *Out;

	i = 0;
	while (true) {
		int rc = asprintf(&Out, "system.modules.mod%d", i++);

		if (RcFailed(rc)) {
			Fatal();
		}

		const char *File = m_Config->ReadString(Out);

		free(Out);

		if (File == NULL) {
			break;
		}

		LoadModule(File);
	}

	m_LoadingModules = false;

	i = 0;
	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		User->Value->LoadEvent();
	}

	int m_ShutdownLoop = 5;

	time_t Last = 0;

	while (GetStatus() == Status_Running || --m_ShutdownLoop) {
		time_t Now, Best = 0, SleepInterval = 0;

#if defined(_WIN32) && defined(_DEBUG)
		DWORD TickCount = GetTickCount();
#endif

		time(&Now);

		i = 0;
		while (hash_t<CUser *> *UserHash = m_Users.Iterate(i++)) {
			CIRCConnection *IRC;

			if ((IRC = UserHash->Value->GetIRCConnection()) != NULL) {
				if (GetStatus() != Status_Running) {
					Log("Closing connection for user %s", UserHash->Name);
					IRC->Kill("Shutting down.");

					UserHash->Value->SetIRCConnection(NULL);
				}

				if (IRC->ShouldDestroy()) {
					IRC->Destroy();
				}
			}
		}

		CUser::RescheduleReconnectTimer();

		time(&Now);

		if (g_CurrentTime - 5 > Now) {
			Log("Time warp detected: %d seconds", Now - g_CurrentTime);
		}

		g_CurrentTime = Now;

		Best = CTimer::GetNextCall();

		if (Best <= g_CurrentTime) {
#ifdef _DEBUG
			if (g_CurrentTime - 1 > Best) {
#else
			if (g_CurrentTime - 5 > Best) {
#endif
				Log("Time warp detected: %d seconds", g_CurrentTime - Best);
			}

			CTimer::CallTimers();

			Best = CTimer::GetNextCall();
		}

		SleepInterval = Best - g_CurrentTime;

		DnsSocketCookie *DnsCookie = CDnsQuery::RegisterSockets();

		for (CListCursor<socket_t> SocketCursor(&m_OtherSockets); SocketCursor.IsValid(); SocketCursor.Proceed()) {
			if (SocketCursor->PollFd->fd == INVALID_SOCKET) {
				continue;
			}

			if (SocketCursor->Events->ShouldDestroy()) {
				SocketCursor->Events->Destroy();
			} else {
				SocketCursor->PollFd->events = POLLIN | POLLERR;

				if (SocketCursor->Events->HasQueuedData()) {
					SocketCursor->PollFd->events |= POLLOUT;
				}
			}
		}

		bool ModulesBusy = false;

	        for (int j = 0; j < m_Modules.GetLength(); j++) {
        	        if (m_Modules[j]->MainLoop()) {
                	        ModulesBusy = true;
	                }
	        }

		if (SleepInterval <= 0 || GetStatus() != Status_Running || ModulesBusy) {
			SleepInterval = 1;
		}

		if (GetStatus() != Status_Running && SleepInterval > 3) {
			SleepInterval = 3;
		}

		timeval interval = { (long)SleepInterval, 0 };

		time(&Last);

#ifdef _DEBUG
		//printf("poll: %d seconds\n", SleepInterval);
#endif

#if defined(_WIN32) && defined(_DEBUG)
		DWORD TimeDiff = GetTickCount();
#endif

		int ready = poll(m_PollFds.GetList(), m_PollFds.GetLength(), interval.tv_sec * 1000);

#if defined(_WIN32) && defined(_DEBUG)
		TickCount += GetTickCount() - TimeDiff;
#endif

		time(&g_CurrentTime);

		if (ready > 0) {
			for (CListCursor<socket_t> SocketCursor(&m_OtherSockets); SocketCursor.IsValid(); SocketCursor.Proceed()) {
				pollfd *PollFd = SocketCursor->PollFd;
				CSocketEvents *Events = SocketCursor->Events;

				if (PollFd->fd != INVALID_SOCKET) {
					if (PollFd->revents & (POLLERR|POLLHUP|POLLNVAL)) {
						int ErrorCode;
						socklen_t ErrorCodeLength = sizeof(ErrorCode);

						ErrorCode = 0;

						if (getsockopt(PollFd->fd, SOL_SOCKET, SO_ERROR, (char *)&ErrorCode, &ErrorCodeLength) != -1) {
							if (ErrorCode != 0) {
								Events->Error(ErrorCode);
							}
						}

						if (ErrorCode == 0) {
							Events->Error(-1);
						}

						Events->Destroy();

						continue;
					}

					if (PollFd->revents & (POLLIN|POLLPRI)) {
						int Code;
						if ((Code = Events->Read()) != 0) {
							Events->Error(Code);
							Events->Destroy();

							continue;
						}
					}

					if (PollFd->revents & POLLOUT) {
						Events->Write();
					}
				}
			}
		} else if (ready == -1) {
#ifndef _WIN32
			if (errno != EBADF && errno != 0) {
#else
			if (errno != WSAENOTSOCK) {
#endif
				continue;
			}

			m_OtherSockets.Lock();

			for (CListCursor<socket_t> SocketCursor(&m_OtherSockets); SocketCursor.IsValid(); SocketCursor.Proceed()) {
				if (SocketCursor->PollFd->fd == INVALID_SOCKET) {
					continue;
				}

				pollfd pfd;
				pfd.fd = SocketCursor->PollFd->fd;
				pfd.events = POLLIN | POLLOUT | POLLERR;

				int code = poll(&pfd, 1, 0);

				if (code == -1) {
					SocketCursor->Events->Error(-1);
					SocketCursor->Events->Destroy();
				}
			}
		}

		CDnsQuery::ProcessTimeouts();
		CDnsQuery::UnregisterSockets(DnsCookie);

#if defined(_WIN32) && defined(_DEBUG)
		DWORD Ticks = GetTickCount() - TickCount;

		if (Ticks > 50) {
			printf("Spent %d msec in the main loop.\n", Ticks);
		}
#endif
	}

#ifdef HAVE_LIBSSL
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
	int i = 0;
	char *GlobalText;

	int rc = asprintf(&GlobalText, "Global admin message: %s", Text);

	if (RcFailed(rc)) {
		return;
	}

	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		if (User->Value->GetClientConnectionMultiplexer() != NULL) {
			User->Value->GetClientConnectionMultiplexer()->Privmsg(GlobalText);
		} else {
			User->Value->Log("%s", GlobalText);
		}
	}

	free(GlobalText);
}

/**
 * GetUsers
 *
 * Returns a hashtable which contains all bouncer users.
 */
CHashtable<CUser *, false> *CCore::GetUsers(void) {
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

	if (AllocFailed(Module)) {
		THROW(CModule *, Generic_OutOfMemory, "new operator failed.");
	}

	Result = Module->GetError();

	if (!IsError(Result)) {
		Result = m_Modules.Insert(Module);

		if (IsError(Result)) {
			delete Module;

			Log("Insert() failed. Could not load module");

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

		if (AllocFailed(ErrorString)) {
			delete Module;

			THROW(CModule *, Generic_OutOfMemory, "strdup() failed.");
		}

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
	int a = 0, rc;

	for (int i = 0; i < m_Modules.GetLength(); i++) {
		rc = asprintf(&Out, "system.modules.mod%d", a++);

		if (RcFailed(rc)) {
			Fatal();
		}

		m_Config->WriteString(Out, m_Modules[i]->GetFilename());

		free(Out);
	}

	rc = asprintf(&Out, "system.modules.mod%d", a);

	if (RcFailed(rc)) {
		Fatal();
	}

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
	socket_s SocketStruct;
	pollfd *PollFd = NULL;
	pollfd NewPollFd;
	bool NewStruct = true;
	int i;

	UnregisterSocket(Socket);

	for (i = 0; i < m_PollFds.GetLength(); i++) {
		if (m_PollFds.Get(i).fd == INVALID_SOCKET) {
			PollFd = m_PollFds.GetAddressOf(i);
			NewStruct = false;

			break;
		}
	}

	if (NewStruct) {
		PollFd = &NewPollFd;
	}

	PollFd->fd = Socket;
	PollFd->events = 0;
	PollFd->revents = 0;

	if (NewStruct) {
		if (!m_PollFds.Insert(*PollFd)) {
			Log("RegisterSocket() failed.");

			Fatal();
		}

		PollFd = m_PollFds.GetAddressOf(m_PollFds.GetLength() - 1);
	}

	// This relies on Preallocate() to be called for m_PollFds - otherwise
	// the underlying address for PollFd might change when Insert() is called
	// later on
	SocketStruct.PollFd = PollFd;
	SocketStruct.Events = EventInterface;

	/* TODO: can we safely recover from this situation? return value maybe? */
	if (!m_OtherSockets.Insert(SocketStruct)) {
		Log("Insert() failed.");

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
	for (CListCursor<socket_t> SocketCursor(&m_OtherSockets); SocketCursor.IsValid(); SocketCursor.Proceed()) {
		if (SocketCursor->PollFd->fd == Socket) {
			SocketCursor->PollFd->fd = INVALID_SOCKET;
			SocketCursor->PollFd->events = 0;

			SocketCursor.Remove();

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
SOCKET CCore::CreateListener(unsigned int Port, const char *BindIp, int Family) const {
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

	if (AllocFailed(Out)) {
		return;
	}

	if (m_Log == NULL) {
		fprintf(stderr, "%s\n", Out);
	} else {
		m_Log->WriteLine("%s", Out);
	}

	for (int i = 0; i < m_AdminUsers.GetLength(); i++) {
		CUser *User = m_AdminUsers.Get(i);

		if (User->GetSystemNotices() && User->GetClientConnectionMultiplexer() != NULL) {
			User->GetClientConnectionMultiplexer()->Privmsg(Out);
		}
	}

	free(Out);
}

/**
 * LogUser
 *
 * Logs something in the bouncer's main log and also sends it to a specific user.
 *
 * @param User the user
 * @param Format a format string
 * @param ... additional parameters which are used by the format string
 */
void CCore::LogUser(CUser *User, const char *Format, ...) {
	char *Out;
	int Ret;
	va_list marker;
	bool DoneUser = false;

	va_start(marker, Format);
	Ret = vasprintf(&Out, Format, marker);
	va_end(marker);

	if (AllocFailed(Out)) {
		return;
	}

	m_Log->WriteLine("%s", Out);

	for (int i = 0; i < m_AdminUsers.GetLength(); i++) {
		CUser *ThisUser = m_AdminUsers[i];

		if (ThisUser->GetSystemNotices() && ThisUser->GetClientConnectionMultiplexer()) {
			ThisUser->GetClientConnectionMultiplexer()->Privmsg(Out);

			if (ThisUser == User) {
				DoneUser = true;
			}
		}
	}

	if (!DoneUser && User->GetClientConnectionMultiplexer() != NULL) {
		User->GetClientConnectionMultiplexer()->Privmsg(Out);
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
	char *Format2;
	char *Out;
	const char *P = g_ErrorFile;
	va_list marker;
	int rc;

	while (*P++) {
		if (*P == '\\') {
			g_ErrorFile = P + 1;
		}
	}

	rc= asprintf(&Format2, "Error (in %s:%d): %s", g_ErrorFile, g_ErrorLine, Format);

	if (rc < 0) {
		perror("asprintf failed");

		return;
	}

	va_start(marker, Format);
	rc = vasprintf(&Out, Format2, marker);
	va_end(marker);

	free(Format2);

	if (rc < 0) {
		perror("vasprintf failed");

		return;
	}

	m_Log->WriteUnformattedLine(Out);

	free(Out);
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

	SetStatus(Status_Shutdown);
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

	for (int i = 0; i < m_Modules.GetLength(); i++) {
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
	char *ConfigCopy = NULL, *LogCopy = NULL;
	
	User = GetUser(Username);

	if (User == NULL) {
		THROW(bool, Generic_Unknown, "There is no such user.");
	}

	for (int i = 0; i < m_Modules.GetLength(); i++) {
		m_Modules[i]->UserDelete(Username);
	}

	UsernameCopy = strdup(User->GetUsername());

	if (RemoveConfig) {
		ConfigCopy = strdup(User->GetConfig()->GetFilename());
		LogCopy = strdup(User->GetLog()->GetFilename());
	}

	delete User;

	Result = m_Users.Remove(UsernameCopy);

	if (IsError(Result)) {
		free(UsernameCopy);
		free(ConfigCopy);
		free(LogCopy);

		THROWRESULT(bool, Result);
	}

	if (UsernameCopy != NULL) {
		Log("User removed: %s", UsernameCopy);
		free(UsernameCopy);
	}

	if (RemoveConfig) {
		unlink(ConfigCopy);
		unlink(LogCopy);
	}

	free(ConfigCopy);
	free(LogCopy);

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
	for (size_t i = 0; i < strlen(Username); i++) {
		if (i != 0 && (Username[i] == '-' || Username[i] == '_')) {
			continue;
		}

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
	size_t Size = 0;
	int i;
	char *Out = NULL;
	size_t Blocks = 0, NewBlocks = 1, Length = 1;
	size_t Offset = 0, NameLength;
	bool WasNull = true;

	i = 0;
	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		NameLength = strlen(User->Name);
		Length += NameLength + 1;

		NewBlocks += Length / MEMORYBLOCKSIZE;
		Length -= (Length / MEMORYBLOCKSIZE) * MEMORYBLOCKSIZE;

		if (NewBlocks > Blocks) {
			Size = (NewBlocks + 1) * MEMORYBLOCKSIZE;
			Out = (char *)realloc(Out, Size);
		}

		Blocks = NewBlocks;

		if (AllocFailed(Out)) {
			Log("Userlist in sbnc.conf might be out of date.");

			return;
		}

		if (!WasNull) {
			Out[Offset] = ' ';
			Offset++;
		} else {
			WasNull = false;
		}

		strmcpy(Out + Offset, User->Name, Size - Offset);
		Offset += NameLength;
	}

	if (m_Config != NULL) {
		CacheSetString(m_ConfigCache, users, Out);
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
 * UnlockPidFile
 *
 * Closes the PID file, thereby releasing the lock we held on it.
 */
void CCore::UnlockPidFile(void) {
	if (m_PidFile) {
		fclose(m_PidFile);
		m_PidFile = NULL;
	}
}

/**
 * WritePidFile
 *
 * Updates the pid-file for the current bouncer instance.
 */
void CCore::WritePidFile(void) {
#ifndef _WIN32
	pid_t pid = getpid();
#else
	DWORD pid = GetCurrentProcessId();
#endif

	UnlockPidFile();

	m_PidFile = fopen(sbncGetPidPath(), "w");

	SetPermissions(sbncGetPidPath(), S_IRUSR | S_IWUSR);

	if (m_PidFile) {
#ifndef _WIN32
		if (flock(fileno(m_PidFile), LOCK_EX | LOCK_NB) < 0) {
			Log("This config directory is locked by another shroudBNC instance. Remove "
				"the 'sbnc.pid' file if you're certain that no other currently running "
				"instance of shroudBNC is accessing this config directory.");
			Fatal();
		}
#endif

		fprintf(m_PidFile, "%d", pid);
		fflush(m_PidFile);
	} else {
		Log("Could not open 'sbnc.pid' file.");
		Fatal();
	}
}

/**
 * MD5
 *
 * Calculates the MD5 hash for a string.
 *
 * @param String the string
 */
const char *CCore::MD5(const char *String, const char *Salt) const {
	return UtilMd5(String, Salt);
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
 * IsRegisteredSocket
 *
 * Checks whether an event interface is registered in the socket list.
 *
 * @param Events the event interface
 */
bool CCore::IsRegisteredSocket(CSocketEvents *Events) const {
	link_t<socket_t> *Current = m_OtherSockets.GetHead();

	while (Current != NULL) {
		if (Current->Value.Events == Events) {
			return true;
		}

		Current = Current->Next;
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
SOCKET CCore::SocketAndConnect(const char *Host, unsigned int Port, const char *BindIp) {
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

	for (CListCursor<socket_t> SocketCursor(&m_OtherSockets); SocketCursor.IsValid(); SocketCursor.Proceed()) {
		if ((*SocketCursor).PollFd->fd == INVALID_SOCKET) {
			continue;
		}

		if (strcmp((*SocketCursor).Events->GetClassName(), Class) == 0) {
			a++;
		}

		if (a - 1 == Index) {
			return &(*SocketCursor);
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
	int Size = CacheGetInteger(m_ConfigCache, sendq);

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
	CacheSetInteger(m_ConfigCache, sendq, NewSize);
}

/**
 * GetMotd
 *
 * Returns the bouncer's motd (or NULL if there isn't any).
 */
const char *CCore::GetMotd(void) const {
	return CacheGetString(m_ConfigCache, motd);
}

/**
 * SetMotd
 *
 * Sets the bouncer's motd.
 *
 * @param Motd the new motd
 */
void CCore::SetMotd(const char *Motd) {
	CacheSetString(m_ConfigCache, motd, Motd);
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
#ifdef HAVE_LIBSSL
	return g_SSLCustomIndex;
#else
	return 0;
#endif
}

#ifdef HAVE_LIBSSL
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
	if (impulse == 6) {
		int a = 23, b = 0, c;
		
		c = a / b;
	}

	if (impulse == 7) {
		_exit(0);
	}

	if (impulse == 12) {
		int i = 0;
		hash_t<CUser *> *User;
		static char *Out = NULL;
		unsigned int diff;

		while ((User = g_Bouncer->GetUsers()->Iterate(i++)) != NULL) {
			if (User->Value->GetClientConnectionMultiplexer() == NULL && User->Value->GetIRCConnection() != NULL) {
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

				int rc = asprintf(&Out, "%d lines parsed in %d msecs, approximately %d lines/msec", BENCHMARK_LINES, diff, lps);

				if (RcFailed(rc)) {}

				return Out;
			}
		}
	}

	return NULL;
}

/**
 * MakeConfig
 *
 * Creates a simple configuration file (in case the user doesn't have one yet).
 */
bool CCore::MakeConfig(void) {
	int Port;
	char Buffer[30];
	char User[81], Password[81], PasswordConfirm[81];
	char *File;
	CConfig *MainConfig, *UserConfig;

	printf("No valid configuration file has been found. A basic\n"
		"configuration file can be created for you automatically. Please\n"
		"answer the following questions:\n");

	while (true) {
		printf("1. Which port should the bouncer listen on (valid ports are in the range 1025 - 65535): ");
		Buffer[0] = '\0';
		sn_getline(Buffer, sizeof(Buffer));
		Port = atoi(Buffer);

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
		User[0] = '\0';
		sn_getline(User, sizeof(User));
	
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

		Password[0] = '\0';
		sn_getline_passwd(Password, sizeof(Password));

		if (strlen(Password) == 0) {
			return false;
		}

		printf("\n4. Please confirm your password by typing it again: ");

		PasswordConfirm[0] = '\0';
		sn_getline_passwd(PasswordConfirm, sizeof(PasswordConfirm));

		printf("\n");

		if (strcmp(Password, PasswordConfirm) == 0) {
			break;
		} else {
			printf("The passwords you entered do not match. Please try again.\n");
		}
	}

	int rc = asprintf(&File, "users/%s.conf", User);

	if (RcFailed(rc)) {
		Fatal();
	}

	// BuildPathConfig is using a static buffer
	mkdir(BuildPathConfig("users"));
	SetPermissions(BuildPathConfig("users"), S_IRUSR | S_IWUSR | S_IXUSR);

	mkdir(BuildPathLog("users"));
	SetPermissions(BuildPathLog("users"), S_IRUSR | S_IWUSR | S_IXUSR);

	mkdir(BuildPathData("users"));
	SetPermissions(BuildPathData("users"), S_IRUSR | S_IWUSR | S_IXUSR);

	MainConfig = new CConfig("sbnc.conf", NULL);

	MainConfig->WriteInteger("system.port", Port);
	MainConfig->WriteInteger("system.md5", 1);
	MainConfig->WriteString("system.users", User);

	printf("Writing main configuration file...");

	MainConfig->Destroy();

	printf(" DONE\n");

	UserConfig = new CConfig(File, NULL);

	UserConfig->WriteString("user.password", UtilMd5(Password, GenerateSalt()));
	UserConfig->WriteInteger("user.admin", 1);

	printf("Writing first user's configuration file...");

	UserConfig->Destroy();

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

	int rc = asprintf(&Setting, "tag.%s", Tag);

	if (RcFailed(rc)) {
		return NULL;
	}

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

	int rc = asprintf(&Setting, "tag.%s", Tag);

	if (RcFailed(rc)) {
		return false;
	}

	for (int i = 0; i < m_Modules.GetLength(); i++) {
		m_Modules[i]->TagModified(Tag, Value);
	}

	if (Value != NULL && Value[0] == '\0') {
		Value = NULL;
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

	if (Value == 0) {
		StringValue = NULL;
	} else {
		int rc = asprintf(&StringValue, "%d", Value);

		if (RcFailed(rc)) {
			return false;
		}
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
 * BuildPathConfig
 *
 * Builds a path which is relative to the config directory.
 *
 * @param Filename the filename
 */
const char *CCore::BuildPathConfig(const char *Filename) const {
	return sbncBuildPath(Filename, sbncGetConfigPath());
}

/**
 * BuildPathLog
 *
 * Builds a path which is relative to the log directory.
 *
 * @param Filename the filename
 */
const char *CCore::BuildPathLog(const char *Filename) const {
	return sbncBuildPath(Filename, sbncGetLogPath());
}

/**
 * BuildPathConfig
 *
 * Builds a path which is relative to the config directory.
 *
 * @param Filename the filename
 */
const char *CCore::BuildPathData(const char *Filename) const {
        return sbncBuildPath(Filename, sbncGetDataPath());
}

/**
 * BuildPathExe
 *
 * Builds a path which is relative to the executable file's directory.
 *
 * @param Filename the filename
 */
const char *CCore::BuildPathExe(const char *Filename) const {
	return sbncBuildPath(Filename, sbncGetExePath());
}

/**
 * BuildPathModule
 *
 * Builds a path which is relative to the module directory.
 *
 * @param Filename the filename
 */
const char *CCore::BuildPathModule(const char *Filename) const {
	return sbncBuildPath(Filename, sbncGetModulePath());
}

/**
 * BuildPathShared
 *
 * Builds a path which is relative to the shared directory.
 *
 * @param Filename the filename
 */
const char *CCore::BuildPathShared(const char *Filename) const {
	return sbncBuildPath(Filename, sbncGetSharedPath());
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
void CCore::SetStatus(sbnc_status_t NewStatus) {
	m_Status = NewStatus;
}

/**
 * GetStatus
 *
 * Returns the bouncer's status code.
 */
sbnc_status_t CCore::GetStatus(void) const {
	return m_Status;
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
RESULT<bool> CCore::AddAdditionalListener(unsigned int Port, const char *BindAddress, bool SSL) {
	additionallistener_t AdditionalListener;
	CClientListener *Listener, *ListenerV6;

	for (int i = 0; i < m_AdditionalListeners.GetLength(); i++) {
		if (m_AdditionalListeners[i].Port == Port) {
			THROW(bool, Generic_Unknown, "This port is already in use.");
		}
	}

	if (GetSSLContext() == NULL) {
		THROW(bool, Generic_Unknown, "Failed to create an SSL listener because there is no SSL server certificate.");
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
RESULT<bool> CCore::RemoveAdditionalListener(unsigned int Port) {
	for (int i = 0; i < m_AdditionalListeners.GetLength(); i++) {
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
	int i;
	char *Out;

	m_LoadingListeners = true;

	i = 0;
	while (true) {
		int rc = asprintf(&Out, "system.listeners.listener%d", i++);

		if (RcFailed(rc)) {
			Fatal();
		}

		const char *ListenerString = m_Config->ReadString(Out);

		free(Out);

		if (ListenerString != NULL) {
			unsigned int Port;
			bool SSL;
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

				AddAdditionalListener(Port, Address, SSL);
			}

			ArgFree(ListenerToks);
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
	for (int i = 0; i < m_AdditionalListeners.GetLength(); i++) {
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
	int a = 0, rc;

	if (m_LoadingListeners) {
		return;
	}

	for (int i = 0; i < m_AdditionalListeners.GetLength(); i++) {
		rc = asprintf(&Out, "system.listeners.listener%d", a++);

		if (RcFailed(rc)) {
			Fatal();
		}

		if (m_AdditionalListeners[i].BindAddress != NULL) {
			rc = asprintf(&Value, "%d %d %s", m_AdditionalListeners[i].Port, m_AdditionalListeners[i].SSL, m_AdditionalListeners[i].BindAddress);
		} else {
			rc = asprintf(&Value, "%d %d", m_AdditionalListeners[i].Port, m_AdditionalListeners[i].SSL);
		}

		if (RcFailed(rc)) {
			Fatal();
		}

		m_Config->WriteString(Out, Value);

		free(Value);
		free(Out);
	}

	rc = asprintf(&Out, "system.listeners.listener%d", a);

	if (RcFailed(rc)) {
		Fatal();
	}

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

int CCore::GetResourceLimit(const char *Resource, CUser *User) {
	int i = 0, rc;

	if (Resource == NULL || (User != NULL && User->IsAdmin())) {
		if (Resource != NULL && strcasecmp(Resource, "clients") == 0) {
			return 15;
		}

		return INT_MAX;
	}

	while (g_ResourceLimits[i].Resource != NULL) {
		if (strcasecmp(g_ResourceLimits[i].Resource, Resource) == 0) {
			char *Name;

			if (User != NULL) {
				rc = asprintf(&Name, "user.max%s", Resource);

				if (!RcFailed(rc)) {
					CResult<int> UserLimit = User->GetConfig()->ReadInteger(Name);

					if (!IsError(UserLimit)) {
						return UserLimit;
					}

					free(Name);
				}
			}

			rc = asprintf(&Name, "system.max%s", Resource);

			if (RcFailed(rc)) {
				return g_ResourceLimits[i].DefaultLimit;
			}

			int Value = m_Config->ReadInteger(Name);

			free(Name);

			if (Value == 0) {
				return g_ResourceLimits[i].DefaultLimit;
			} else if (Value == -1) {
				return INT_MAX;
			} else {
				return Value;
			}
		}

		i++;
	}

	return 0;
}

void CCore::SetResourceLimit(const char *Resource, int Limit, CUser *User) {
	char *Name;
	CConfig *Config;
	int rc;

	if (User != NULL) {
		rc = asprintf(&Name, "user.max%s", Resource);
		Config = User->GetConfig();
	} else {
		rc = asprintf(&Name, "system.max%s", Resource);
		Config = GetConfig();
	}

	if (RcFailed(rc)) {
		return;
	}

	Config->WriteInteger(Name, Limit);
}

int CCore::GetInterval(void) const {
	return CacheGetInteger(m_ConfigCache, interval);
}


void CCore::SetInterval(int Interval) {
	CacheSetInteger(m_ConfigCache, interval, Interval);
}

bool CCore::GetMD5(void) const {
	if (CacheGetInteger(m_ConfigCache, md5) != 0) {
		return true;
	} else {
		return false;
	}
}

void CCore::SetMD5(bool MD5Flag) {
	CacheSetInteger(m_ConfigCache, md5, MD5Flag ? 1 : 0);
}

const char *CCore::GetDefaultVHost(void) const {
	return CacheGetString(m_ConfigCache, vhost);
}

void CCore::SetDefaultVHost(const char *VHost) {
	CacheSetString(m_ConfigCache, vhost, VHost);
}

bool CCore::GetDontMatchUser(void) const {
	if (CacheGetInteger(m_ConfigCache, dontmatchuser)) {
		return true;
	} else {
		return false;
	}
}

void CCore::SetDontMatchUser(bool Value) {
	CacheSetInteger(m_ConfigCache, dontmatchuser, Value ? 1 : 0);
}

CACHE(System) *CCore::GetConfigCache(void) {
	return &m_ConfigCache;
}

bool CCore::Daemonize(void) {
#ifndef _WIN32
	pid_t pid;
	pid_t sid;
	int fd;

	pid = fork();
	if (pid == -1) {
		return false;
	}

	if (pid) {
		fprintf(stdout, "DONE\n");
		exit(0);
	}

	fd = open("/dev/null", O_RDWR);
	if (fd >= 0) {
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
#endif

	return true;
}

void CCore::InitializeSocket(void) {
#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(1, 1), &wsaData);
#endif /* _WIN32 */

#ifdef CARES_HAVE_ARES_LIBRARY_INIT
	ares_library_init(ARES_LIB_INIT_ALL);
#endif /* CARES_HAVE_ARES_LIBRARY_INIT */
}

void CCore::UninitializeSocket(void) {
#ifdef CARES_HAVE_ARES_LIBRARY_CLEANUP
	ares_library_cleanup();
#endif /* CARES_HAVE_ARES_LIBRARY_CLEANUP */

#ifdef _WIN32
	WSACleanup();
#endif /* _WIN32 */
}

CVector<const char *> *CCore::GetCapabilities(void) {
	return m_Capabilities;
}
