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

#define DEFAULT_SENDQ (10 * 1024)

class CConfig;
class CUser;
class CLog;
class CClientConnection;
class CIRCConnection;
class CIdentSupport;
class CModule;
class CConnection;
class CTimer;
class CAssocArray;
struct CSocketEvents;
struct sockaddr_in;
struct loaderparams_s;

typedef struct socket_s {
	SOCKET Socket;
	CSocketEvents *Events;
} socket_t;

typedef void (*ExitHandler)(void *Cookie);

typedef struct exithandler_s {
	ExitHandler Handler;
	void *Cookie;
} exithandler_t;

class CClientListener;

class CCore {
#ifndef SWIG
	friend class CTimer;
	friend class CDnsQuery;
#endif

	CConfig *m_Config;

	CClientListener *m_Listener, *m_ListenerV6;
	CClientListener *m_SSLListener, *m_SSLListenerV6;

	CHashtable<CUser *, false, 64> m_Users;
	CVector<CModule *> m_Modules;
	CVector<socket_t> m_OtherSockets;
	CVector<CTimer *>m_Timers;

	time_t m_Startup;

	CLog *m_Log;

	CIdentSupport *m_Ident;

	bool m_LoadingModules;
	bool m_Running;

	CVector<char *>m_Args;

	CVector<CDnsQuery *> m_DnsQueries;

	int m_SendQSizeCache;

	SSL_CTX *m_SSLContext;
	SSL_CTX *m_SSLClientContext;

	void UpdateModuleConfig(void);
	void UpdateUserConfig(void);
	bool Daemonize(void);
	void WritePidFile(void);
	bool MakeConfig(void);

	CVector<exithandler_t> m_ExitHandlers;

	void RegisterTimer(CTimer *Timer);
	void UnregisterTimer(CTimer *Timer);

	void RegisterDnsQuery(CDnsQuery *DnsQuery);
	void UnregisterDnsQuery(CDnsQuery *DnsQuery);
public:
#ifndef SWIG
	CCore(CConfig *Config, int argc, char **argv);
#endif
	virtual ~CCore(void);

	virtual void StartMainLoop(void);

	virtual CUser *GetUser(const char *Name);

	virtual void GlobalNotice(const char *Text, bool AdminOnly = false);

	virtual CHashtable<CUser *, false, 64> *GetUsers(void);
	virtual int GetUserCount(void);

	virtual CModule *LoadModule(const char *Filename, const char **Error);
	virtual bool UnloadModule(CModule *Module);
	virtual CVector<CModule *> *GetModules(void);

	virtual void SetIdent(const char *Ident);
	virtual const char *GetIdent(void);

	virtual CConfig *GetConfig(void);

	virtual void RegisterSocket(SOCKET Socket, CSocketEvents *EventInterface);
	virtual void UnregisterSocket(SOCKET Socket);

	virtual SOCKET CreateListener(unsigned short Port, const char *BindIp = NULL, int Family = AF_INET);

	virtual void Log(const char *Format, ...);
	virtual CLog *GetLog(void);

	virtual void Shutdown(void);

	virtual CUser *CreateUser(const char *Username, const char *Password);
	virtual bool RemoveUser(const char *Username, bool RemoveConfig = true);
	virtual bool IsValidUsername(const char *Username);

	virtual time_t GetStartup(void);

	virtual const char *MD5(const char *String);

	virtual int GetArgC(void);
	virtual char **GetArgV(void);

	virtual CConnection *WrapSocket(SOCKET Socket, bool SSL = false, connection_role_e Role = Role_Server);
	virtual void DeleteWrapper(CConnection *Wrapper);

	virtual bool IsRegisteredSocket(CSocketEvents *Events);
	virtual SOCKET SocketAndConnect(const char *Host, unsigned short Port, const char *BindIp);

	virtual socket_t *GetSocketByClass(const char *Class, int Index);

	virtual CTimer *CreateTimer(unsigned int Interval, bool Repeat, TimerProc Function, void *Cookie);

	virtual int GetTimerStats(void);

	virtual bool Match(const char *Pattern, const char *String);

	virtual int GetSendQSize(void);
	virtual void SetSendQSize(int NewSize);

	virtual const char *GetMotd(void);
	virtual void SetMotd(const char *Motd);

	virtual void InternalLogError(const char *Format, ...);
	virtual void InternalSetFileAndLine(const char *Filename, unsigned int Line);
	virtual void Fatal(void);

	virtual SSL_CTX *GetSSLContext(void);
	virtual SSL_CTX *GetSSLClientContext(void);
	virtual int GetSSLCustomIndex(void);

	virtual const char *DebugImpulse(int impulse);

	virtual bool Freeze(CAssocArray *Box);
	virtual bool Unfreeze(CAssocArray *Box);
	virtual bool InitializeFreeze(void);
	virtual const loaderparams_s *GetLoaderParameters(void);

	virtual const utility_t *GetUtilities(void);

	virtual const char *GetTagString(const char *Tag);
	virtual int GetTagInteger(const char *Tag);
	virtual bool SetTagString(const char *Tag, const char *Value);
	virtual bool SetTagInteger(const char *Tag, int Value);
	virtual const char *GetTagName(int Index);

	virtual const char *GetBasePath(void);
	virtual const char *BuildPath(const char *Filename, const char *BasePath = NULL);

	virtual const char *GetBouncerVersion(void);

	virtual bool RegisterExitHandler(ExitHandler Handler, void *Cookie);
	virtual bool UnregisterExitHandler(ExitHandler Handler, void *Cookie);
};

extern CCore *g_Bouncer;

#define CHECK_ALLOC_RESULT(Variable, Function) \
	do { \
		if (Variable == NULL) { \
			if (g_Bouncer != NULL) { \
				LOGERROR(#Function " failed."); \
			} else { \
				printf(#Function " failed."); \
			} \
			\
		} \
		if (Variable == NULL)

#define CHECK_ALLOC_RESULT_END } while (0)
