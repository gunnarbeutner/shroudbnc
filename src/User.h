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

class CClientConnection;
class CIRCConnection;
struct CConfig;
class CLog;
class CTrafficStats;
class CKeyring;
class CTimer;

/**
 * Cache: User
 */
DEFINE_CACHE(User)
	DEFINE_OPTION_INT(quitted);
	DEFINE_OPTION_INT(ts);
	DEFINE_OPTION_INT(admin);
	DEFINE_OPTION_INT(port);
	DEFINE_OPTION_INT(lock);
	DEFINE_OPTION_INT(seen);
	DEFINE_OPTION_INT(delayjoin);
	DEFINE_OPTION_INT(ssl);
	DEFINE_OPTION_INT(ipv6);
	DEFINE_OPTION_INT(ignsysnotices);
	DEFINE_OPTION_INT(lean);
	DEFINE_OPTION_INT(quitaway);

	DEFINE_OPTION_STRING(automodes);
	DEFINE_OPTION_STRING(dropmodes);
	DEFINE_OPTION_STRING(password);
	DEFINE_OPTION_STRING(away);
	DEFINE_OPTION_STRING(awaynick);
	DEFINE_OPTION_STRING(nick);
	DEFINE_OPTION_STRING(realname);
	DEFINE_OPTION_STRING(server);
	DEFINE_OPTION_STRING(ip);
	DEFINE_OPTION_STRING(channels);
	DEFINE_OPTION_STRING(suspend);
	DEFINE_OPTION_STRING(spass);
	DEFINE_OPTION_STRING(ident);
	DEFINE_OPTION_STRING(tz);
	DEFINE_OPTION_STRING(awaymessage);
	DEFINE_OPTION_STRING(channelsort);
END_DEFINE_CACHE

/**
 * client_t
 *
 * Internal representation of clients in the client list.
 */
typedef struct client_s {
	time_t Creation;
	CClientConnection *Client;
} client_t;

template class SBNCAPI CVector<client_t>;

/**
 * badlogin_t
 *
 * Describes a failed login attempt.
 */
typedef struct badlogin_s {
	sockaddr *Address; /**< the address of the user */
	unsigned int Count; /**< the number of times the user has used an
							 incorrect password */
} badlogin_t;

#ifndef SWIG
bool BadLoginTimer(time_t Now, void *User);
bool UserReconnectTimer(time_t Now, void *User);
#endif

/**
 * USER_SETFUNCTION
 *
 * Implements a Set*-function
 *
 * @param Setting the name of the setting
 * @param Value the new value for this setting
 */
#define USER_SETFUNCTION(Setting, Value) { \
	char *DupValue = NULL; \
	\
	if (Value != NULL) { \
		DupValue = strdup(Value); \
		\
		CHECK_ALLOC_RESULT(DupValue, strdup) { \
			return; \
		} CHECK_ALLOC_RESULT_END; \
		\
	} \
	\
	CacheSetString(m_ConfigCache, Setting, Value); \
	\
	free(DupValue); \
}

/**
 * CUser
 *
 * A bouncer user.
 */
class SBNCAPI CUser : public CZoneObject<CUser, 128>, public CMemoryManager, public CPersistable {
	friend class CCore;
#ifndef SWIG
	friend bool BadLoginTimer(time_t Now, void *User);
	friend bool UserReconnectTimer(time_t Now, void *User);
#endif

	char *m_Name; /**< the name of the user */

	CClientConnection *m_PrimaryClient;
	CClientConnectionMultiplexer *m_ClientMultiplexer;
	CVector<client_t> m_Clients; /**< the user's client connections */
	CIRCConnection *m_IRC; /**< the user's irc connection */
	CConfig *m_Config; /**< the user's configuration object */
	mutable CACHE(User) m_ConfigCache; /**< config cache */
	CLog *m_Log; /**< the user's log file */

	time_t m_ReconnectTime; /**< when the next connect() attempt is going to be made */
	time_t m_LastReconnect; /**< when the last connect() attempt was made for this user */

	CVector<badlogin_t> m_BadLogins; /**< a list of failed login attempts for this user */

	CTrafficStats *m_ClientStats; /**< traffic stats for the user's client connection(s) */
	CTrafficStats *m_IRCStats; /**< traffic stats for the user's irc connection(s) */

	CKeyring *m_Keys; /**< a list of channel keys */

	CTimer *m_BadLoginPulse; /**< a timer which will remove "bad logins" */

	CVector<X509 *> m_ClientCertificates; /**< the client certificates for the user */

	size_t m_ManagedMemory;
	mmanager_t *m_MemoryManager;

	bool PersistCertificates(void);

	void BadLoginPulse(void);
public:
#ifndef SWIG
	CUser(const char *Name, safe_box_t Box);
	virtual ~CUser(void);
#endif

	static void RescheduleReconnectTimer(void);

	CClientConnection *GetPrimaryClientConnection(void);
	CClientConnection *GetClientConnectionMultiplexer(void);
	CVector<client_t> *GetClientConnections(void);

	CIRCConnection *GetIRCConnection(void);

	bool CheckPassword(const char *Password) const;
	void Attach(CClientConnection *Client);

	const char *GetNick(void) const;
	void SetNick(const char *Nick);

	const char *GetRealname(void) const;
	void SetRealname(const char *Realname);

	const char *GetUsername(void) const;
	CConfig *GetConfig(void);

	void Simulate(const char *Command, CClientConnection *FakeClient = NULL);
	const char *SimulateWithResult(const char *Command);

	void Reconnect(void);

	bool ShouldReconnect(void) const;
	void ScheduleReconnect(int Delay = 10);

	unsigned int GetIRCUptime(void) const;

	CLog *GetLog(void);
	void Log(const char *Format, ...);

	void Lock(void);
	void Unlock(void);
	bool IsLocked(void) const;

	void SetIRCConnection(CIRCConnection *IRC);
//	void SetClientConnection(CClientConnection *Client, bool DontSetAway = false);

	void AddClientConnection(CClientConnection *Client, bool Silent = false);
	void RemoveClientConnection(CClientConnection *Client, bool Silent = false);
	bool IsRegisteredClientConnection(CClientConnection *Client);

	void SetAdmin(bool Admin = true);
	bool IsAdmin(void) const;

	void SetPassword(const char *Password);

	void SetServer(const char *Server);
	const char *GetServer(void) const;

	void SetPort(int Port);
	int GetPort(void) const;

	void MarkQuitted(bool RequireManualJump = false);
	int IsQuitted(void) const;

	void LoadEvent(void);

	void LogBadLogin(sockaddr *Peer);
	bool IsIpBlocked(sockaddr *Peer) const;

	const CTrafficStats *GetClientStats(void) const;
	const CTrafficStats *GetIRCStats(void) const;

	CKeyring *GetKeyring(void);

	time_t GetLastSeen(void) const;

	const char *GetAwayNick(void) const;
	void SetAwayNick(const char *Nick);

	const char *GetAwayText(void) const;
	void SetAwayText(const char *Reason);

	const char *GetVHost(void) const;
	void SetVHost(const char *VHost);

	int GetDelayJoin(void) const;
	void SetDelayJoin(int DelayJoin);

	const char *GetConfigChannels(void) const;
	void SetConfigChannels(const char *Channels);

	const char *GetSuspendReason(void) const;
	void SetSuspendReason(const char *Reason);

	const char *GetServerPassword(void) const;
	void SetServerPassword(const char *Password);

	const char *GetAutoModes(void) const;
	void SetAutoModes(const char *AutoModes);

	const char *GetDropModes(void) const;
	void SetDropModes(const char *DropModes);

	const CVector<X509 *> *GetClientCertificates(void) const;
	bool AddClientCertificate(const X509 *Certificate);
	bool RemoveClientCertificate(const X509 *Certificate);
	bool FindClientCertificate(const X509 *Certificate) const;

	void SetSSL(bool SSL);
	bool GetSSL(void) const;

	void SetIdent(const char *Ident);
	const char *GetIdent(void) const;

	void SetIPv6(bool IPv6);
	bool GetIPv6(void) const;

	const char *GetTagString(const char *Tag) const;
	int GetTagInteger(const char *Tag) const;
	bool SetTagString(const char *Tag, const char *Value);
	bool SetTagInteger(const char *Tag, int Value);
	const char *GetTagName(int Index) const;

	const char *FormatTime(time_t Timestamp) const;
	void SetGmtOffset(int Offset);
	int GetGmtOffset(void) const;

	void SetSystemNotices(bool SystemNotices);
	bool GetSystemNotices(void) const;

	void SetAwayMessage(const char *Text);
	const char *GetAwayMessage(void) const;

	void SetLeanMode(unsigned int Mode);
	unsigned int GetLeanMode(void);

	void SetAppendTimestamp(bool Value);
	bool GetAppendTimestamp(void);

	void SetUseQuitReason(bool Value);
	bool GetUseQuitReason(void);

	void SetChannelSortMode(const char *Mode);
	const char *GetChannelSortMode(void) const;

	bool MemoryAddBytes(size_t Bytes);
	void MemoryRemoveBytes(size_t Bytes);
	size_t MemoryGetSize(void);
	size_t MemoryGetLimit(void);
	mmanager_t *MemoryGetManager(void);
};
