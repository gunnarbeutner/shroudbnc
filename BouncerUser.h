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

#if !defined(AFX_BOUNCERUSER_H__4861F444_EA24_49F0_83CA_AC12AD2A977B__INCLUDED_)
#define AFX_BOUNCERUSER_H__4861F444_EA24_49F0_83CA_AC12AD2A977B__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CClientConnection;
class CIRCConnection;
class CBouncerConfig;
class CBouncerLog;

typedef struct badlogin_s {
	sockaddr_in ip;
	unsigned int count;
} badlogin_t;

class CBouncerUser {
	friend class CBouncerCore;

	CClientConnection* m_Client;
	CIRCConnection* m_IRC;
	CBouncerConfig* m_Config;
	CBouncerLog* m_Log;

	char* m_Name;

	time_t m_ReconnectTime;
	time_t m_LastReconnect;

	bool m_Locked;
	bool m_Quitted;

	badlogin_t* m_BadLogins;
	unsigned int m_BadLoginCount;
public:
	CBouncerUser(const char* Name);
	virtual ~CBouncerUser(void);

	virtual SOCKET GetIRCSocket(void);
	virtual CClientConnection* GetClientConnection(void);
	virtual CIRCConnection* GetIRCConnection(void);
	virtual bool IsConnectedToIRC(void);

	virtual bool Validate(const char* Password);
	virtual void Attach(CClientConnection* Client);

	virtual const char* GetNick(void);
	virtual void SetNick(const char* Nick);

	virtual const char* GetRealname(void);
	virtual void SetRealname(const char* Realname);

	
	virtual const char* GetUsername(void);
	virtual CBouncerConfig* GetConfig(void);

	virtual void Simulate(const char* Command);

	virtual void Reconnect(void);

	virtual bool ShouldReconnect(void);
	virtual void ScheduleReconnect(int Delay = 10);

	virtual void Notice(const char* Text);
	virtual void RealNotice(const char* Text);

	virtual int IRCUptime(void);

	virtual CBouncerLog* GetLog(void);
	virtual void Log(const char* Format, ...);

	virtual void Lock(void);
	virtual void Unlock(void);
	virtual bool IsLocked(void);

	virtual void SetIRCConnection(CIRCConnection* IRC);
	void SetClientConnection(CClientConnection* Client, bool DontSetAway = false);

	virtual void SetAdmin(bool Admin = true);
	virtual bool IsAdmin(void);

	virtual void SetPassword(const char* Password);
	
	virtual void SetServer(const char* Server);
	virtual const char* GetServer(void);

	virtual void SetPort(int Port);
	virtual int GetPort(void);

	virtual void MarkQuitted(void);

	virtual void LoadEvent(void);

	virtual void LogBadLogin(sockaddr_in Peer);
	virtual bool IsIpBlocked(sockaddr_in Peer);

	virtual void Pulse(time_t Now);
};

#endif // !defined(AFX_BOUNCERUSER_H__4861F444_EA24_49F0_83CA_AC12AD2A977B__INCLUDED_)
