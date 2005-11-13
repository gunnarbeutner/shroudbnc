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

enum connection_state_e {
	State_Unknown,
	State_Connecting,
	State_Pong,
	State_Connected
};

class CBouncerUser;
class CBouncerConfig;
class CChannel;
class CQueue;
class CFloodControl;
class CTimer;

#ifndef SWIG
bool DelayJoinTimer(time_t Now, void* IRCConnection);
bool IRCPingTimer(time_t Now, void* IRCConnection);
bool IrcAdnsTimeoutTimer(time_t Now, void* IRC);
#endif

class CIRCConnection : public CConnection, public CDnsEvents {
#ifndef SWIG
	friend bool DelayJoinTimer(time_t Now, void* IRCConnection);
	friend bool IRCPingTimer(time_t Now, void* IRCConnection);
	friend bool IRCAdnsTimeoutTimer(time_t Now, void* IRC);
#endif

	connection_state_e m_State;

	char* m_CurrentNick;
	char* m_Server;

	CHashtable<CChannel*, false, 16, false>* m_Channels;

	char* m_ServerVersion;
	char* m_ServerFeat;

	CBouncerConfig* m_ISupport;

	time_t m_LastBurst;
	
	CTimer* m_DelayJoinTimer;
	CTimer* m_PingTimer;

	CQueue* m_QueueLow;
	CQueue* m_QueueMiddle;
	CQueue* m_QueueHigh;
	CFloodControl* m_FloodControl;

	CTimer* m_AdnsTimeout;

	unsigned short m_PortCache;
	char* m_BindIpCache;

	bool m_LatchedDestruction;

	adns_query* m_AdnsQuery;

	char* m_Site;

	void AddChannel(const char* Channel);
	void RemoveChannel(const char* Channel);

	void UpdateChannelConfig(void);
	void UpdateHostHelper(const char* Host);

	bool ModuleEvent(int argc, const char** argv);

	void InitIrcConnection(CBouncerUser* Owning);
public:
#ifndef SWIG
	CIRCConnection(SOCKET Socket, CBouncerUser* Owning);
	CIRCConnection(const char* Host, unsigned short Port, CBouncerUser* Owning, const char* BindIp);
#endif
	virtual ~CIRCConnection();

	virtual void Write(void);
	virtual bool HasQueuedData(void);
	virtual void InternalWriteLine(const char* In);

	virtual connection_role_e GetRole(void);

	virtual CChannel* GetChannel(const char* Name);
	virtual CHashtable<CChannel*, false, 16, false>* GetChannels(void);

	virtual const char* GetCurrentNick(void);
	virtual const char* GetServer(void);

	virtual void Destroy(void);
	virtual const char* ClassName(void);

	virtual bool IsOnChannel(const char* Channel);

	virtual char* NickFromHostmask(const char* Hostmask);
	virtual void FreeNick(char* Nick);

	virtual CBouncerConfig* GetISupportAll(void);
	virtual const char* GetISupport(const char* Feature);
	virtual bool IsChanMode(char Mode);
	virtual int RequiresParameter(char Mode);

	virtual bool IsNickPrefix(char Char);
	virtual bool IsNickMode(char Char);
	virtual char PrefixForChanMode(char Mode);

	virtual void ParseLine(const char* Line);
	virtual bool ParseLineArgV(int argc, const char** argv);

	virtual const char* GetServerVersion(void);
	virtual const char* GetServerFeat(void);

	virtual CQueue* GetQueueHigh(void);
	virtual CQueue* GetQueueMiddle(void);
	virtual CQueue* GetQueueLow(void);

	virtual CFloodControl* GetFloodControl(void);

	virtual void JoinChannels(void);

	virtual bool Read(void);

	virtual void AsyncDnsFinished(adns_query* query, adns_answer* response);
	virtual void AdnsTimeout(void);

	virtual bool ShouldDestroy(void);

	virtual const char* GetSite(void);
};
