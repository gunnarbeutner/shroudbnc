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
class CClientListener;
class CClientConnection;
class CIRCConnection;
class CIdentSupport;
class CModule;
class CConnection;
class CTimer;
class CAssocArray;
struct CSocketEvents;
struct sockaddr_in;

typedef struct socket_s {
	SOCKET Socket;
	CSocketEvents *Events;
} socket_t;

typedef struct additionallistener_s {
	unsigned short Port;
	char *BindAddress;
	bool SSL;
	CSocketEvents *Listener;
	CSocketEvents *ListenerV6;
} additionallistener_t;

#ifdef SWIGINTERFACE
%template(CVectorCModule) CVector<class CModule *>;
%template(CVectorCZoneInformation) CVector<struct CZoneInformation *>;
%template(CVectorFileT) CVector<file_t>;
#endif

class CCore {
#ifndef SWIG
	friend class CTimer;
	friend class CDnsQuery;
	friend bool RegisterZone(CZoneInformation *ZoneInformation);
#endif

	CConfig *m_Config;

	CClientListener *m_Listener, *m_ListenerV6;
	CClientListener *m_SSLListener, *m_SSLListenerV6;

	CHashtable<CUser *, false, 512> m_Users;
	CVector<CModule *> m_Modules;
	CVector<socket_t> m_OtherSockets;
	CVector<CTimer *>m_Timers;

	time_t m_Startup;

	CLog *m_Log;

	CIdentSupport *m_Ident;

	bool m_LoadingModules;
	bool m_LoadingListeners;

	CVector<char *>m_Args;

	CVector<CDnsQuery *> m_DnsQueries;

	int m_SendqSizeCache;

	SSL_CTX *m_SSLContext;
	SSL_CTX *m_SSLClientContext;

	int m_Status;

	CVector<char *> m_HostAllows; /**< a list of hosts which are able to use this bouncer */

	CVector<additionallistener_t> m_AdditionalListeners;

	void UpdateModuleConfig(void);
	void UpdateUserConfig(void);
	void UpdateHosts(void);
	bool Daemonize(void) ;
	void WritePidFile(void) const;
	bool MakeConfig(void);

	CVector<CZoneInformation *> m_Zones;
	CVector<CUser *> m_AdminUsers;

	void RegisterTimer(CTimer *Timer);
	void UnregisterTimer(CTimer *Timer);

	void RegisterDnsQuery(CDnsQuery *DnsQuery);
	void UnregisterDnsQuery(CDnsQuery *DnsQuery);

	void RegisterZone(CZoneInformation *ZoneInformation);

	void InitializeAdditionalListeners(void);
	void UninitializeAdditionalListeners(void);
	void UpdateAdditionalListeners(void);
public:
#ifndef SWIG
	CCore(CConfig *Config, int argc, char **argv);
	virtual ~CCore(void);
#endif

	virtual void StartMainLoop(void);

	virtual CUser *GetUser(const char *Name);

	virtual void GlobalNotice(const char *Text);

	virtual CHashtable<CUser *, false, 512> *GetUsers(void);

	virtual RESULT<CModule *> LoadModule(const char *Filename);
	virtual bool UnloadModule(CModule *Module);
	virtual const CVector<CModule *> *GetModules(void) const;

	virtual void SetIdent(const char *Ident);
	virtual const char *GetIdent(void) const;

	virtual CConfig *GetConfig(void);

	virtual void RegisterSocket(SOCKET Socket, CSocketEvents *EventInterface);
	virtual void UnregisterSocket(SOCKET Socket);

	virtual SOCKET CreateListener(unsigned short Port, const char *BindIp = NULL, int Family = AF_INET) const;

	virtual void Log(const char *Format, ...);
	virtual CLog *GetLog(void);

	virtual void Shutdown(void);

	virtual RESULT<CUser *> CreateUser(const char *Username, const char *Password);
	virtual RESULT<bool> RemoveUser(const char *Username, bool RemoveConfig = true);
	virtual bool IsValidUsername(const char *Username) const;

	virtual time_t GetStartup(void) const;

	virtual const char *MD5(const char *String) const;

	virtual int GetArgC(void) const;
	virtual const char * const* GetArgV(void) const;

	virtual CConnection *WrapSocket(SOCKET Socket, bool SSL = false, connection_role_e Role = Role_Server) const;
	virtual void DeleteWrapper(CConnection *Wrapper) const;

	virtual bool IsRegisteredSocket(CSocketEvents *Events) const;
	virtual SOCKET SocketAndConnect(const char *Host, unsigned short Port, const char *BindIp);

	virtual const socket_t *GetSocketByClass(const char *Class, int Index) const;

	virtual CTimer *CreateTimer(unsigned int Interval, bool Repeat, TimerProc Function, void *Cookie) const;

	virtual int GetTimerStats(void) const;

	virtual bool Match(const char *Pattern, const char *String) const;

	virtual size_t GetSendqSize(void) const;
	virtual void SetSendqSize(size_t NewSize);

	virtual const char *GetMotd(void) const;
	virtual void SetMotd(const char *Motd);

	virtual void InternalLogError(const char *Format, ...);
	virtual void InternalSetFileAndLine(const char *Filename, unsigned int Line);
	virtual void Fatal(void);

	virtual SSL_CTX *GetSSLContext(void) ;
	virtual SSL_CTX *GetSSLClientContext(void);
	virtual int GetSSLCustomIndex(void) const;

	virtual const char *DebugImpulse(int impulse);

	virtual bool Freeze(CAssocArray *Box);
	virtual bool Thaw(CAssocArray *Box);
	virtual bool InitializeFreeze(void);
	virtual const loaderparams_s *GetLoaderParameters(void) const;

	virtual const utility_t *GetUtilities(void);

	virtual const char *GetTagString(const char *Tag) const;
	virtual int GetTagInteger(const char *Tag) const;
	virtual bool SetTagString(const char *Tag, const char *Value);
	virtual bool SetTagInteger(const char *Tag, int Value);
	virtual const char *GetTagName(int Index) const;

	virtual const char *GetBasePath(void) const;
	virtual const char *BuildPath(const char *Filename, const char *BasePath = NULL) const;

	virtual const char *GetBouncerVersion(void) const;

	virtual void SetStatus(int NewStatus);
	virtual int GetStatus(void) const;

	virtual const CVector<CZoneInformation *> *GetZones(void) const;
	virtual const CVector<file_t> *GetAllocationInformation(void) const;

	virtual CFakeClient *CreateFakeClient(void) const;
	virtual void DeleteFakeClient(CFakeClient *FakeClient) const;

	virtual RESULT<bool> AddHostAllow(const char *Mask, bool UpdateConfig = true);
	virtual RESULT<bool> RemoveHostAllow(const char *Mask, bool UpdateConfig = true);
	virtual const CVector<char *> *GetHostAllows(void) const;
	virtual bool CanHostConnect(const char *Host) const;
	virtual bool IsValidHostAllow(const char *Mask) const;

	virtual CVector<CUser *> *GetAdminUsers(void);

	virtual RESULT<bool> AddAdditionalListener(unsigned short Port, const char *BindAddress = NULL, bool SSL = false);
	virtual RESULT<bool> RemoveAdditionalListener(unsigned short Port);
	virtual CVector<additionallistener_t> *GetAdditionalListeners(void);

	virtual CClientListener *GetMainListener(void) const;
	virtual CClientListener *GetMainListenerV6(void) const;
	virtual CClientListener *GetMainSSLListener(void) const;
	virtual CClientListener *GetMainSSLListenerV6(void) const;
};

extern CCore *g_Bouncer;

#ifndef SWIG
extern time_t g_CurrentTime;
#endif

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
