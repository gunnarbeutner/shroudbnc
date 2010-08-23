/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007,2010 Gunnar Beutner                                 *
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

#ifndef CORE_H
#define CORE_H

#define DEFAULT_SENDQ (10 * 1024)

class CConfig;
class CUser;
class CLog;
class CClientListener;
class CClientConnection;
class CIRCConnection;
class CIdentSupport;
class CModule;
class CConnection;
class CTimer;
class CFakeClient;
struct CSocketEvents;
struct sockaddr_in;

/**
 * Settings cache.
 *
 * Commonly used settings are cached.
 */
DEFINE_CACHE(System)
	DEFINE_OPTION_INT(dontmatchuser);
	DEFINE_OPTION_INT(port);
	DEFINE_OPTION_INT(sslport);
	DEFINE_OPTION_INT(sendq);
	DEFINE_OPTION_INT(md5);
	DEFINE_OPTION_INT(interval);

	DEFINE_OPTION_STRING(vhost);
	DEFINE_OPTION_STRING(users);
	DEFINE_OPTION_STRING(ip);
	DEFINE_OPTION_STRING(motd);
	DEFINE_OPTION_STRING(realname);
END_DEFINE_CACHE

/**
 * socket_t
 *
 * A registered socket.
 */
typedef struct socket_s {
	pollfd *PollFd	; /**< the underlying socket object */
	CSocketEvents *Events; /**< the event interface for this socket */
} socket_t;

/**
 * CCore
 *
 * The main application class.
 */
class SBNCAPI CCore {
#ifndef SWIG
	friend class CTimer;
	friend pollfd *registersocket(int Socket);
	friend void unregistersocket(int Socket);
#endif /* SWIG */

	FILE *m_PidFile; /**< sbnc.pid file */
	CConfig *m_Config; /**< sbnc.conf object */

	CClientListener *m_Listener, *m_ListenerV6; /**< the main unencrypted listeners */
	CClientListener *m_SSLListener, *m_SSLListenerV6; /**< the main ssl listeners */

	CHashtable<CUser *, false> m_Users; /**< the bouncer users */
	CVector<CModule *> m_Modules; /**< currently loaded modules */
	mutable CList<socket_t> m_OtherSockets; /**< a list of active sockets */
	CList<CTimer *> m_Timers; /**< a list of active timers */

	time_t m_Startup; /**< TS when the bouncer was started */

	CLog *m_Log; /**< the bouncer's main log */

	CIdentSupport *m_Ident; /**< ident support interface */

	bool m_LoadingModules; /**< are we currently loading modules? */
	bool m_LoadingListeners; /**< are we currently loading listeners */

	CVector<char *> m_Args; /**< program arguments */

	mutable CACHE(System) m_ConfigCache;

	SSL_CTX *m_SSLContext; /**< SSL context for client listeners */
	SSL_CTX *m_SSLClientContext; /**< SSL context for IRC connections */

	CVector<CUser *> m_AdminUsers; /**< cached list of admin users */

	CVector<pollfd> m_PollFds; /**< pollfd structures */

	int m_Status; /**< shroudBNC's current status */

	void UpdateModuleConfig(void);
	void UpdateUserConfig(void);
	void UpdateHosts(void);
	void WritePidFile(void);
	bool MakeConfig(void);

	void InitializeAdditionalListeners(void);
	void UninitializeAdditionalListeners(void);
	void UpdateAdditionalListeners(void);

	bool Daemonize(void);
public:
#ifndef SWIG
	CCore(CConfig *Config, int argc, char **argv);
	virtual ~CCore(void);
#endif /* SWIG */

	void StartMainLoop(bool ShouldDaemonize);

	CUser *GetUser(const char *Name);

	void GlobalNotice(const char *Text);

	CHashtable<CUser *, false> *GetUsers(void);

	RESULT<CModule *> LoadModule(const char *Filename);
	bool UnloadModule(CModule *Module);
	const CVector<CModule *> *GetModules(void) const;

	void SetIdent(const char *Ident);
	const char *GetIdent(void) const;

	CConfig *GetConfig(void);

	void RegisterSocket(SOCKET Socket, CSocketEvents *EventInterface);
	void UnregisterSocket(SOCKET Socket);

	SOCKET CreateListener(unsigned int Port, const char *BindIp = NULL, int Family = AF_INET) const;

	void Log(const char *Format, ...);
	void LogUser(CUser *User, const char *Format, ...);
	CLog *GetLog(void);

	void Shutdown(void);

	RESULT<CUser *> CreateUser(const char *Username, const char *Password);
	RESULT<bool> RemoveUser(const char *Username, bool RemoveConfig = true);
	bool IsValidUsername(const char *Username) const;

	time_t GetStartup(void) const;

	const char *MD5(const char *String, const char *Salt) const;

	int GetArgC(void) const;
	const char *const *GetArgV(void) const;

	bool IsRegisteredSocket(CSocketEvents *Events) const;
	SOCKET SocketAndConnect(const char *Host, unsigned int Port, const char *BindIp);

	const socket_t *GetSocketByClass(const char *Class, int Index) const;

	CTimer *CreateTimer(unsigned int Interval, bool Repeat, TimerProc Function, void *Cookie) const;

	bool Match(const char *Pattern, const char *String) const;

	size_t GetSendqSize(void) const;
	void SetSendqSize(size_t NewSize);

	const char *GetMotd(void) const;
	void SetMotd(const char *Motd);

	void InternalLogError(const char *Format, ...);
	void InternalSetFileAndLine(const char *Filename, unsigned int Line);
	void Fatal(void);

	SSL_CTX *GetSSLContext(void) ;
	SSL_CTX *GetSSLClientContext(void);
	int GetSSLCustomIndex(void) const;

	const char *DebugImpulse(int impulse);

	const utility_t *GetUtilities(void);

	const char *GetTagString(const char *Tag) const;
	int GetTagInteger(const char *Tag) const;
	bool SetTagString(const char *Tag, const char *Value);
	bool SetTagInteger(const char *Tag, int Value);
	const char *GetTagName(int Index) const;

	const char *BuildPathConfig(const char *Filename) const;
	const char *BuildPathExe(const char *Filename) const;
	const char *BuildPathModule(const char *Filename) const;
	const char *BuildPathShared(const char *Filename) const;

	const char *GetBouncerVersion(void) const;

	void SetStatus(int NewStatus);
	int GetStatus(void) const;

	CFakeClient *CreateFakeClient(void) const;
	void DeleteFakeClient(CFakeClient *FakeClient) const;

	CVector<CUser *> *GetAdminUsers(void);

	CClientListener *GetMainListener(void) const;
	CClientListener *GetMainListenerV6(void) const;
	CClientListener *GetMainSSLListener(void) const;
	CClientListener *GetMainSSLListenerV6(void) const;

	int GetResourceLimit(const char *Resource, CUser *User = NULL);
	void SetResourceLimit(const char *Resource, int Limit, CUser *User = NULL);

	int GetInterval(void) const;
	void SetInterval(int Interval);

	bool GetMD5(void) const;
	void SetMD5(bool MD5Flag);

	const char *GetDefaultRealName(void) const;
	void SetDefaultRealName(const char *RealName);

	const char *GetDefaultVHost(void) const;
	void SetDefaultVHost(const char *VHost);

	bool GetDontMatchUser(void) const;
	void SetDontMatchUser(bool Value);

	CACHE(System) *GetConfigCache(void);

	CConfig *CreateConfigObject(const char *Filename, CUser *User);
};

#ifndef SWIG
extern CCore *g_Bouncer; /**< the main bouncer object */
extern time_t g_CurrentTime; /**< the current time (updated in main loop) */
#endif /* SWIG */

#endif /* CORE_H */
