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

#include "StdAfx.h"
#include "SocketEvents.h"
#include "Connection.h"
#include "ClientConnection.h"
#include "IRCConnection.h"
#include "BouncerConfig.h"
#include "BouncerUser.h"
#include "BouncerCore.h"
#include "BouncerLog.h"
#include "Channel.h"
#include "ModuleFar.h"
#include "Module.h"
#include "utility.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

extern time_t g_LastReconnect;

CBouncerUser::CBouncerUser(const char* Name) {
	m_Client = NULL;
	m_IRC = NULL;
	m_Name = strdup(Name);

	assert(m_Name != NULL);

	char Out[1024];
	snprintf(Out, sizeof(Out), "users/%s.conf", Name);

	m_Config = new CBouncerConfig(Out);

	assert(m_Config != NULL);

	m_IRC = NULL;

	m_ReconnectTime = 0;
	m_LastReconnect = 0;

	snprintf(Out, sizeof(Out), "users/%s.log", Name);

	m_Log = new CBouncerLog(Out);

	assert(m_Log != NULL);

	m_Locked = false;
}

CBouncerUser::~CBouncerUser() {
	if (m_Client)
		delete m_Client;

	delete m_Config;

	free(m_Name);

	if (m_IRC) {
		m_IRC->Kill("fish go moo.");

		delete m_IRC;
	}

	delete m_Log;
}

SOCKET CBouncerUser::GetIRCSocket(void) {
	if (m_IRC)
		return m_IRC->GetSocket();
	else
		return INVALID_SOCKET;
}

CClientConnection* CBouncerUser::GetClientConnection(void) {
	return m_Client;
}

CIRCConnection* CBouncerUser::GetIRCConnection(void) {
	return m_IRC;
}

bool CBouncerUser::IsConnectedToIRC(void) {
	return m_IRC != NULL;
}

void CBouncerUser::Attach(CClientConnection* Client) {
	char Out[1024];

	if (m_Locked) {
		Client->Kill("You cannot attach to this user.");
		return;
	}

	if (m_Client) {
		m_Client->Kill("Another client has connected.");

		SetClientConnection(NULL);
	}

	snprintf(Out, sizeof(Out), "User %s logged on.", GetUsername());

	for (int i = 0; i < g_Bouncer->GetModuleCount(); i++) {
		CModule* M = g_Bouncer->GetModules()[i];

		if (M) {
			M->AttachClient(GetUsername());
		}
	}

	g_Bouncer->Log(Out);
	g_Bouncer->GlobalNotice(Out, true);

	m_Client = Client;
	Client->m_Owner = this;

	if (m_IRC) {
		const char* IrcNick = m_IRC->GetCurrentNick();

		if (IrcNick) {
			Client->WriteLine(":%s!ident@sbnc NICK :%s", Client->GetNick(), IrcNick);

			if (Client->GetNick() && strcmp(Client->GetNick(), IrcNick) != 0)
				m_IRC->WriteLine("NICK :%s", Client->GetNick());

			m_Client->WriteLine(":%s 001 %s :Welcome to the Internet Relay Network %s", m_IRC->GetServer(), IrcNick, IrcNick);

			m_Client->ParseLine("VERSION");

			CChannel** Chans = m_IRC->GetChannels();
			int Count = m_IRC->GetChannelCount();

			for (int i = 0; i < Count; i++) {
				if (Chans[i]) {
					m_Client->WriteLine(":%s!%s@bouncer JOIN %s", m_IRC->GetCurrentNick(), m_Name, Chans[i]->GetName());

					snprintf(Out, sizeof(Out), "NAMES %s", Chans[i]->GetName());
					m_Client->ParseLine(Out);

					snprintf(Out, sizeof(Out), "TOPIC %s", Chans[i]->GetName());
					m_Client->ParseLine(Out);
				}
			}
		}

		m_IRC->InternalWriteLine("AWAY");
	} else
		ScheduleReconnect(0);

	const char* Motd = g_Bouncer->GetConfig()->ReadString("system.motd");

	if (Motd && *Motd) {
		snprintf(Out, sizeof(Out), "Message of the day: %s", Motd);
		
		Notice(Out);
	}

	if (!GetLog()->IsEmpty())
		Notice("You have new messages. Use /PLAYPRIVATELOG to view them.");
}

bool CBouncerUser::Validate(const char* Password) {
	const char* RealPass = m_Config->ReadString("user.password");

	if (!RealPass || strlen(RealPass) == 0)
		return false;

	return (strcmp(RealPass, Password) == 0);
}

const char* CBouncerUser::GetNick(void) {
	if (m_Client)
		return m_Client->GetNick();
	else if (m_IRC && m_IRC->GetCurrentNick())
		return m_IRC->GetCurrentNick();
	else {
		const char* Nick = m_Config->ReadString("user.awaynick");

		if (Nick)
			return Nick;
		
		Nick = m_Config->ReadString("user.nick");

		if (Nick)
			return Nick;
		else
			return m_Name;
	}
}

const char* CBouncerUser::GetUsername(void) {
	return m_Name;
}

const char* CBouncerUser::GetRealname(void) {
	const char* Out = m_Config->ReadString("user.realname");

	if (!Out)
		return "shroudBNC User";
	else
		return Out;
}

CBouncerConfig* CBouncerUser::GetConfig(void) {
	return m_Config;
}

void CBouncerUser::Simulate(const char* Command) {
	if (!Command)
		return;

	char* C = strdup(Command);

	if (m_Client)
		m_Client->ParseLine(C);
	else {
		sockaddr_in null_peer;
		memset(&null_peer, 0, sizeof(null_peer));
		CClientConnection* FakeClient = new CClientConnection(INVALID_SOCKET, null_peer);
		FakeClient->m_Owner = this;
		FakeClient->ParseLine(C);
		FakeClient->m_Owner = NULL;
		delete FakeClient;
	}

	free(C);
}

void CBouncerUser::Reconnect(void) {
	if (m_IRC) {
		m_IRC->Kill("Reconnecting");
		delete m_IRC;
		m_IRC = NULL;
	}

	char Out[1024];

	const char* Server = m_Config->ReadString("user.server");
	int Port = m_Config->ReadInteger("user.port");

	if (!Server || !Port) {
		snprintf(Out, sizeof(Out), "%s has no default server. Can't (re)connect.", m_Name);

		ScheduleReconnect(120);

		g_Bouncer->Log(Out);
		g_Bouncer->GlobalNotice(Out, true);

		return;
	}

	snprintf(Out, sizeof(Out), "Trying to reconnect to %s:%d", Server, Port);
	g_Bouncer->Log(Out);
	Notice(Out);

	snprintf(Out, sizeof(Out), "Trying to reconnect to %s:%d for %s", Server, Port, m_Name);
	g_Bouncer->GlobalNotice(Out, true);

	m_LastReconnect = time(NULL);

	g_Bouncer->SetIdent(m_Name);

	const char* BindIp = BindIp = m_Config->ReadString("user.ip");

	if (!BindIp)
		BindIp = g_Bouncer->GetConfig()->ReadString("system.ip");

	m_IRC = CreateIRCConnection(Server, Port, this, BindIp);

	if (!m_IRC) {
		Notice("Can't connect..");
		g_Bouncer->Log("Can't connect..");
		g_Bouncer->GlobalNotice("Failed to connect.", true);
	} else {
		g_Bouncer->Log("Connected!");
		g_Bouncer->GlobalNotice("Connected!", true);
	}
}

bool CBouncerUser::ShouldReconnect(void) {
	if (!m_IRC && m_ReconnectTime < time(NULL) && time(NULL) - m_LastReconnect > 120 && time(NULL) - g_LastReconnect > 15)
		return true;
	else
		return false;
}

void CBouncerUser::ScheduleReconnect(int Delay) {
	if (m_IRC)
		return;

	if (m_ReconnectTime < time(NULL) + Delay) {
		m_ReconnectTime = time(NULL) + Delay;
		printf("Scheduled reconnect for %s in %d seconds.\n", m_Name, Delay);

		char Out[1024];
		snprintf(Out, sizeof(Out), "Scheduled reconnect in %d seconds.", Delay);
		Notice(Out);
	}
}

void CBouncerUser::Notice(const char* Text) {
	if (m_Client)
		m_Client->WriteLine(":-sBNC!core@bnc.server PRIVMSG %s :%s", GetNick(), Text);
}

int CBouncerUser::IRCUptime(void) {
	return m_IRC ? (time(NULL) - m_LastReconnect) : 0;
}

CBouncerLog* CBouncerUser::GetLog(void) {
	return m_Log;
}

void CBouncerUser::Log(const char* Format, ...) {
	char Out[1024];
	va_list marker;

	va_start(marker, Format);
	vsnprintf(Out, sizeof(Out), Format, marker);
	va_end(marker);

	m_Log->InternalWriteLine(Out);
}


void CBouncerUser::Lock(void) {
	m_Locked = true;
}

void CBouncerUser::Unlock(void) {
	m_Locked = false;
}

void CBouncerUser::SetIRCConnection(CIRCConnection* IRC) {
	m_IRC = IRC;

	if (!IRC)
		Notice("Disconnected from the server.");
}

void CBouncerUser::SetClientConnection(CClientConnection* Client) {
	m_Client = Client;

	if (!Client) {
		char Out[1024];

		snprintf(Out, sizeof(Out), "User %s logged off.", GetUsername());
		g_Bouncer->GlobalNotice(Out, true);

		for (int i = 0; i < g_Bouncer->GetModuleCount(); i++) {
			CModule* M = g_Bouncer->GetModules()[i];

			if (M) {
				M->DetachClient(GetUsername());
			}
		}

		if (m_IRC) {
			const char* Offnick = m_Config->ReadString("user.awaynick");

			if (Offnick)
				m_IRC->WriteLine("NICK %s", Offnick);

			const char* Away = m_Config->ReadString("user.away");

			if (Away)
				m_IRC->WriteLine("AWAY :%s", Away);
		}
	}
}

void CBouncerUser::SetAdmin(bool Admin) {
	m_Config->WriteString("user.admin", Admin ? "1" : "0");
}

bool CBouncerUser::IsAdmin(void) {
	int Admin = m_Config->ReadInteger("user.admin");

	return Admin != 0;
}

void CBouncerUser::SetPassword(const char* Password) {
	m_Config->WriteString("user.password", Password);
}

void CBouncerUser::SetServer(const char* Server) {
	m_Config->WriteString("user.server", Server);
}

const char* CBouncerUser::GetServer(void) {
	const char* Ptr = m_Config->ReadString("user.server");

	return Ptr;
}

void CBouncerUser::SetPort(int Port) {
	m_Config->WriteInteger("user.port", Port);
}

int CBouncerUser::GetPort(void) {
	int Port = m_Config->ReadInteger("user.port");

	if (!Port)
		return 6667;
	else
		return Port;
}

bool CBouncerUser::IsLocked(void) {
	return m_Locked;
}

void CBouncerUser::SetNick(const char* Nick) {
	m_Config->WriteString("user.nick", Nick);
}

void CBouncerUser::SetRealname(const char* Realname) {
	if (Realname)
		m_Config->WriteString("user.realname", Realname);
}
