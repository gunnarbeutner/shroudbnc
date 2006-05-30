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
class CFakeClient;
struct CSocketEvents;
struct sockaddr_in;

/**
 * socket_t
 *
 * A registered socket.
 */
typedef struct socket_s {
	SOCKET Socket; /**< the underlying socket object */
	CSocketEvents *Events; /**< the event interface for this socket */
} socket_t;

/**
 * additionallistener_t
 *
 * An additional listener for client connections.
 */
typedef struct additionallistener_s {
	unsigned short Port; /**< the port of the listener */
	char *BindAddress; /**< the bind address, or NULL */
	bool SSL; /**< whether this is an SSL listener */
	CSocketEvents *Listener; /**< IPv4 listener object */
	CSocketEvents *ListenerV6; /**< IPv6 listener object */
} additionallistener_t;

#ifdef SWIGINTERFACE
%template(CVectorCModule) CVector<class CModule *>;
%template(CVectorCZoneInformation) CVector<struct CZoneInformation *>;
%template(CVectorFileT) CVector<file_t>;
#endif

/**
 * CCore
 *
 * The main application class.
 */
class CCore {
#ifndef SWIG
	friend class CTimer;
	friend class CDnsQuery;
	friend bool RegisterZone(CZoneInformation *ZoneInformation);
#endif

	CConfig *m_Config; /**< sbnc.conf object */

	CClientListener *m_Listener, *m_ListenerV6; /**< the main unencrypted listeners */
	CClientListener *m_SSLListener, *m_SSLListenerV6; /**< the main ssl listeners */

	CHashtable<CUser *, false, 512> m_Users; /**< the bouncer users */
	CVector<CModule *> m_Modules; /**< currently loaded modules */
	CVector<socket_t> m_OtherSockets; /**< a list of active sockets */
	CVector<CTimer *>m_Timers; /**< a list of active timers */

	time_t m_Startup; /**< TS when the bouncer was started */

	CLog *m_Log; /**< the bouncer's main log */

	CIdentSupport *m_Ident; /**< ident support interface */

	bool m_LoadingModules; /**< are we currently loading modules? */
	bool m_LoadingListeners; /**< are we currently loading listeners */

	CVector<char *>m_Args; /**< program arguments */

	CVector<CDnsQuery *> m_DnsQueries; /**< currently active dns queries */

	int m_SendqSizeCache; /**< cached size of the sendq (or -1 if unknown) */

	SSL_CTX *m_SSLContext; /**< SSL context for client listeners */
	SSL_CTX *m_SSLClientContext; /**< SSL context for IRC connections */

	int m_Status; /**< the program's status code */

	CVector<char *> m_HostAllows; /**< a list of hosts which are able to use this bouncer */

	CVector<additionallistener_t> m_AdditionalListeners; /**< a list of additional listeners */

	CVector<CZoneInformation *> m_Zones; /**< currently used allocation zones */
	CVector<CUser *> m_AdminUsers; /**< cached list of admin users */

	void UpdateModuleConfig(void);
	void UpdateUserConfig(void);
	void UpdateHosts(void);
	bool Daemonize(void) ;
	void WritePidFile(void) const;
	bool MakeConfig(void);

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
	virtual const char *const *GetArgV(void) const;

	virtual CConnection *WrapSocket(SOCKET Socket, bool SSL = false, connection_role_e Role = Role_Server) const;
	virtual void DeleteWrapper(CConnection *Wrapper) const;

	virtual bool IsRegisteredSocket(CSocketEvents *Events) const;
	virtual SOCKET SocketAndConnect(const char *Host, unsigned short Port, const char *BindIp);

	virtual const socket_t *GetSocketByClass(const char *Class, int Index) const;

	virtual CTimer *CreateTimer(unsigned int Interval, bool Repeat, TimerProc Function, void *Cookie) const;

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

	virtual unsigned int GetResourceLimit(const char *Resource);
	virtual void SetResourceLimit(const char *Resource, unsigned int Limit);

	virtual void SetOwnerHelper(CUser *User, size_t Bytes, bool Add);
};

#ifndef SWIG
extern CCore *g_Bouncer; /**< the main bouncer object */
extern time_t g_CurrentTime; /**< the current time (updated in main loop) */
#endif

/**
 * CHECK_ALLOC_RESULT
 *
 * Verifies that the result of an allocation function
 * is not NULL.
 *
 * @param Variable the variable holding the result
 * @param Function the name of the allocation function
 */
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

/**
 * CHECK_ALLOC_RESULT_END
 *
 * Marks the end of a CHECK_ALLOC_RESULT block.
 */
#define CHECK_ALLOC_RESULT_END } while (0)
