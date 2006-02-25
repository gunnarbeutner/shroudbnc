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

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

extern time_t g_LastReconnect;

CUser::CUser(const char* Name) {
	m_Client = NULL;
	m_IRC = NULL;
	m_Name = strdup(Name);

	CHECK_ALLOC_RESULT(m_Name, strdup) {
		g_Bouncer->Fatal();
	} CHECK_ALLOC_RESULT_END;

	char* Out;
	asprintf(&Out, "users/%s.conf", Name);

	CHECK_ALLOC_RESULT(Out, asprintf) {
		g_Bouncer->Fatal();
	} CHECK_ALLOC_RESULT_END;

	m_Config = new CConfig(g_Bouncer->BuildPath(Out));

	free(Out);

	CHECK_ALLOC_RESULT(m_Config, new) {
		g_Bouncer->Fatal();
	} CHECK_ALLOC_RESULT_END;

	m_IRC = NULL;

	m_ReconnectTime = 0;
	m_LastReconnect = 0;

	asprintf(&Out, "users/%s.log", Name);

	CHECK_ALLOC_RESULT(Out, asprintf) {
		g_Bouncer->Fatal();
	} CHECK_ALLOC_RESULT_END;

	m_Log = new CLog(g_Bouncer->BuildPath(Out));

	free(Out);

	CHECK_ALLOC_RESULT(m_Log, new) {
		g_Bouncer->Fatal();
	} CHECK_ALLOC_RESULT_END;

	unsigned int i = 0;
	while (true) {
		asprintf(&Out, "user.hosts.host%d", i++);

		CHECK_ALLOC_RESULT(Out, asprintf) {
			g_Bouncer->Fatal();
		} CHECK_ALLOC_RESULT_END;

		const char* Hostmask = m_Config->ReadString(Out);

		free(Out);

		if (Hostmask != NULL) {
			AddHostAllow(Hostmask, false);
		} else {
			break;
		}
	}

	m_ClientStats = new CTrafficStats();
	m_IRCStats = new CTrafficStats();

	m_Keys = new CKeyring(m_Config);

	m_BadLoginPulse = g_Bouncer->CreateTimer(200, true, BadLoginTimer, this);
	m_ReconnectTimer = NULL;

	m_IsAdminCache = -1;

#ifdef USESSL
	asprintf(&Out, "users/%s.pem", Name);

	CHECK_ALLOC_RESULT(Out, asprintf) {
		g_Bouncer->Fatal();
	} CHECK_ALLOC_RESULT_END;

	X509* Cert;
	FILE* ClientCert = fopen(g_Bouncer->BuildPath(Out), "r");

	free(Out);

	if (ClientCert != NULL) {
		while ((Cert = PEM_read_X509(ClientCert, NULL, NULL, NULL)) != NULL) {
			m_ClientCertificates.Insert(Cert);
		}

		fclose(ClientCert);
	}

#endif

	if (m_Config->ReadInteger("user.quitted") != 2) {
		ScheduleReconnect(0);
	}
}

void CUser::LoadEvent(void) {
	CVector<CModule *> *Modules = g_Bouncer->GetModules();

	for (unsigned int i = 0; i < Modules->GetLength(); i++) {
		Modules->Get(i)->UserLoad(m_Name);
	}
}

CUser::~CUser() {
	if (m_Client != NULL) {
		m_Client->Kill("Removing user.");
	}

	if (m_IRC != NULL) {
		m_IRC->Kill("-)(- If you can't see the fnords, they can't eat you.");
	}

	delete m_Config;
	delete m_Log;

	delete m_ClientStats;
	delete m_IRCStats;

	delete m_Keys;

	free(m_Name);

	if (m_BadLoginPulse != NULL) {
		m_BadLoginPulse->Destroy();
	}

#ifdef USESSL
	for (unsigned int i = 0; i < m_ClientCertificates.GetLength(); i++) {
		X509_free(m_ClientCertificates[i]);
	}
#endif

	if (m_ReconnectTimer != NULL) {
		delete m_ReconnectTimer;
	}

	for (unsigned int i = 0; i < m_HostAllows.GetLength(); i++) {
		free(m_HostAllows.Get(i));
	}
}

CClientConnection *CUser::GetClientConnection(void) {
	return m_Client;
}

CIRCConnection *CUser::GetIRCConnection(void) {
	return m_IRC;
}

bool CUser::IsConnectedToIRC(void) {
	if (m_IRC != NULL) {
		return true;
	} else {
		return false;
	}
}

void CUser::Attach(CClientConnection* Client) {
	char* Out;

	if (IsLocked()) {
		const char* Reason = GetSuspendReason();

		if (Reason == NULL)
			Client->Kill("*** You cannot attach to this user.");
		else {
			asprintf(&Out, "*** Your account is suspended. Reason: %s", Reason);

			if (Out == NULL) {
				LOGERROR("asprintf() failed.");

				Client->Kill("*** Your account is suspended.");
			} else {
				Client->Kill(Out);

				free(Out);
			}
		}

		g_Bouncer->Log("Login for user %s failed: User is locked.", m_Name);

		return;
	}

	Client->SetOwner(this);

	SetClientConnection(Client, true);

	CLog *Motd = new CLog("sbnc.motd");

	if (m_IRC) {
		m_IRC->WriteUnformattedLine("AWAY");

		const char* AutoModes = m_Config->ReadString("user.automodes");

		if (AutoModes && *AutoModes)
			m_IRC->WriteLine("MODE %s +%s", m_IRC->GetCurrentNick(), AutoModes);

		const char* IrcNick = m_IRC->GetCurrentNick();

		if (IrcNick) {
			Client->WriteLine(":%s!%s@%s NICK :%s", Client->GetNick(), GetUsername(), Client->GetPeerName(), IrcNick);

			if (Client->GetNick() && strcmp(Client->GetNick(), IrcNick) != 0)
				m_IRC->WriteLine("NICK :%s", Client->GetNick());

			m_Client->WriteLine(":%s 001 %s :Welcome to the Internet Relay Network %s", m_IRC->GetServer(), IrcNick, IrcNick);

			if (Motd->IsEmpty()) {
				m_Client->WriteLine(":%s 422 %s :MOTD File is missing", m_IRC->GetServer(), IrcNick);
			} else{
				Motd->PlayToUser(this, Log_Motd);
			}

			m_Client->ParseLine("VERSION");

			int a = 0;

			char** Keys = m_IRC->GetChannels()->GetSortedKeys();

			while (Keys[a]) {
				m_Client->WriteLine(":%s!%s@%s JOIN %s", m_IRC->GetCurrentNick(), m_Name, m_Client->GetPeerName(), Keys[a]);

				asprintf(&Out, "TOPIC %s", Keys[a]);

				if (Out == NULL) {
					LOGERROR("asprintf() failed.");

					m_Client->Kill("Internal error.");
				} else {
					m_Client->ParseLine(Out);
					free(Out);
				}

				asprintf(&Out, "NAMES %s", Keys[a]);

				if (Out == NULL) {
					LOGERROR("asprintf() failed.");

					m_Client->Kill("Internal error.");
				} else {
					m_Client->ParseLine(Out);
					free(Out);
				}

				a++;
			}

			free(Keys);
		}
	} else {
		if (!Motd->IsEmpty()) {
			Motd->PlayToUser(this, Log_Motd);
		}

		if (m_Config->ReadInteger("user.quitted") != 2) {
			ScheduleReconnect(0);
		}
	}

	const char* MotdText = g_Bouncer->GetConfig()->ReadString("system.motd");

	if (MotdText && *MotdText) {
		asprintf(&Out, "Message of the day: %s", MotdText);
		
		if (Out == NULL) {
			LOGERROR("asprintf() failed.");
		} else {
			Notice(Out);

			free(Out);
		}
	}

	CVector<CModule *> *Modules = g_Bouncer->GetModules();

	for (unsigned int i = 0; i < Modules->GetLength(); i++) {
		Modules->Get(i)->AttachClient(GetUsername());
	}

	if (m_IRC == NULL) {
		if (GetServer() == NULL || *(GetServer()) == '\0') {
			Notice("You haven't set a server yet. Use /sbnc set server <Hostname> <Port> to do that now.");
		} else if (m_Config->ReadInteger("user.quitted") == 2) {
			Notice("You are not connected to an irc server. Use /sbnc jump to reconnect now.");
		}
	}

	if (GetLog()->IsEmpty() == false) {
		Notice("You have new messages. Use '/msg -sBNC read' to view them.");
	}
}

bool CUser::Validate(const char *Password) {
	const char *RealPass = m_Config->ReadString("user.password");

	if (RealPass == NULL || Password == NULL || strlen(Password) == 0) {
		return false;
	}

	if (g_Bouncer->GetConfig()->ReadInteger("system.md5")) {
		Password = g_Bouncer->MD5(Password);
	}

	if (strcmp(RealPass, Password) == 0) {
		return true;
	} else {
		return false;
	}
}

const char *CUser::GetNick(void) {
	if (m_Client != NULL && m_Client->GetNick() != NULL) {
		return m_Client->GetNick();
	} else if (m_IRC != NULL && m_IRC->GetCurrentNick() != NULL) {
		return m_IRC->GetCurrentNick();
	} else {
		const char *Nick = m_Config->ReadString("user.awaynick");

		if (Nick != NULL && Nick[0] != '\0') {
			return Nick;
		}

		Nick = m_Config->ReadString("user.nick");

		if (Nick != NULL && Nick[0] != '\0') {
			return Nick;
		} else {
			return m_Name;
		}
	}
}

const char *CUser::GetUsername(void) {
	return m_Name;
}

const char *CUser::GetRealname(void) {
	const char *Realname = m_Config->ReadString("user.realname");

	if (Realname == NULL) {
		Realname = g_Bouncer->GetConfig()->ReadString("system.realname");

		if (Realname != NULL) {
			return Realname;
		} else {
			return "shroudBNC User";
		}
	} else {
		return Realname;
	}
}

CConfig *CUser::GetConfig(void) {
	return m_Config;
}

void CUser::Simulate(const char *Command, CClientConnection *FakeClient) {
	bool FakeWasNull;
	CClientConnection *OldClient;
	char *CommandDup;
	CUser *OldOwner;

	if (Command == NULL) {
		return;
	}

	if (FakeClient == NULL) {
		FakeWasNull = true;
	} else {
		FakeWasNull = false;
	}

	CommandDup = strdup(Command);

	CHECK_ALLOC_RESULT(CommandDup, strdup) {
		return;
	} CHECK_ALLOC_RESULT_END;

	if (FakeClient == NULL) {
		FakeClient = new CClientConnection(INVALID_SOCKET);

		CHECK_ALLOC_RESULT(FakeClient, new) {
			free(CommandDup);

			return;
		} CHECK_ALLOC_RESULT_END;
	}

	OldClient = m_Client;
	m_Client = FakeClient;

	OldOwner = FakeClient->GetOwner();
	FakeClient->SetOwner(this);

	FakeClient->ParseLine(CommandDup);

	FakeClient->SetOwner(OldOwner);

	m_Client = OldClient;

	if (FakeWasNull) {
		FakeClient->Destroy();
	}

	free(CommandDup);
}

void CUser::Reconnect(void) {
	if (m_IRC != NULL) {
		m_IRC->Kill("Reconnecting.");

		SetIRCConnection(NULL);
	}

	if (m_ReconnectTimer) {
		m_ReconnectTimer->Destroy();
		m_ReconnectTimer = NULL;
	}

	const char* Server = GetServer();
	int Port = GetPort();

	if (!Server || !Port) {
		ScheduleReconnect(120);

		g_Bouncer->Log("%s has no default server. Can't (re)connect.", m_Name);

		return;
	}

	if (GetIPv6()) {
		g_Bouncer->Log("Trying to reconnect to [%s]:%d for %s", Server, Port, m_Name);
		Log("Trying to reconnect to [%s]:%d", Server, Port);
	} else {
		g_Bouncer->Log("Trying to reconnect to %s:%d for %s", Server, Port, m_Name);
		Log("Trying to reconnect to %s:%d", Server, Port);
	}

	time(&m_LastReconnect);

	const char* BindIp = BindIp = m_Config->ReadString("user.ip");

	if (!BindIp || BindIp[0] == '\0') {
		BindIp = g_Bouncer->GetConfig()->ReadString("system.vhost");

		if (BindIp && BindIp[0] == '\0')
			BindIp = NULL;
	}

	if (GetIdent() != NULL) {
		g_Bouncer->SetIdent(GetIdent());
	} else {
		g_Bouncer->SetIdent(m_Name);
	}

	CIRCConnection* Connection = new CIRCConnection(Server, Port, this, BindIp, GetSSL(), GetIPv6() ? AF_INET6 : AF_INET);

	if (Connection == NULL) {
		g_Bouncer->Log("Internal error: Could not create IRC connection object for %s", GetUsername());
	} else {
		SetIRCConnection(Connection);

		if (GetClientConnection() != NULL) {
			GetClientConnection()->SetPreviousNick(GetNick());
		}

		g_Bouncer->Log("Connection initialized for %s. Waiting for response...", GetUsername());
	}
}

bool CUser::ShouldReconnect(void) {
	int Interval = g_Bouncer->GetConfig()->ReadInteger("system.interval");

	if (Interval == 0) {
		Interval = 15;
	}

	if (!m_IRC && !m_Config->ReadInteger("user.quitted") && m_ReconnectTime < time(NULL) && time(NULL) - m_LastReconnect > 120 && time(NULL) - g_LastReconnect > Interval)
		return true;
	else
		return false;
}

void CUser::ScheduleReconnect(int Delay) {
	if (m_IRC)
		return;

	m_Config->WriteInteger("user.quitted", 0);

	int MaxDelay = Delay;

	int Interval = g_Bouncer->GetConfig()->ReadInteger("system.interval");

	if (Interval == 0)
		Interval = 15;

	if (time(NULL) - g_LastReconnect < Interval && MaxDelay < Interval)
		MaxDelay = Interval;

	if (time(NULL) - m_LastReconnect < 120 && MaxDelay < 120 && !IsAdmin())
		MaxDelay = 120;

	if (m_ReconnectTime < time(NULL) + MaxDelay) {
		if (m_ReconnectTimer)
			m_ReconnectTimer->Destroy();

		m_ReconnectTimer = g_Bouncer->CreateTimer(MaxDelay, false, UserReconnectTimer, this);

		time(&m_ReconnectTime);
		m_ReconnectTime += MaxDelay;
	}

	if (GetServer()) {
		char *Out;
		asprintf(&Out, "Scheduled reconnect in %d seconds.", m_ReconnectTime - time(NULL));

		CHECK_ALLOC_RESULT(Out, asprintf) {
			return;
		} CHECK_ALLOC_RESULT_END;

		Log(Out);
		free(Out);
	}
}

void CUser::Notice(const char *Text) {
	const char *Nick = GetNick();

	if (m_Client != NULL && Nick != NULL) {
		m_Client->WriteLine(":-sBNC!bouncer@shroudbnc.info PRIVMSG %s :%s", Nick, Text);
	}
}

void CUser::RealNotice(const char *Text) {
	const char *Nick = GetNick();

	if (m_Client != NULL && Nick != NULL) {
		m_Client->WriteLine(":-sBNC!bouncer@shroudbnc.info NOTICE %s :%s", Nick, Text);
	}
}

unsigned int CUser::GetIRCUptime(void) {
	if (m_IRC != NULL) {
		return time(NULL) - m_LastReconnect;
	} else {
		return 0;
	}
}

CLog *CUser::GetLog(void) {
	return m_Log;
}

void CUser::Log(const char *Format, ...) {
	char *Out;
	va_list marker;

	va_start(marker, Format);
	vasprintf(&Out, Format, marker);
	va_end(marker);

	CHECK_ALLOC_RESULT(Out, vasprintf) {
		return;
	} CHECK_ALLOC_RESULT_END;

	if (GetClientConnection() == NULL) {
		m_Log->WriteLine(FormatTime(time(NULL)), "%s", Out);
	}

	Notice(Out);

	free(Out);
}


void CUser::Lock(void) {
	m_Config->WriteInteger("user.lock", 1);
}

void CUser::Unlock(void) {
	m_Config->WriteInteger("user.lock", 0);
}

void CUser::SetIRCConnection(CIRCConnection* IRC) {
	CVector<CModule *> *Modules;
	bool WasNull = (m_IRC == NULL) ? true : false;

	m_IRC = IRC;

	Modules = g_Bouncer->GetModules();

	if (!IRC && !WasNull) {
		Log("You were disconnected from the IRC server.");

		g_Bouncer->Log("%s was disconnected from the server.", GetUsername());

		for (unsigned int i = 0; i < Modules->GetLength(); i++) {
			Modules->Get(i)->ServerDisconnect(GetUsername());
		}
	} else if (IRC) {
		for (unsigned int i = 0; i < Modules->GetLength(); i++) {
			Modules->Get(i)->ServerConnect(GetUsername());
		}

		if (m_ReconnectTimer) {
			m_ReconnectTimer->Destroy();
			m_ReconnectTimer = NULL;
		}

		time(&m_LastReconnect);

		IRC->SetTrafficStats(m_IRCStats);
	}
}

void CUser::SetClientConnection(CClientConnection* Client, bool DontSetAway) {
	if (!m_Client && !Client)
		return;

	if (!m_Client) {
		g_Bouncer->Log("User %s logged on (from %s).", GetUsername(), Client->GetPeerName());

		m_Config->WriteInteger("user.seen", time(NULL));
	}

	if (m_Client && Client) {
		g_Bouncer->Log("User %s re-attached to an already active session. Old session was closed.", GetUsername());

		m_Client->SetOwner(NULL);
		m_Client->Kill("Another client has connected.");

		SetClientConnection(NULL, true);
	}

	m_Client = Client;

	if (!Client) {
		if (!DontSetAway) {
			g_Bouncer->Log("User %s logged off.", GetUsername());

			m_Config->WriteInteger("user.seen", time(NULL));
		}

		CVector<CModule *> *Modules = g_Bouncer->GetModules();

		for (unsigned int i = 0; i < Modules->GetLength(); i++) {
			Modules->Get(i)->DetachClient(GetUsername());
		}

		if (m_IRC && !DontSetAway) {
			const char* DropModes = m_Config->ReadString("user.dropmodes");

			if (DropModes && *DropModes)
				m_IRC->WriteLine("MODE %s -%s", m_IRC->GetCurrentNick(), DropModes);

			const char* Offnick = m_Config->ReadString("user.awaynick");

			if (Offnick)
				m_IRC->WriteLine("NICK %s", Offnick);

			const char* Away = m_Config->ReadString("user.away");

			if (Away && *Away) {
				bool AppendTS = (m_Config->ReadInteger("user.ts") != 0);

				if (!AppendTS)
					m_IRC->WriteLine("AWAY :%s", Away);
				else {
					tm Now;
					time_t tNow;
					char strNow[100];

					time(&tNow);
					Now = *localtime(&tNow);

#ifdef _WIN32
					strftime(strNow, sizeof(strNow), "%#c" , &Now);
#else
					strftime(strNow, sizeof(strNow), "%c" , &Now);
#endif

					m_IRC->WriteLine("AWAY :%s (Away since %s)", Away, strNow);
				}
			}
		}
	} else {
		Client->SetTrafficStats(m_ClientStats);
	}
}

void CUser::SetAdmin(bool Admin) {
	m_Config->WriteInteger("user.admin", Admin ? 1 : 0);
	m_IsAdminCache = (int)Admin;
}

bool CUser::IsAdmin(void) {
	if (m_IsAdminCache == -1) {
		m_IsAdminCache = m_Config->ReadInteger("user.admin");
	}

	if (m_IsAdminCache != 0) {
		return true;
	} else {
		return false;
	}
}

void CUser::SetPassword(const char *Password) {
	if (g_Bouncer->GetConfig()->ReadInteger("system.md5") != 0) {
		Password = g_Bouncer->MD5(Password);
	}

	m_Config->WriteString("user.password", Password);
}

void CUser::SetServer(const char *Server) {
	USER_SETFUNCTION(server, Server);
}

const char *CUser::GetServer(void) {
	return m_Config->ReadString("user.server");
}

void CUser::SetPort(int Port) {
	m_Config->WriteInteger("user.port", Port);
}

int CUser::GetPort(void) {
	int Port = m_Config->ReadInteger("user.port");

	if (Port == 0) {
		return 6667;
	} else {
		return Port;
	}
}

bool CUser::IsLocked(void) {
	return m_Config->ReadInteger("user.lock") != 0;
}

void CUser::SetNick(const char *Nick) {
	USER_SETFUNCTION(nick, Nick);
}

void CUser::SetRealname(const char *Realname) {
	USER_SETFUNCTION(realname, Realname);
}

void CUser::MarkQuitted(bool RequireManualJump) {
	m_Config->WriteInteger("user.quitted", RequireManualJump ? 2 : 1);
}

void CUser::LogBadLogin(sockaddr *Peer) {
	badlogin_t BadLogin;

	for (unsigned int i = 0; i < m_BadLogins.GetLength(); i++) {
		if (CompareAddress(m_BadLogins[i].Address, Peer) == 0 && m_BadLogins[i].Count < 3) {
			m_BadLogins[i].Count++;

			return;
		}
	}

	BadLogin.Count = 1;
	BadLogin.Address = (sockaddr *)malloc(SOCKADDR_LEN(Peer->sa_family));
	memcpy(BadLogin.Address, Peer, SOCKADDR_LEN(Peer->sa_family));

	m_BadLogins.Insert(BadLogin);
}

bool CUser::IsIpBlocked(sockaddr *Peer) {
	for (unsigned int i = 0; i < m_BadLogins.GetLength(); i++) {
		if (CompareAddress(m_BadLogins[i].Address, Peer) == 0) {
			if (m_BadLogins[i].Count > 2) {
				return true;
			} else {
				return false;
			}
		}
	}

	return false;
}

void CUser::BadLoginPulse(void) {
	for (int i = m_BadLogins.GetLength() - 1; i >= 0; i--) {
		if (m_BadLogins[i].Count > 0) {
			m_BadLogins[i].Count--;

			if (m_BadLogins[i].Count <= 0) {
				free(m_BadLogins[i].Address);
				m_BadLogins.Remove(i);
			}
		}
	}
}

void CUser::AddHostAllow(const char *Mask, bool UpdateConfig) {
	char *dupMask;

	if (Mask == NULL || CanHostConnect(Mask)) {
		return;
	}

	dupMask = strdup(Mask);

	CHECK_ALLOC_RESULT(dupMask, strdup) {
		return;
	} CHECK_ALLOC_RESULT_END;

	if (m_HostAllows.Insert(dupMask) == false) {
		LOGERROR("Insert() failed. Host could not be added.");

		free(dupMask);

		return;
	}

	if (UpdateConfig) {
		UpdateHosts();
	}
}

void CUser::RemoveHostAllow(const char *Mask, bool UpdateConfig) {
	for (int i = m_HostAllows.GetLength() - 1; i >= 0; i--) {
		if (strcasecmp(m_HostAllows[i], Mask) == 0) {
			free(m_HostAllows[i]);
			m_HostAllows.Remove(i);

			if (UpdateConfig) {
				UpdateHosts();
			}

			return;
		}
	}
}

CVector<char *> *CUser::GetHostAllows(void) {
	return &m_HostAllows;
}

bool CUser::CanHostConnect(const char *Host) {
	unsigned int Count = 0;

	for (unsigned int i = 0; i < m_HostAllows.GetLength(); i++) {
		if (mmatch(m_HostAllows[i], Host) == 0) {
			return true;
		} else {
			Count++;
		}
	}

	if (Count > 0) {
		return false;
	} else {
		return true;	
	}
}

void CUser::UpdateHosts(void) {
	char *Out;
	int a = 0;

	for (unsigned int i = 0; i < m_HostAllows.GetLength(); i++) {
		asprintf(&Out, "user.hosts.host%d", a++);

		CHECK_ALLOC_RESULT(Out, asprintf) {
			g_Bouncer->Fatal();
		} CHECK_ALLOC_RESULT_END;

		m_Config->WriteString(Out, m_HostAllows[i]);
		free(Out);
	}

	asprintf(&Out, "user.hosts.host%d", a);

	CHECK_ALLOC_RESULT(Out, asprintf) {
		g_Bouncer->Fatal();
	} CHECK_ALLOC_RESULT_END;

	m_Config->WriteString(Out, NULL);
	free(Out);
}

CTrafficStats *CUser::GetClientStats(void) {
	return m_ClientStats;
}

CTrafficStats *CUser::GetIRCStats(void) {
	return m_IRCStats;
}

CKeyring *CUser::GetKeyring(void) {
	return m_Keys;
}

bool BadLoginTimer(time_t Now, void *User) {
	((CUser *)User)->BadLoginPulse();

	return true;
}

bool UserReconnectTimer(time_t Now, void *User) {
	int Interval;

	if (((CUser *)User)->GetIRCConnection() || !(g_Bouncer->GetStatus() == STATUS_RUN || g_Bouncer->GetStatus() == STATUS_PAUSE)) {
		return false;
	}

	((CUser *)User)->m_ReconnectTimer = NULL;

	Interval = g_Bouncer->GetConfig()->ReadInteger("system.interval");

	if (Interval == 0) {
		Interval = 15;
	}

	if (time(NULL) - g_LastReconnect > Interval) {
		((CUser *)User)->Reconnect();
	} else {
		((CUser*)User)->ScheduleReconnect(Interval);
	}

	return false;
}

time_t CUser::GetLastSeen(void) {
	return m_Config->ReadInteger("user.seen");
}

const char *CUser::GetAwayNick(void) {
	return m_Config->ReadString("user.awaynick");
}

void CUser::SetAwayNick(const char *Nick) {
	USER_SETFUNCTION(awaynick, Nick);

	if (m_Client == NULL && m_IRC != NULL) {
		m_IRC->WriteLine("NICK :%s", Nick);
	}
}

const char *CUser::GetAwayText(void) {
	return m_Config->ReadString("user.away");
}

void CUser::SetAwayText(const char *Reason) {
	USER_SETFUNCTION(away, Reason);

	if (m_Client == NULL && m_IRC != NULL) {
		m_IRC->WriteLine("AWAY :%s", Reason);
	}
}

const char *CUser::GetVHost(void) {
	return m_Config->ReadString("user.ip");
}

void CUser::SetVHost(const char *VHost) {
	USER_SETFUNCTION(vhost, VHost);
}

int CUser::GetDelayJoin(void) {
	return m_Config->ReadInteger("user.delayjoin");
}

void CUser::SetDelayJoin(int DelayJoin) {
	m_Config->WriteInteger("user.delayjoin", DelayJoin);
}

const char *CUser::GetConfigChannels(void) {
	return m_Config->ReadString("user.channels");
}

void CUser::SetConfigChannels(const char *Channels) {
	USER_SETFUNCTION(channels, Channels);
}

const char *CUser::GetSuspendReason(void) {
	return m_Config->ReadString("user.suspend");
}

void CUser::SetSuspendReason(const char *Reason) {
	USER_SETFUNCTION(suspend, Reason);
}

const char *CUser::GetServerPassword(void) {
	return m_Config->ReadString("user.spass");
}

void CUser::SetServerPassword(const char *Password) {
	USER_SETFUNCTION(spass, Password);
}

const char* CUser::GetAutoModes(void) {
	return m_Config->ReadString("user.automodes");
}

void CUser::SetAutoModes(const char *AutoModes) {
	USER_SETFUNCTION(automodes, AutoModes);
}

const char *CUser::GetDropModes(void) {
	return m_Config->ReadString("user.dropmodes");
}

void CUser::SetDropModes(const char *DropModes) {
	USER_SETFUNCTION(dropmodes, DropModes);
}

const CVector<X509 *> *CUser::GetClientCertificates(void) const {
#ifdef USESSL
	return &m_ClientCertificates;
#else
	return NULL;
#endif
}

bool CUser::AddClientCertificate(const X509 *Certificate) {
#ifdef USESSL
	X509 *DuplicateCertificate;

	for (unsigned int i = 0; i < m_ClientCertificates.GetLength(); i++) {
		if (X509_cmp(m_ClientCertificates[i], Certificate) == 0) {
			return true;
		}
	}

	DuplicateCertificate = /*X509_dup(*/const_cast<X509 *>(Certificate)/*)*/;

	m_ClientCertificates.Insert(DuplicateCertificate);

	return PersistCertificates();
#else
	return false;
#endif
}

bool CUser::RemoveClientCertificate(const X509 *Certificate) {
#ifdef USESSL
	for (unsigned int i = 0; i < m_ClientCertificates.GetLength(); i++) {
		if (X509_cmp(m_ClientCertificates[i], Certificate) == 0) {
			X509_free(m_ClientCertificates[i]);

			m_ClientCertificates.Remove(i);

			return PersistCertificates();
		}
	}
#endif

	return false;
}

#ifdef USESSL
bool CUser::PersistCertificates(void) {
	char *TempFilename;
	const char *Filename;
	FILE *CertFile;

	asprintf(&TempFilename, "users/%s.pem", m_Name);
	Filename = g_Bouncer->BuildPath(TempFilename);
	free(TempFilename);

	CHECK_ALLOC_RESULT(Filename, asprintf) {
		return false;
	} CHECK_ALLOC_RESULT_END;

	if (m_ClientCertificates.GetLength() == 0) {
		unlink(Filename);
	} else {
		CertFile = fopen(Filename, "w");

		SetPermissions(Filename, S_IRUSR | S_IWUSR);

		CHECK_ALLOC_RESULT(CertFile, fopen) {
			return false;
		} CHECK_ALLOC_RESULT_END;

		for (unsigned int i = 0; i < m_ClientCertificates.GetLength(); i++) {
			PEM_write_X509(CertFile, m_ClientCertificates[i]);
			fprintf(CertFile, "\n");
		}

		fclose(CertFile);
	}

	return true;
}
#endif

bool CUser::FindClientCertificate(const X509 *Certificate) const {
#ifdef USESSL
	for (unsigned int i = 0; i < m_ClientCertificates.GetLength(); i++) {
		if (X509_cmp(m_ClientCertificates[i], Certificate) == 0) {
			return true;
		}
	}
#endif

	return false;
}

bool CUser::GetSSL(void) {
#ifdef USESSL
	if (m_Config->ReadInteger("user.ssl") != 0) {
		return true;
	} else {
		return false;
	}
#else
	return false;
#endif
}

void CUser::SetSSL(bool SSL) {
#ifdef USESSL
	if (SSL) {
		m_Config->WriteInteger("user.ssl", 1);
	} else {
		m_Config->WriteInteger("user.ssl", 0);
	}
#endif
}

const char *CUser::GetIdent(void) {
	return m_Config->ReadString("user.ident");
}

void CUser::SetIdent(const char *Ident) {
	USER_SETFUNCTION(ident, Ident);
}

const char *CUser::GetTagString(const char *Tag) {
	char *Setting;
	const char *Value;

	if (Tag == NULL) {
		return NULL;
	}

	asprintf(&Setting, "tag.%s", Tag);

	CHECK_ALLOC_RESULT(Setting, asprintf) {
		return NULL;
	} CHECK_ALLOC_RESULT_END;

	Value = m_Config->ReadString(Setting);

	free(Setting);

	return Value;
}

int CUser::GetTagInteger(const char *Tag) {
	const char *Value = GetTagString(Tag);

	if (Value != NULL) {
		return atoi(Value);
	} else {
		return 0;
	}
}

bool CUser::SetTagString(const char *Tag, const char *Value) {
	bool ReturnValue;
	char *Setting;
	CVector<CModule *> *Modules;

	if (Tag == NULL) {
		return false;
	}

	asprintf(&Setting, "tag.%s", Tag);

	CHECK_ALLOC_RESULT(Setting, asprintf) {
		return false;
	} CHECK_ALLOC_RESULT_END;

	ReturnValue = m_Config->WriteString(Setting, Value);

	Modules = g_Bouncer->GetModules();

	for (unsigned int i = 0; i < Modules->GetLength(); i++) {
		Modules->Get(i)->UserTagModified(Tag, Value);
	}

	return ReturnValue;
}

bool CUser::SetTagInteger(const char *Tag, int Value) {
	bool ReturnValue;
	char *StringValue;

	asprintf(&StringValue, "%d", Value);

	CHECK_ALLOC_RESULT(StringValue, asprintf) {
		return false;
	} CHECK_ALLOC_RESULT_END;

	ReturnValue = SetTagString(Tag, StringValue);

	free(StringValue);

	return ReturnValue;
}

const char *CUser::GetTagName(int Index) {
	int Skip = 0;
	int Count = m_Config->GetLength();

	for (int i = 0; i < Count; i++) {
		hash_t<char *> *Item = m_Config->Iterate(i);

		if (strstr(Item->Name, "tag.") == Item->Name) {
			if (Skip == Index) {
				return Item->Name + 4;
			}

			Skip++;
		}
	}

	return NULL;
}

bool CUser::GetIPv6(void) {
	if (m_Config->ReadInteger("user.ipv6") != 0) {
		return true;
	} else {
		return false;
	}
}

void CUser::SetIPv6(bool IPv6) {
	m_Config->WriteInteger("user.ipv6", IPv6 ? 1 : 0);
}

const char *CUser::FormatTime(time_t Timestamp) {
	tm *Time;
	static char *Buffer = NULL;
	char *NewLine;

	free(Buffer);

	Timestamp += GetGmtOffset() * 60;
	Time = gmtime(&Timestamp);
	Buffer = strdup(asctime(Time));

	while ((NewLine = strchr(Buffer, '\n')) != NULL) {
		*NewLine = '\0';
	}

	return Buffer;
}

void CUser::SetGmtOffset(unsigned int Offset) {
	m_Config->WriteInteger("user.tz", Offset);
}

unsigned int CUser::GetGmtOffset(void) {
	const char *Offset;
	time_t Now;
	tm GMTime;
	tm *CurrentTime;

	time(&Now);

	Offset = m_Config->ReadString("user.tz");

	if (Offset == NULL) {
		CurrentTime = gmtime(&Now);
		memcpy(&GMTime, CurrentTime, sizeof(GMTime));

		CurrentTime = localtime(&Now);

		return (CurrentTime->tm_hour - GMTime.tm_hour) * 60 + (CurrentTime->tm_min - GMTime.tm_min);
	} else {
		return atoi(Offset);
	}
}
