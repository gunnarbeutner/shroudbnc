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

class CClientConnection;
class CIRCConnection;
class CConfig;
class CLog;
class CTrafficStats;
class CKeyring;
class CTimer;

/**
 * badlogin_t
 *
 * Describes a failed login attempt.
 */
typedef struct badlogin_s {
	sockaddr *Address;
	unsigned int Count;
} badlogin_t;

#ifndef SWIG
bool BadLoginTimer(time_t Now, void *User);
bool UserReconnectTimer(time_t Now, void *User);
#endif

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
	m_Config->WriteString("user." #Setting, Value); \
	\
	free(DupValue); \
}

/**
 * CUser
 *
 * A bouncer user.
 */
class CUser : public CZoneObject<CUser, 32> {
	friend class CCore;
#ifndef SWIG
	friend bool BadLoginTimer(time_t Now, void *User);
	friend bool UserReconnectTimer(time_t Now, void *User);
#endif

	char *m_Name; /**< the name of the user */

	CClientConnection *m_Client; /**< the user's client connection */
	CIRCConnection *m_IRC; /**< the user's irc connection */
	CConfig *m_Config; /**< the user's configuration object */
	CLog *m_Log; /**< the user's log file */

	time_t m_ReconnectTime; /**< when the next connect() attempt is going to be made */
	time_t m_LastReconnect; /**< when the last connect() attempt was made for this user */

	CVector<badlogin_t> m_BadLogins; /**< a list of failed login attempts for this user */

	CTrafficStats *m_ClientStats; /**< traffic stats for the user's client connection(s) */
	CTrafficStats *m_IRCStats; /**< traffic stats for the user's irc connection(s) */

	CKeyring *m_Keys; /**< a list of channel keys */

	CTimer *m_BadLoginPulse; /**< a timer which will remove "bad logins" */
	CTimer *m_ReconnectTimer; /**< a timer which reconnects the user to an IRC server */

	mutable int m_IsAdminCache; /**< cached value which determines whether the user is an admin */

	CVector<X509 *> m_ClientCertificates; /**< the client certificates for the user */

	bool PersistCertificates(void);

	void BadLoginPulse(void);
public:
#ifndef SWIG
	CUser(const char *Name);
	virtual ~CUser(void);
#endif

	virtual CClientConnection *GetClientConnection(void);
	virtual CIRCConnection *GetIRCConnection(void);

	virtual bool CheckPassword(const char *Password) const;
	virtual void Attach(CClientConnection *Client);

	virtual const char *GetNick(void) const;
	virtual void SetNick(const char *Nick);

	virtual const char *GetRealname(void) const;
	virtual void SetRealname(const char *Realname);

	virtual const char *GetUsername(void) const;
	virtual CConfig *GetConfig(void);

	virtual void Simulate(const char *Command, CClientConnection *FakeClient = NULL);

	virtual void Reconnect(void);

	virtual bool ShouldReconnect(void) const;
	virtual void ScheduleReconnect(int Delay = 10);

	virtual void Notice(const char *Text);
	virtual void RealNotice(const char *Text);

	virtual unsigned int GetIRCUptime(void) const;

	virtual CLog *GetLog(void);
	virtual void Log(const char *Format, ...);

	virtual void Lock(void);
	virtual void Unlock(void);
	virtual bool IsLocked(void) const;

	virtual void SetIRCConnection(CIRCConnection *IRC);
	virtual void SetClientConnection(CClientConnection *Client, bool DontSetAway = false);

	virtual void SetAdmin(bool Admin = true);
	virtual bool IsAdmin(void) const;

	virtual void SetPassword(const char *Password);

	virtual void SetServer(const char *Server);
	virtual const char *GetServer(void) const;

	virtual void SetPort(int Port);
	virtual int GetPort(void) const;

	virtual void MarkQuitted(bool RequireManualJump = false);
	virtual int IsQuitted(void) const;

	virtual void LoadEvent(void);

	virtual void LogBadLogin(sockaddr *Peer);
	virtual bool IsIpBlocked(sockaddr *Peer) const;

	virtual const CTrafficStats *GetClientStats(void) const;
	virtual const CTrafficStats *GetIRCStats(void) const;

	virtual CKeyring *GetKeyring(void);

	virtual time_t GetLastSeen(void) const;

	virtual const char *GetAwayNick(void) const;
	virtual void SetAwayNick(const char *Nick);

	virtual const char *GetAwayText(void) const;
	virtual void SetAwayText(const char *Reason);

	virtual const char *GetVHost(void) const;
	virtual void SetVHost(const char *VHost);

	virtual int GetDelayJoin(void) const;
	virtual void SetDelayJoin(int DelayJoin);

	virtual const char *GetConfigChannels(void) const;
	virtual void SetConfigChannels(const char *Channels);

	virtual const char *GetSuspendReason(void) const;
	virtual void SetSuspendReason(const char *Reason);

	virtual const char *GetServerPassword(void) const;
	virtual void SetServerPassword(const char *Password);

	virtual const char *GetAutoModes(void) const;
	virtual void SetAutoModes(const char *AutoModes);

	virtual const char *GetDropModes(void) const;
	virtual void SetDropModes(const char *DropModes);

	virtual const CVector<X509 *> *GetClientCertificates(void) const;
	virtual bool AddClientCertificate(const X509 *Certificate);
	virtual bool RemoveClientCertificate(const X509 *Certificate);
	virtual bool FindClientCertificate(const X509 *Certificate) const;

	virtual void SetSSL(bool SSL);
	virtual bool GetSSL(void) const;

	virtual void SetIdent(const char *Ident);
	virtual const char *GetIdent(void) const;

	virtual void SetIPv6(bool IPv6);
	virtual bool GetIPv6(void) const;

	virtual const char *GetTagString(const char *Tag) const;
	virtual int GetTagInteger(const char *Tag) const;
	virtual bool SetTagString(const char *Tag, const char *Value);
	virtual bool SetTagInteger(const char *Tag, int Value);
	virtual const char *GetTagName(int Index) const;

	virtual const char *FormatTime(time_t Timestamp) const;
	virtual void SetGmtOffset(int Offset);
	virtual int GetGmtOffset(void) const;

	virtual void SetSystemNotices(bool SystemNotices);
	virtual bool GetSystemNotices(void) const;

	virtual void SetAwayMessage(const char *Text);
	virtual const char *GetAwayMessage(void) const;

	virtual void SetLeanMode(unsigned int Mode);
	virtual unsigned int GetLeanMode(void);
};
