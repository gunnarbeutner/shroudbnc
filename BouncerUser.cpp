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
#include "Hashtable.h"
#include "SocketEvents.h"
#include "DnsEvents.h"
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
#include "Match.h"
#include "TrafficStats.h"
#include "Keyring.h"

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

	m_Quitted = false;

	m_BadLogins = NULL;
	m_BadLoginCount = 0;

	m_HostAllows = NULL;
	m_HostAllowCount = 0;

	unsigned int i = 0;
	while (true) {
		snprintf(Out, sizeof(Out), "user.hosts.host%d", i++);

		const char* Hostmask = m_Config->ReadString(Out);

		if (Hostmask)
			AddHostAllow(Hostmask, false);
		else
			break;
	}

	m_ClientStats = new CTrafficStats();
	m_IRCStats = new CTrafficStats();

	m_Keys = new CKeyring(m_Config);

	m_BadLoginPulse = g_Bouncer->CreateTimer(200, true, BadLoginTimer, this);
	m_ReconnectTimer = NULL;

	ScheduleReconnect(0);
}

void CBouncerUser::LoadEvent(void) {
	for (int i = 0; i < g_Bouncer->GetModuleCount(); i++) {
		CModule* M = g_Bouncer->GetModules()[i];

		if (M) {
			M->UserLoad(m_Name);
		}
	}
}

CBouncerUser::~CBouncerUser() {
	if (m_Client)
		m_Client->Kill("Removing user.");

	delete m_Config;

	free(m_Name);

	if (m_IRC)
		m_IRC->Kill("fish go moo.");

	delete m_Log;

	delete m_ClientStats;
	delete m_IRCStats;

	delete m_Keys;

	free(m_BadLogins);

	m_BadLoginPulse->Destroy();

	if (m_ReconnectTimer)
		delete m_ReconnectTimer;
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

	if (IsLocked()) {
		Client->Kill("*** You cannot attach to this user.");
		return;
	}

	Client->m_Owner = this;

	SetClientConnection(Client, true);

	if (m_IRC) {
		const char* IrcNick = m_IRC->GetCurrentNick();

		if (IrcNick) {
			Client->WriteLine(":%s!ident@sbnc NICK :%s", Client->GetNick(), IrcNick);

			if (Client->GetNick() && strcmp(Client->GetNick(), IrcNick) != 0)
				m_IRC->WriteLine("NICK :%s", Client->GetNick());

			m_Client->WriteLine(":%s 001 %s :Welcome to the Internet Relay Network %s", m_IRC->GetServer(), IrcNick, IrcNick);

			m_Client->WriteLine(":%s 422 %s :MOTD File is missing", m_IRC->GetServer(), IrcNick);

			m_Client->ParseLine("VERSION");

			int a = 0;

			while (xhash_t<CChannel*>* Chan = m_IRC->GetChannels()->Iterate(a++)) {
				m_Client->WriteLine(":%s!%s@bouncer JOIN %s", m_IRC->GetCurrentNick(), m_Name, Chan->Name);

				snprintf(Out, sizeof(Out), "NAMES %s", Chan->Name);
				m_Client->ParseLine(Out);

				snprintf(Out, sizeof(Out), "TOPIC %s", Chan->Name);
				m_Client->ParseLine(Out);
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

	for (int i = 0; i < g_Bouncer->GetModuleCount(); i++) {
		CModule* M = g_Bouncer->GetModules()[i];

		if (M) {
			M->AttachClient(GetUsername());
		}
	}

	if (m_IRC == NULL && GetServer() == NULL)
		Notice("You haven't set a server yet. Use /sbnc set server <Hostname> <Port> to do that now.");

	if (!GetLog()->IsEmpty())
		Notice("You have new messages. Use '/msg -sBNC read' to view them.");
}

bool CBouncerUser::Validate(const char* Password) {
	const char* RealPass = m_Config->ReadString("user.password");

	if (!RealPass || strlen(RealPass) == 0)
		return false;

	if (g_Bouncer->GetConfig()->ReadInteger("system.md5"))
		Password = g_Bouncer->MD5(Password);

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

		SetIRCConnection(NULL);
	}

	char Out[1024];

	m_ReconnectTimer = NULL;

	const char* Server = m_Config->ReadString("user.server");
	int Port = m_Config->ReadInteger("user.port");

	if (!Server || !Port) {
		snprintf(Out, sizeof(Out), "%s has no default server. Can't (re)connect.", m_Name);

		ScheduleReconnect(120);

		g_Bouncer->GetLog()->InternalWriteLine(Out);
		g_Bouncer->GlobalNotice(Out, true);

		return;
	}

	snprintf(Out, sizeof(Out), "Trying to reconnect to %s:%d", Server, Port);
	g_Bouncer->GetLog()->InternalWriteLine(Out);
	Notice(Out);

	snprintf(Out, sizeof(Out), "Trying to reconnect to %s:%d for %s", Server, Port, m_Name);
	g_Bouncer->GlobalNotice(Out, true);

	m_LastReconnect = time(NULL);

	g_Bouncer->SetIdent(m_Name);

	const char* BindIp = BindIp = m_Config->ReadString("user.ip");

	if (!BindIp)
		BindIp = g_Bouncer->GetConfig()->ReadString("system.ip");

	CIRCConnection* Connection = CreateIRCConnection(Server, Port, this, BindIp);

	if (!Connection) {
		Notice("Can't connect..");
		g_Bouncer->Log("Can't connect..");
		g_Bouncer->GlobalNotice("Failed to connect.", true);
	} else {
		SetIRCConnection(Connection);

		g_Bouncer->Log("Connected!");
		g_Bouncer->GlobalNotice("Connected!", true);
	}
}

bool CBouncerUser::ShouldReconnect(void) {
	if (!m_IRC && !m_Quitted && m_ReconnectTime < time(NULL) && time(NULL) - m_LastReconnect > 120 && time(NULL) - g_LastReconnect > 15)
		return true;
	else
		return false;
}

void CBouncerUser::ScheduleReconnect(int Delay) {
	if (m_IRC)
		return;

	if (m_Quitted && Delay == 0)
		m_Quitted = false;

	int MaxDelay = Delay;

	if (time(NULL) - g_LastReconnect < 20 && MaxDelay < 20)
		MaxDelay = 20;

	if (time(NULL) - m_LastReconnect < 120 && MaxDelay < 120)
		MaxDelay = 120;

	if (m_ReconnectTime < time(NULL) + MaxDelay) {
		if (m_ReconnectTimer)
			m_ReconnectTimer->Destroy();

		m_ReconnectTimer = g_Bouncer->CreateTimer(MaxDelay, false, UserReconnectTimer, this);

		m_ReconnectTime = time(NULL) + MaxDelay;

		if (GetServer()) {
			char Out[1024];
			snprintf(Out, sizeof(Out), "Scheduled reconnect in %d seconds.", MaxDelay);
			Notice(Out);
		}
	}
}

void CBouncerUser::Notice(const char* Text) {
	if (m_Client)
		m_Client->WriteLine(":-sBNC!core@bnc.server PRIVMSG %s :%s", GetNick(), Text);
}

void CBouncerUser::RealNotice(const char* Text) {
	if (m_Client)
		m_Client->WriteLine(":-sBNC!core@bnc.server NOTICE %s :%s", GetNick(), Text);
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
	m_Config->WriteInteger("user.lock", 1);
}

void CBouncerUser::Unlock(void) {
	m_Config->WriteInteger("user.lock", 0);
}

void CBouncerUser::SetIRCConnection(CIRCConnection* IRC) {
	m_IRC = IRC;

	if (!IRC) {
		Notice("Disconnected from the server.");

		for (int i = 0; i < g_Bouncer->GetModuleCount(); i++) {
			CModule* M = g_Bouncer->GetModules()[i];

			if (M) {
				M->ServerDisconnect(GetUsername());
			}
		}

		ScheduleReconnect(10);
	} else {
		for (int i = 0; i < g_Bouncer->GetModuleCount(); i++) {
			CModule* M = g_Bouncer->GetModules()[i];

			if (M) {
				M->ServerConnect(GetUsername());
			}
		}

		IRC->AttachStats(m_IRCStats);
	}
}

void CBouncerUser::SetClientConnection(CClientConnection* Client, bool DontSetAway) {
	if (!m_Client && !Client)
		return;

	char Out[1024];

	if (!m_Client) {
		snprintf(Out, sizeof(Out), "User %s logged on.", GetUsername());

		g_Bouncer->GetLog()->InternalWriteLine(Out);
		g_Bouncer->GlobalNotice(Out, true);
	}


	if (m_Client && Client) {
		snprintf(Out, sizeof(Out), "Seamless transition for %s", GetUsername());
		g_Bouncer->GlobalNotice(Out, true);
		g_Bouncer->GetLog()->InternalWriteLine(Out);

		m_Client->SetOwner(NULL);
		m_Client->Kill("Another client has connected.");
	}

	m_Client = Client;

	if (!Client) {
		if (!DontSetAway) {
			snprintf(Out, sizeof(Out), "User %s logged off.", GetUsername());
			g_Bouncer->GlobalNotice(Out, true);
			g_Bouncer->GetLog()->InternalWriteLine(Out);
		}

		for (int i = 0; i < g_Bouncer->GetModuleCount(); i++) {
			CModule* M = g_Bouncer->GetModules()[i];

			if (M) {
				M->DetachClient(GetUsername());
			}
		}

		if (m_IRC && !DontSetAway) {
			const char* Offnick = m_Config->ReadString("user.awaynick");

			if (Offnick)
				m_IRC->WriteLine("NICK %s", Offnick);

			const char* Away = m_Config->ReadString("user.away");

			if (Away)
				m_IRC->WriteLine("AWAY :%s", Away);
		}
	} else
		Client->AttachStats(m_ClientStats);
}

void CBouncerUser::SetAdmin(bool Admin) {
	m_Config->WriteString("user.admin", Admin ? "1" : "0");
}

bool CBouncerUser::IsAdmin(void) {
	int Admin = m_Config->ReadInteger("user.admin");

	return Admin != 0;
}

void CBouncerUser::SetPassword(const char* Password) {
	if (g_Bouncer->GetConfig()->ReadInteger("system.md5"))
		Password = g_Bouncer->MD5(Password);

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
	return m_Config->ReadInteger("user.lock") != 0;
}

void CBouncerUser::SetNick(const char* Nick) {
	m_Config->WriteString("user.nick", Nick);
}

void CBouncerUser::SetRealname(const char* Realname) {
	if (Realname)
		m_Config->WriteString("user.realname", Realname);
}

void CBouncerUser::MarkQuitted(void) {
	m_Quitted = true;
}

void CBouncerUser::LogBadLogin(sockaddr_in Peer) {
	for (unsigned int i = 0; i < m_BadLoginCount; i++) {
#ifdef _WIN32
		if (m_BadLogins[i].ip.sin_addr.S_un.S_addr == Peer.sin_addr.S_un.S_addr) {
#else
		if (m_BadLogins[i].ip.sin_addr.s_addr == Peer.sin_addr.s_addr) {
#endif
			m_BadLogins[i].count++;

			return;
		}
	}

	for (unsigned int a = 0; a < m_BadLoginCount; a++) {
		if (m_BadLogins[a].count == 0) {
			m_BadLogins[a].ip = Peer;
			m_BadLogins[a].count = 1;

			return;
		}
	}

	m_BadLogins = (badlogin_t*)realloc(m_BadLogins, sizeof(badlogin_t) * ++m_BadLoginCount);

	m_BadLogins[m_BadLoginCount - 1].ip = Peer;
	m_BadLogins[m_BadLoginCount - 1].count = 1;
}

bool CBouncerUser::IsIpBlocked(sockaddr_in Peer) {
	for (unsigned int i = 0; i < m_BadLoginCount; i++) {
#ifdef _WIN32
		if (m_BadLogins[i].ip.sin_addr.S_un.S_addr == Peer.sin_addr.S_un.S_addr) {
#else
		if (m_BadLogins[i].ip.sin_addr.s_addr == Peer.sin_addr.s_addr) {
#endif
			if (m_BadLogins[i].count > 2)
				return true;
			else
				return false;
		}
	}

	return false;
}

void CBouncerUser::BadLoginPulse(void) {
	for (unsigned int i = 0; i < m_BadLoginCount; i++) {
		if (m_BadLogins[i].count)
			m_BadLogins[i].count--;
	}
}

void CBouncerUser::AddHostAllow(const char* Mask, bool UpdateConfig) {
	for (unsigned int i = 0; i < m_HostAllowCount; i++) {
		if (!m_HostAllows[i]) {
			m_HostAllows[i] = strdup(Mask);

			if (UpdateConfig)
				UpdateHosts();

			return;
		}
	}

	m_HostAllows = (char**)realloc(m_HostAllows, sizeof(char*) * ++m_HostAllowCount);
	m_HostAllows[m_HostAllowCount - 1] = strdup(Mask);

	if (UpdateConfig)
		UpdateHosts();
}

void CBouncerUser::RemoveHostAllow(const char* Mask, bool UpdateConfig) {
	for (unsigned int i = 0; i < m_HostAllowCount; i++) {
		if (m_HostAllows[i] && strcmpi(m_HostAllows[i], Mask) == 0) {
			free(m_HostAllows[i]);
			m_HostAllows[i] = NULL;

			if (UpdateConfig)
				UpdateHosts();

			return;
		}
	}
}

char** CBouncerUser::GetHostAllows(void) {
	return m_HostAllows;
}

unsigned int CBouncerUser::GetHostAllowCount(void) {
	return m_HostAllowCount;
}

bool CBouncerUser::CanHostConnect(const char* Host) {
	unsigned int c = 0;

	for (unsigned int i = 0; i < m_HostAllowCount; i++) {
		if (m_HostAllows[i])
			if (mmatch(m_HostAllows[i], Host) == 0)
				return true;
			else
				c++;
	}

	if (c > 0)
		return false;
	else
		return true;	
}

void CBouncerUser::UpdateHosts(void) {
	char Out[1024];
	int a = 0;

	for (unsigned int i = 0; i < m_HostAllowCount; i++) {
		if (m_HostAllows[i]) {
			snprintf(Out, sizeof(Out), "user.hosts.host%d", a++);
			m_Config->WriteString(Out, m_HostAllows[i]);
		}
	}

	snprintf(Out, sizeof(Out), "user.hosts.host%d", a);

	m_Config->WriteString(Out, NULL);
}

CTrafficStats* CBouncerUser::GetClientStats(void) {
	return m_ClientStats;
}

CTrafficStats* CBouncerUser::GetIRCStats(void) {
	return m_IRCStats;
}

CKeyring* CBouncerUser::GetKeyring(void) {
	return m_Keys;
}

bool BadLoginTimer(time_t Now, void* User) {
	((CBouncerUser*)User)->BadLoginPulse();

	return true;
}

bool UserReconnectTimer(time_t Now, void* User) {
	if (((CBouncerUser*)User)->GetIRCConnection())
		return false;

	((CBouncerUser*)User)->m_ReconnectTimer = NULL;

	if (time(NULL) - g_LastReconnect > 15)
		((CBouncerUser*)User)->Reconnect();
	else
		((CBouncerUser*)User)->ScheduleReconnect(10);

	return false;
}
