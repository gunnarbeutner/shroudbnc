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
class CConfig;
class CChannel;
class CQueue;
class CFloodControl;
class CTimer;
class CAssocArray;

#ifndef SWIG
bool DelayJoinTimer(time_t Now, void* IRCConnection);
bool IRCPingTimer(time_t Now, void* IRCConnection);
#endif

#ifdef SWIGINTERFACE
%template(COwnedObjectCUser) COwnedObject<class CUser>;
%template(CZoneObjectCIRCConnection) CZoneObject<class CIRCConnection, 16>;
#endif

/**
 * CIRCConnection
 *
 * An IRC connection.
 */
class CIRCConnection : public CConnection, public COwnedObject<CUser>, public CZoneObject<CIRCConnection, 16> {
#ifndef SWIG
	friend bool DelayJoinTimer(time_t Now, void* IRCConnection);
	friend bool IRCPingTimer(time_t Now, void* IRCConnection);
#endif

	connection_state_e m_State; /**< the current status of the IRC connection */

	char *m_CurrentNick; /**< the current nick for this IRC connection */
	char *m_Site; /**< the ident\@host of this IRC connection */
	char *m_Server; /**< the hostname of the IRC server */

	CHashtable<CChannel *, false, 16> *m_Channels; /**< the channels this IRC user is on */

	char *m_ServerVersion; /**< the version from the 351 reply */
	char *m_ServerFeat; /**< the server features from the 351 reply */

	CConfig *m_ISupport; /**< the key/value pairs from the 005 replies */
	
	CTimer *m_DelayJoinTimer; /**< timer for delay-joining channels */
	CTimer *m_PingTimer; /**< timer for sending regular PINGs to the server */

	CQueue *m_QueueLow; /**< low-priority queue */
	CQueue *m_QueueMiddle; /**< middle-priority queue */
	CQueue *m_QueueHigh; /**< high-priority queue */
	CFloodControl *m_FloodControl; /**< flood control object for the IRC connection */

	time_t m_LastResponse; /**< a TS which describes when the last line was received from the server */

	CChannel *AddChannel(const char* Channel);
	void RemoveChannel(const char* Channel);

	void UpdateChannelConfig(void);
	void UpdateHostHelper(const char *Host);
	void UpdateWhoHelper(const char *Nick, const char *Realname, const char *Server);

	bool ModuleEvent(int ArgC, const char **ArgV);

	void InitIrcConnection(CUser *Owner, bool Unfreezing = false);

	void WriteUnformattedLine(const char *Line);

	virtual bool Read(void);
	virtual void Error(void);
	virtual void Write(void);
	virtual bool HasQueuedData(void) const;
	virtual const char *GetClassName(void) const;

	bool ParseLineArgV(int ArgC, const char **ArgV);

	void AsyncDnsFinished(hostent *Response);
	void AsyncBindIpDnsFinished(hostent *Response);
public:
#ifndef SWIG
	CIRCConnection(SOCKET Socket, CUser *Owner, bool SSL = false);
	CIRCConnection(const char* Host, unsigned short Port, CUser* Owner, const char* BindIp, bool SSL = false, int Family = AF_INET);
	virtual ~CIRCConnection();

	bool Freeze(CAssocArray *Box);
	static CIRCConnection *Thaw(CAssocArray *Box, CUser *Owner);
#endif

	virtual CChannel *GetChannel(const char *Name);
	virtual CHashtable<CChannel *, false, 16> *GetChannels(void);

	virtual const char *GetCurrentNick(void) const;
	virtual const char *GetSite(void) const;
	virtual const char *GetServer(void) const;

	virtual const char *GetServerVersion(void) const;
	virtual const char *GetServerFeat(void) const;

	virtual CQueue *GetQueueHigh(void);
	virtual CQueue *GetQueueMiddle(void);
	virtual CQueue *GetQueueLow(void);

	virtual CFloodControl *GetFloodControl(void);

	virtual const CConfig *GetISupportAll(void) const;
	virtual const char *GetISupport(const char *Feature) const;
	virtual bool IsChanMode(char Mode) const;
	virtual int RequiresParameter(char Mode) const;
	virtual bool IsNickPrefix(char Char) const;
	virtual bool IsNickMode(char Char) const;
	virtual char PrefixForChanMode(char Mode) const;
	virtual char GetHighestUserFlag(const char *Modes) const;

	virtual void ParseLine(const char *Line);

	virtual void JoinChannels(void);

	virtual void Destroy(void);

	virtual int SSLVerify(int PreVerifyOk, X509_STORE_CTX *Context) const;

	virtual void Kill(const char *Error);
};
