/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007 Gunnar Beutner                                      *
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

const char *g_ErrorFile; /**< name of the file where the last error occured */
unsigned int g_ErrorLine; /**< line where the last error occurred */
time_t g_CurrentTime; /**< current time (updated in main loop) */

#ifdef USESSL
int SSLVerifyCertificate(int preverify_ok, X509_STORE_CTX *x509ctx);
int g_SSLCustomIndex; /**< custom SSL index */
#endif

time_t g_LastReconnect = 0; /**< time of the last reconnect */

static struct reslimit_s {
	const char *Resource;
	unsigned int DefaultLimit;
} g_ResourceLimits[] = {
		{ "memory", 10 * 1024 * 1024 },
		{ "channels", 50 },
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
CCore::CCore(CConfig *Config, int argc, char **argv, bool Daemonized) {
	int i;
	char *Out;
	const char *Hostmask;

	m_Daemonized = Daemonized;

	m_OriginalConfig = Config;

	m_SSLContext = NULL;

	m_Status = STATUS_RUN;

	CacheInitialize(m_ConfigCache, Config, "system.");

	char *SourcePath = strdup(BuildPath("sbnc.log"));
	rename(SourcePath, BuildPath("sbnc.log.old"));
	free(SourcePath);

	m_PollFds.Preallocate(SFD_SETSIZE);

	m_Log = new CLog("sbnc.log", true);

	if (m_Log == NULL) {
		safe_printf("Log system could not be initialized. Shutting down.");

		exit(EXIT_FAILURE);
	}

	m_Log->Clear();
	Log("Log system initialized.");

	g_Bouncer = this;

	m_Config = Config;

	m_Args.SetList(argv, argc);

	m_Ident = new CIdentSupport();

	const char *ConfigModule = m_Config->ReadString("system.configmodule");

	m_ConfigModule = new CConfigModule(ConfigModule);

	CResult<bool> Error = m_ConfigModule->GetError();

	if (IsError(Error)) {
		LOGERROR(GETDESCRIPTION(Error));

		Fatal();
	}

	m_ConfigModule->Init(this);

	m_Config = m_ConfigModule->CreateConfigObject("sbnc.conf", NULL);
	CacheInitialize(m_ConfigCache, m_Config, "system.");

	const char *Users;
	CUser *User;

	if ((Users = m_Config->ReadString("system.users")) == NULL) {
		if (IsDaemonized()) {
			LOGERROR("Configuration file does not contain any users. please re-run shroudBNC with --foreground");

			Fatal();
		}

		if (!MakeConfig()) {
			LOGERROR("Configuration file could not be created.");

			Fatal();
		}

		safe_print("Configuration has been successfully saved. Please restart shroudBNC now.\n");
		safe_exit(0);
	}

	const char *Args;
	int Count;

	Args = ArgTokenize(Users);

	CHECK_ALLOC_RESULT(Args, ArgTokenize) {
		Fatal();
	} CHECK_ALLOC_RESULT_END;

	Count = ArgCount(Args);

	safe_box_t UsersBox = safe_put_box(NULL, "Users");

	for (i = 0; i < Count; i++) {
		safe_box_t UserBox = NULL;
		const char *Name = ArgGet(Args, i + 1);

		if (UsersBox != NULL) {
			UserBox = safe_put_box(UsersBox, Name);
		}

		User = new CUser(Name, UserBox);

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

	for (CListCursor<socket_t> SocketCursor(&m_OtherSockets); SocketCursor.IsValid(); SocketCursor.Proceed()) {
		if (SocketCursor->PollFd->fd != INVALID_SOCKET) {
			SocketCursor->Events->Destroy();
		}
	}

	i = 0;
	while (hash_t<CUser *> *User = m_Users.Iterate(i++)) {
		delete User->Value;
	}

	if (m_OriginalConfig != m_Config) {
		m_Config->Destroy();
	}

	delete m_ConfigModule;

	CTimer::DestroyAllTimers();

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

	time(&g_CurrentTime);

	int argc = m_Args.GetLength();
	char **argv = m_Args.GetList();

	int Port = CacheGetInteger(m_ConfigCache, port);
#ifdef USESSL
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
			safe_box_t ListenerBox;

			ListenerBox = safe_put_box(NULL, "Listener");

			m_Listener = new CClientListener(Port, ListenerBox, BindIp, AF_INET);
		} else {
			m_Listener = NULL;
		}
	}

	if (m_ListenerV6 == NULL) {
		if (Port != 0) {
			safe_box_t ListenerBox;

			ListenerBox = safe_put_box(NULL, "ListenerV6");

			m_ListenerV6 = new CClientListener(Port, ListenerBox, BindIp, AF_INET6);

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
			safe_box_t ListenerBox;

			ListenerBox = safe_put_box(NULL, "ListenerSSL");

			m_SSLListener = new CClientListener(SSLPort, ListenerBox, BindIp, AF_INET, true);
		} else {
			m_SSLListener = NULL;
		}
	}

	if (m_SSLListenerV6 == NULL) {
		if (SSLPort != 0) {
			safe_box_t ListenerBox;

			ListenerBox = safe_put_box(NULL, "ListenerSSLV6");

			m_SSLListenerV6 = new CClientListener(SSLPort, ListenerBox, BindIp, AF_INET6, true);

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
	SSL_CTX_set_safe_passwd_cb(m_SSLContext);

	m_SSLClientContext = SSL_CTX_new(SSLMethod);
	SSL_CTX_set_safe_passwd_cb(m_SSLClientContext);

	SSL_CTX_set_mode(m_SSLContext, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
	SSL_CTX_set_mode(m_SSLClientContext, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

	g_SSLCustomIndex = SSL_get_ex_new_index(0, (void *)"CConnection*", NULL, NULL, NULL);

	if (!SSL_CTX_use_PrivateKey_file(m_SSLContext, BuildPath("sbnc.key"), SSL_FILETYPE_PEM)) {
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

	if (!SSL_CTX_use_certificate_chain_file(m_SSLContext, BuildPath("sbnc.crt"))) {
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

#ifdef USESSL
	if (SSLPort != 0 && m_SSLListener != NULL && m_SSLListener->IsValid()) {
		Log("Created ssl listener.");
	} else if (SSLPort != 0) {
		Log("Could not create ssl listener port");
		return;
	}
#endif

	InitializeAdditionalListeners();

	Log("Starting main loop.");

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

	time_t Last = 0;
	time_t LastCheck = 0;

	while (GetStatus() == STATUS_RUN || GetStatus() == STATUS_PAUSE || --m_ShutdownLoop) {
		time_t Now, Best = 0, SleepInterval = 0;

#if defined(_WIN32) && defined(_DEBUG)
		DWORD TickCount = GetTickCount();
#endif

		time(&Now);

		i = 0;
		while (hash_t<CUser *> *UserHash = m_Users.Iterate(i++)) {
			CIRCConnection *IRC;

			if ((IRC = UserHash->Value->GetIRCConnection()) != NULL) {
				if (GetStatus() != STATUS_RUN && GetStatus() != STATUS_PAUSE && IRC->IsLocked() == false) {
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

		if (SleepInterval <= 0 || (GetStatus() != STATUS_RUN && GetStatus() != STATUS_PAUSE)) {
			SleepInterval = 1;
		}

		timeval interval = { SleepInterval, 0 };

		if (GetStatus() != STATUS_RUN && GetStatus() != STATUS_PAUSE && SleepInterval > 3) {
			interval.tv_sec = 3;
		}

		for (unsigned int i = 0; i < m_DnsQueries.GetLength(); i++) {
			ares_channel Channel = m_DnsQueries[i]->GetChannel();

			if (Channel != NULL) {
				ares_fds(Channel);
				ares_timeout(Channel, NULL, &interval);
			}
		}

		time(&Last);

#ifdef _DEBUG
		//safe_printf("poll: %d seconds\n", SleepInterval);
#endif

#if defined(_WIN32) && defined(_DEBUG)
		DWORD TimeDiff = GetTickCount();
#endif

		int ready = safe_poll(m_PollFds.GetList(), m_PollFds.GetLength(), interval.tv_sec * 1000);

#if defined(_WIN32) && defined(_DEBUG)
		TickCount += GetTickCount() - TimeDiff;
#endif

		time(&g_CurrentTime);

		for (unsigned int i = 0; i < m_DnsQueries.GetLength(); i++) {
			ares_channel Channel = m_DnsQueries[i]->GetChannel();

			ares_process(Channel);

			m_DnsQueries[i]->Cleanup();
		}

		if (ready > 0) {
			for (CListCursor<socket_t> SocketCursor(&m_OtherSockets); SocketCursor.IsValid(); SocketCursor.Proceed()) {
				pollfd *PollFd = SocketCursor->PollFd;
				CSocketEvents *Events = SocketCursor->Events;

				if (PollFd->fd != INVALID_SOCKET) {
					if (PollFd->revents & (POLLERR|POLLHUP|POLLNVAL)) {
						int ErrorCode;
						socklen_t ErrorCodeLength = sizeof(ErrorCode);

						ErrorCode = 0;

						if (safe_getsockopt(PollFd->fd, SOL_SOCKET, SO_ERROR, (char *)&ErrorCode, &ErrorCodeLength) != -1) {
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
			if (safe_errno() != EBADF && safe_errno() != 0) {
#else
			if (safe_errno() != WSAENOTSOCK) {
#endif
				continue;
			}

			link_t<socket_t> *Current = m_OtherSockets.GetHead();

			m_OtherSockets.Lock();

			for (CListCursor<socket_t> SocketCursor(&m_OtherSockets); SocketCursor.IsValid(); SocketCursor.Proceed()) {
				if (SocketCursor->PollFd->fd == INVALID_SOCKET) {
					continue;
				}

				pollfd pfd;
				pfd.fd = SocketCursor->PollFd->fd;
				pfd.events = POLLIN | POLLOUT | POLLERR;

				int code = safe_poll(&pfd, 1, 0);

				if (code == -1) {
					SocketCursor->Events->Error(-1);
					SocketCursor->Events->Destroy();
				}
			}
		}

#if defined(_WIN32) && defined(_DEBUG)
		DWORD Ticks = GetTickCount() - TickCount;

		if (Ticks > 50) {
			safe_printf("Spent %d msec in the main loop.\n", Ticks);
		}
#endif
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
		if (User->Value->GetClientConnectionMultiplexer() != NULL) {
			User->Value->GetClientConnectionMultiplexer()->Privmsg(Text);
		}
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
	socket_s SocketStruct;
	pollfd *PollFd = NULL;

	UnregisterSocket(Socket);

	PollFd = registersocket(Socket);

	if (PollFd == NULL) {
		LOGERROR("registersocket() failed.");

		Fatal();
	}

	SocketStruct.PollFd = PollFd;
	SocketStruct.Events = EventInterface;

	/* TODO: can we safely recover from this situation? return value maybe? */
	if (!m_OtherSockets.Insert(SocketStruct)) {
		LOGERROR("Insert() failed.");

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
	link_t<socket_t> *Current = m_OtherSockets.GetHead();

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

	CHECK_ALLOC_RESULT(Out, vasprintf) {
		return;
	} CHECK_ALLOC_RESULT_END;

	m_Log->WriteLine(NULL, "%s", Out);

	for (unsigned int i = 0; i < m_AdminUsers.GetLength(); i++) {
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
	char Format2[512];
	char *Out;
	const char *P = g_ErrorFile;
	va_list marker;

	while (*P++) {
		if (*P == '\\') {
			g_ErrorFile = P + 1;
		}
	}

	snprintf(Format2, sizeof(Format2), "Error (in %s:%d): %s", g_ErrorFile, g_ErrorLine, Format);

	va_start(marker, Format);
	vasprintf(&Out, Format2, marker);
	va_end(marker);

	CHECK_ALLOC_RESULT(Out, vasnprintf) {
		return;
	} CHECK_ALLOC_RESULT_END;

	m_Log->WriteUnformattedLine(NULL, Out);

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

	safe_box_t UsersBox, UserBox = NULL;

	UsersBox = safe_get_box(NULL, "Users");

	if (UsersBox != NULL) {
		UserBox = safe_put_box(UsersBox, Username);
	}

	User = new CUser(Username, UserBox);

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
	char *ConfigCopy = NULL, *LogCopy = NULL;
	
	User = GetUser(Username);

	if (User == NULL) {
		THROW(bool, Generic_Unknown, "There is no such user.");
	}

	for (unsigned int i = 0; i < m_Modules.GetLength(); i++) {
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
	for (unsigned int i = 0; i < strlen(Username); i++) {
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
	size_t Size;
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

		if (Out == NULL) {
			LOGERROR("realloc() failed. Userlist in sbnc.conf might be out of date.");

			return;
		}

#undef strcpy
		if (!WasNull) {
			Out[Offset] = ' ';
			Offset++;
			strcpy(Out + Offset, User->Name);
		} else {
			WasNull = false;
		}

		strcpy(Out + Offset, User->Name);
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
	link_t<socket_t> *Current = m_OtherSockets.GetHead();

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
			User->SetPort(6667);

			User->SetLeanMode(2);

			User->Reconnect();

			free(Name);
		}
	}

	if (impulse == 11) {
		int i = 0;
		char **BaseKeys;
		char **Keys = GetUsers()->GetSortedKeys();

		BaseKeys = Keys;

		while (*Keys != NULL) {
			if (match("test*", *Keys) == 0) {
				RemoveUser(*Keys);

			}

			Keys++;
		}

		free(BaseKeys);
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

				asprintf(&Out, "%d lines parsed in %d msecs, approximately %d lines/msec", BENCHMARK_LINES, diff, lps);

				return Out;
			}
		}
	}

	return NULL;
}

const utility_t *CCore::GetUtilities(void) {
	static utility_t *Utilities = NULL;

	if (Utilities == NULL) {
		Utilities = (utility_t *)malloc(sizeof(utility_t));

		CHECK_ALLOC_RESULT(Utilities, malloc) {
			Fatal();
		} CHECK_ALLOC_RESULT_END;

		Utilities->ArgParseServerLine = ArgParseServerLine;
		Utilities->ArgTokenize = ArgTokenize;
		Utilities->ArgToArray = ArgToArray;
		Utilities->ArgRejoinArray = ArgRejoinArray;
		Utilities->ArgDupArray = ArgDupArray;
		Utilities->ArgFree = ArgFree;
		Utilities->ArgFreeArray = ArgFreeArray;
		Utilities->ArgGet = ArgGet;
		Utilities->ArgCount = ArgCount;

		Utilities->FlushCommands = FlushCommands;
		Utilities->AddCommand = AddCommand;
		Utilities->DeleteCommand = DeleteCommand;
		Utilities->CmpCommandT = CmpCommandT;

		Utilities->asprintf = asprintf;

		Utilities->Alloc = malloc;
		Utilities->Free = free;

		Utilities->IpToString = IpToString;
		Utilities->StringToIp = StringToIp;
		Utilities->HostEntToSockAddr = HostEntToSockAddr;
	}

	return Utilities;
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

	safe_printf("No valid configuration file has been found. A basic\n"
		"configuration file can be created for you automatically. Please\n"
		"answer the following questions:\n");

	while (true) {
		safe_printf("1. Which port should the bouncer listen on (valid ports are in the range 1025 - 65535): ");
		Buffer[0] = '\0';
		safe_scan(Buffer, sizeof(Buffer));
		Port = atoi(Buffer);

		if (Port == 0) {
			return false;
		}

#ifdef _WIN32
		if (Port <= 1024 || Port >= 65536) {
#else
		if (Port <= 0 || Port >= 65536) {
#endif
			safe_printf("You did not enter a valid port. Try again. Use 0 to abort.\n");
		} else {
			break;
		}
	}

	while (true) {
		safe_printf("2. What should the first user's name be? ");
		User[0] = '\0';
		safe_scan(User, sizeof(User));
	
		if (strlen(User) == 0) {
			return false;
		}

		if (IsValidUsername(User) == false) {
			safe_printf("Sorry, this is not a valid username. Try again.\n");
		} else {
			break;
		}
	}

	while (true) {
		safe_printf("Please note that passwords will not be echoed while you type them.\n");
		safe_printf("3. Please enter a password for the first user: ");

		Password[0] = '\0';
		safe_scan_passwd(Password, sizeof(Password));

		if (strlen(Password) == 0) {
			return false;
		}

		safe_printf("\n4. Please confirm your password by typing it again: ");

		PasswordConfirm[0] = '\0';
		safe_scan_passwd(PasswordConfirm, sizeof(PasswordConfirm));

		safe_printf("\n");

		if (strcmp(Password, PasswordConfirm) == 0) {
			break;
		} else {
			safe_printf("The passwords you entered do not match. Please try again.\n");
		}
	}

	asprintf(&File, "users/%s.conf", User);

	// BuildPath is using a static buffer
	mkdir(BuildPath("users"));
	SetPermissions(BuildPath("users"), S_IRUSR | S_IWUSR | S_IXUSR);

	MainConfig = m_ConfigModule->CreateConfigObject("sbnc.conf", NULL);

	MainConfig->WriteInteger("system.port", Port);
	MainConfig->WriteInteger("system.md5", 1);
	MainConfig->WriteString("system.users", User);

	safe_printf("Writing main configuration file...");

	MainConfig->Destroy();

	safe_printf(" DONE\n");

	UserConfig = m_ConfigModule->CreateConfigObject(File, NULL);

	UserConfig->WriteString("user.password", UtilMd5(Password, GenerateSalt()));
	UserConfig->WriteInteger("user.admin", 1);

	safe_printf("Writing first user's configuration file...");

	UserConfig->Destroy();

	safe_printf(" DONE\n");

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
		asprintf(&StringValue, "%d", Value);

		if (StringValue == NULL) {
			LOGERROR("asprintf() failed. Could not store global tag.");

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
 * GetBasePath
 *
 * Returns the bouncer's pathname.
 */
const char *CCore::GetBasePath(void) const {
	return sbncGetBaseName();
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
	return sbncBuildPath(Filename, BasePath);
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
		asprintf(&Out, "system.hosts.host%d", a++);

		CHECK_ALLOC_RESULT(Out, asprintf) {
			g_Bouncer->Fatal();
		} CHECK_ALLOC_RESULT_END;

		m_Config->WriteString(Out, m_HostAllows[i]);
		free(Out);
	}

	asprintf(&Out, "system.hosts.host%d", a);

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

	if (GetSSLContext() == NULL) {
		THROW(bool, Generic_Unknown, "Failed to create an SSL listener because there is no SSL server certificate.");
	}

	Listener = new CClientListener(Port, NULL, BindAddress, AF_INET, SSL);

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

	ListenerV6 = new CClientListener(Port, NULL, BindAddress, AF_INET6, SSL);

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

unsigned int CCore::GetResourceLimit(const char *Resource, CUser *User) {
	unsigned int i = 0;

	if (Resource == NULL || (User != NULL && User->IsAdmin())) {
		if (Resource != NULL && strcasecmp(Resource, "clients") == 0) {
			return 15;
		}

		return UINT_MAX;
	}

	while (g_ResourceLimits[i].Resource != NULL) {
		if (strcasecmp(g_ResourceLimits[i].Resource, Resource) == 0) {
			char *Name;

			if (User != NULL) {
				asprintf(&Name, "user.max%s", Resource);

				CHECK_ALLOC_RESULT(Name, asprintf) {} else {
					CResult<int> UserLimit = User->GetConfig()->ReadInteger(Name);

					if (!IsError(UserLimit)) {
						return UserLimit;
					}

					free(Name);
				} CHECK_ALLOC_RESULT_END;
			}

			asprintf(&Name, "system.max%s", Resource);

			CHECK_ALLOC_RESULT(Name, asprintf) {
				return g_ResourceLimits[i].DefaultLimit;
			} CHECK_ALLOC_RESULT_END;

			int Value = m_Config->ReadInteger(Name);

			free(Name);

			if (Value == 0) {
				return g_ResourceLimits[i].DefaultLimit;
			} else if (Value == -1) {
				return UINT_MAX;
			} else {
				return Value;
			}
		}

		i++;
	}

	return 0;
}

void CCore::SetResourceLimit(const char *Resource, unsigned int Limit, CUser *User) {
	char *Name;
	CConfig *Config;

	if (User != NULL) {
		asprintf(&Name, "user.max%s", Resource);
		Config = User->GetConfig();
	} else {
		asprintf(&Name, "system.max%s", Resource);
		Config = GetConfig();
	}

	CHECK_ALLOC_RESULT(Name, asprintf) {
		return;
	} CHECK_ALLOC_RESULT_END;

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

void CCore::SetMD5(bool MD5) {
	CacheSetInteger(m_ConfigCache, md5, MD5 ? 1 : 0);
}

const char *CCore::GetDefaultRealName(void) const {
	return CacheGetString(m_ConfigCache, realname);
}

void CCore::SetDefaultRealName(const char *RealName) {
	CacheSetString(m_ConfigCache, realname, RealName);
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

CConfig *CCore::CreateConfigObject(const char *Filename, CUser *User) {
	return m_ConfigModule->CreateConfigObject(Filename, User);
}

bool CCore::IsDaemonized(void) {
	return m_Daemonized;
}
