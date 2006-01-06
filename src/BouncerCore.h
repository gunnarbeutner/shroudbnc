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

class CBouncerConfig;
class CBouncerUser;
class CBouncerLog;
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
//template <typename value_type, bool casesensitive, int Size, bool VolatileKeys> class CHashtable;

typedef struct socket_s {
	SOCKET Socket;
	CSocketEvents *Events;
} socket_t;

#ifndef USESSL
typedef void SSL_CTX;
#endif

class CClientListener;
class CSSLClientListener;

class CBouncerCore {
	CBouncerConfig *m_Config;

	CClientListener *m_Listener;
	CSSLClientListener *m_SSLListener;

	CHashtable<CBouncerUser *, false, 64> m_Users;
	CVector<CModule *> m_Modules;
	CVector<socket_s> m_OtherSockets;
	CVector<CTimer *>m_Timers;

	time_t m_Startup;

	CBouncerLog *m_Log;

	CIdentSupport *m_Ident;

	bool m_LoadingModules;
	bool m_Running;

	CVector<char *>m_Args;

	int m_SendQSizeCache;

	SSL_CTX *m_SSLContext;
	SSL_CTX *m_SSLClientContext;

	void HandleConnectingClient(SOCKET Client, sockaddr_in Remote, bool SSL = false);
	void UpdateModuleConfig(void);
	void UpdateUserConfig(void);
	bool Daemonize(void);
	void WritePidFile(void);
public:
#ifndef SWIG
	CBouncerCore(CBouncerConfig *Config, int argc, char **argv);
#endif
	virtual ~CBouncerCore(void);

	virtual void StartMainLoop(void);

	virtual CBouncerUser *GetUser(const char *Name);

	virtual void GlobalNotice(const char *Text, bool AdminOnly = false);

	virtual CHashtable<CBouncerUser *, false, 64> *GetUsers(void);
	virtual int GetUserCount(void);

	virtual CModule **GetModules(void);
	virtual int GetModuleCount(void);
	virtual CModule *LoadModule(const char *Filename, const char **Error);
	virtual bool UnloadModule(CModule *Module);

	virtual void SetIdent(const char *Ident);
	virtual const char *GetIdent(void);

	virtual CBouncerConfig *GetConfig(void);

	virtual void RegisterSocket(SOCKET Socket, CSocketEvents *EventInterface);
	virtual void UnregisterSocket(SOCKET Socket);

	virtual SOCKET CreateListener(unsigned short Port, const char *BindIp = NULL);

	virtual void Log(const char *Format, ...);
	virtual CBouncerLog *GetLog(void);

	virtual void Shutdown(void);

	virtual CBouncerUser *CreateUser(const char *Username, const char *Password);
	virtual bool RemoveUser(const char *Username, bool RemoveConfig = true);
	virtual bool IsValidUsername(const char *Username);

	virtual time_t GetStartup(void);

	virtual const char *MD5(const char *String);

	virtual int GetArgC(void);
	virtual char **GetArgV(void);

	virtual CConnection *WrapSocket(SOCKET Socket, bool IsClient = true, bool SSL = false);
	virtual void DeleteWrapper(CConnection *Wrapper);

	virtual void Free(void *Pointer);
	virtual void *Alloc(size_t Size);

	virtual bool IsRegisteredSocket(CSocketEvents *Events);
	virtual SOCKET SocketAndConnect(const char *Host, unsigned short Port, const char *BindIp);

	virtual socket_t *GetSocketByClass(const char *Class, int Index);

	virtual CTimer *CreateTimer(unsigned int Interval, bool Repeat, TimerProc Function, void *Cookie);
	virtual void RegisterTimer(CTimer *Timer);
	virtual void UnregisterTimer(CTimer *Timer);

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
};

extern CBouncerCore *g_Bouncer;

#ifndef SWIG
extern adns_state g_adns_State;
#endif
