/*******************************************************************************
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

#ifndef IRCCONNECTION_H
#define IRCCONNECTION_H

/**
 * connection_state_e
 *
 * Describes the status of an IRC connection.
 */
enum connection_state_e {
	State_Unknown, /**< unknown status */
	State_Connecting, /**< first ping event has not yet been received */
	State_Pong, /**< first pong has been sent */
	State_Connected /**< the motd has been received */
};

class CUser;
class CChannel;
class CQueue;
class CFloodControl;
class CTimer;

#ifdef SWIGINTERFACE
%template(COwnedObjectCUser) COwnedObject<class CUser>;
#endif /* SWIGINTERFACE */

#ifndef SWIG
bool DelayJoinTimer(time_t Now, void *IRCConnection);
bool IRCPingTimer(time_t Now, void *IRCConnection);
bool NickCatchTimer(time_t Now, void *IRCConnection);
#endif /* SWIG */

/**
 * CIRCConnection
 *
 * An IRC connection.
 */
class SBNCAPI CIRCConnection : public CConnection, public CObject<CIRCConnection, CUser> {
private:
#ifndef SWIG
	friend bool DelayJoinTimer(time_t Now, void *IRCConnection);
	friend bool IRCPingTimer(time_t Now, void *IRCConnection);
	friend bool NickCatchTimer(time_t Now, void *IRCConnection);
#endif /* SWIG */

	connection_state_e m_State; /**< the current status of the IRC connection */
	bool m_SeenMotd; /* whether we've seen the motd */

	char *m_CurrentNick; /**< the current nick for this IRC connection */
	char *m_Site; /**< the ident\@host of this IRC connection */
	char *m_Server; /**< the hostname of the IRC server */
	char *m_Usermodes; /**< the usermodes */

	CHashtable<CChannel *, false> *m_Channels; /**< the channels this IRC user is on */

	char *m_ServerVersion; /**< the version from the 004 reply */
	char *m_ServerFeat; /**< the server features from the 351 reply */
	char *m_ChanModes; /**< the channel modes from the 004 reply */
	char *m_UserModes; /**< the user modes from the 004 reply */

	CHashtable<char *, false> *m_ISupport; /**< the key/value pairs from the 005 replies */
	
	CTimer *m_DelayJoinTimer; /**< timer for delay-joining channels */
	CTimer *m_PingTimer; /**< timer for sending regular PINGs to the server */
	CTimer *m_NickCatchTimer; /**< used for regaining a user's away nick */

	CQueue *m_QueueLow; /**< low-priority queue */
	CQueue *m_QueueMiddle; /**< middle-priority queue */
	CQueue *m_QueueHigh; /**< high-priority queue */
	CFloodControl *m_FloodControl; /**< flood control object for the IRC connection */

	time_t m_LastResponse; /**< a TS which describes when the last line was received from the server */

	bool m_EatPong; /**< whether to ignore the next PONG event from the IRC server */

	CChannel *AddChannel(const char *Channel);
	void RemoveChannel(const char *Channel);

	void UpdateChannelConfig(void);
	void UpdateHostHelper(const char *Host);
	void UpdateWhoHelper(const char *Nick, const char *Realname, const char *Server);

	bool ModuleEvent(int ArgC, const char **ArgV);

	void WriteUnformattedLine(const char *Line);

	virtual int Read(void);
	virtual void Error(int ErrorValue);
	virtual int Write(void);
	virtual bool HasQueuedData(void) const;
	virtual const char *GetClassName(void) const;

	bool ParseLineArgV(int ArgC, const char **ArgV);

	void AsyncDnsFinished(hostent *Response);
	void AsyncBindIpDnsFinished(hostent *Response);
public:
#ifndef SWIG
	CIRCConnection(const char *Host, unsigned int Port, CUser *Owner, const char *BindIp, bool SSL = false, int Family = AF_INET);
	virtual ~CIRCConnection(void);
#endif /* SWIG */

	CChannel *GetChannel(const char *Name);
	CHashtable<CChannel *, false> *GetChannels(void);

	const char *GetCurrentNick(void) const;
	const char *GetSite(void) /* const */;
	const char *GetServer(void) const;

	const char *GetServerVersion(void) const;
	const char *GetServerFeat(void) const;
	const char *GetServerUserModes(void) const;
	const char *GetServerChanModes(void) const;

	CQueue *GetQueueHigh(void);
	CQueue *GetQueueMiddle(void);
	CQueue *GetQueueLow(void);

	CFloodControl *GetFloodControl(void);

	const CHashtable<char *, false> *GetISupportAll(void) const;
	const char *GetISupport(const char *Feature) const;
	void SetISupport(const char *Feature, const char *Value);
	bool IsChanMode(char Mode) const;
	int RequiresParameter(char Mode) const;
	bool IsNickPrefix(char Char) const;
	bool IsNickMode(char Char) const;
	char PrefixForChanMode(char Mode) const;
	char GetHighestUserFlag(const char *Modes) const;

	void ParseLine(const char *Line);

	void JoinChannels(void);

	void Destroy(void);

	virtual int SSLVerify(int PreVerifyOk, X509_STORE_CTX *Context) const;

	void Kill(const char *Error);

	const char *GetUsermodes(void);

	int GetState(void);
};

#endif /* IRCCONNECTION_H */
