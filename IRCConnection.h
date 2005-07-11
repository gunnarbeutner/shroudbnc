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

#if !defined(AFX_IRCCONNECTION_H__219E3E6C_0C55_4167_A663_D9098377ECE6__INCLUDED_)
#define AFX_IRCCONNECTION_H__219E3E6C_0C55_4167_A663_D9098377ECE6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

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
#endif

class CIRCConnection : public CConnection {
#ifndef SWIG
	friend bool DelayJoinTimer(time_t Now, void* IRCConnection);
#endif

	connection_state_e m_State;

	char* m_CurrentNick;
	char* m_Server;

	CHashtable<CChannel*, false, 5>* m_Channels;

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

	void AddChannel(const char* Channel);
	void RemoveChannel(const char* Channel);

	void UpdateChannelConfig(void);
	void UpdateHostHelper(const char* Host);

	bool ModuleEvent(int argc, const char** argv);
public:
#ifndef SWIG
	CIRCConnection(SOCKET Socket, sockaddr_in Peer, CBouncerUser* Owning);
	~CIRCConnection();
#endif

	virtual void Write(void);
	virtual bool HasQueuedData(void);
	virtual void InternalWriteLine(const char* In);

	virtual connection_role_e GetRole(void);

	virtual CChannel* GetChannel(const char* Name);
	virtual CHashtable<CChannel*, false, 5>* GetChannels(void);

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
};

#endif // !defined(AFX_IRCCONNECTION_H__219E3E6C_0C55_4167_A663_D9098377ECE6__INCLUDED_)
