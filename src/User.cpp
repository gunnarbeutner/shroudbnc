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

	if (m_Name == NULL) {
		LOGERROR("strdup() failed. Could not create user.");

		g_Bouncer->Fatal();
	}

	assert(m_Name != NULL);

	char* Out;
	asprintf(&Out, "users/%s.conf", Name);

	if (Out == NULL) {
		LOGERROR("asprintf() failed.");

		g_Bouncer->Fatal();
	}

	m_Config = new CConfig(Out);

	free(Out);

	if (m_Config == NULL) {
		LOGERROR("new operator failed. Could not create user.");

		g_Bouncer->Fatal();
	}

	m_IRC = NULL;

	m_ReconnectTime = 0;
	m_LastReconnect = 0;

	asprintf(&Out, "users/%s.log", Name);

	if (Out == NULL) {
		LOGERROR("asprintf() failed.");

		g_Bouncer->Fatal();
	}

	m_Log = new CLog(Out);

	free(Out);

	if (m_Log == NULL) {
		LOGERROR("new operator failed. Could not create user log.");

		g_Bouncer->Fatal();
	}

	m_BadLogins = NULL;
	m_BadLoginCount = 0;

	m_HostAllows = NULL;
	m_HostAllowCount = 0;

	unsigned int i = 0;
	while (true) {
		asprintf(&Out, "user.hosts.host%d", i++);

		if (Out == NULL) {
			LOGERROR("asprintf() failed.");

			g_Bouncer->Fatal();
		}

		const char* Hostmask = m_Config->ReadString(Out);

		free(Out);

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

	m_IsAdminCache = -1;

	m_ClientCertificates = NULL;
	m_ClientCertificateCount = 0;

#ifdef USESSL
	asprintf(&Out, "users/%s.pem", Name);

	X509* Cert;
	FILE* ClientCert = fopen(Out, "r");

	if (ClientCert != NULL) {
		while ((Cert = PEM_read_X509(ClientCert, NULL, NULL, NULL)) != NULL) {
			m_ClientCertificates = (X509**)realloc(m_ClientCertificates, sizeof(X509*) * ++m_ClientCertificateCount);

			m_ClientCertificates[m_ClientCertificateCount - 1] = Cert;
		}

		fclose(ClientCert);
	} else
		m_ClientCertificates = NULL;

	free(Out);
#endif

	if (!m_Config->ReadInteger("user.quitted"))
		ScheduleReconnect(0);
}

void CUser::LoadEvent(void) {
	CVector<CModule *> *Modules = g_Bouncer->GetModules();

	for (int i = 0; i < Modules->Count(); i++) {
		Modules->Get(i)->UserLoad(m_Name);
	}
}

CUser::~CUser() {
	if (m_Client)
		m_Client->Kill("Removing user.");

	if (m_IRC)
		m_IRC->Kill("-)(- If you can't see the fnords, they can't eat you.");

	delete m_Config;
	delete m_Log;

	delete m_ClientStats;
	delete m_IRCStats;

	delete m_Keys;

	free(m_BadLogins);

	free(m_Name);

	m_BadLoginPulse->Destroy();

#ifdef USESSL
	for (int i = 0; i < m_ClientCertificateCount; i++)
		X509_free(m_ClientCertificates[i]);

	free(m_ClientCertificates);
#endif

	if (m_ReconnectTimer)
		delete m_ReconnectTimer;
}

SOCKET CUser::GetIRCSocket(void) {
	if (m_IRC)
		return m_IRC->GetSocket();
	else
		return INVALID_SOCKET;
}

CClientConnection* CUser::GetClientConnection(void) {
	return m_Client;
}

CIRCConnection* CUser::GetIRCConnection(void) {
	return m_IRC;
}

bool CUser::IsConnectedToIRC(void) {
	return m_IRC != NULL;
}

void CUser::Attach(CClientConnection* Client) {
	char* Out;

	if (IsLocked()) {
		const char* Reason = GetSuspendReason();

		if (!Reason)
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

	Client->m_Owner = this;

	SetClientConnection(Client, true);

	CLog *Motd = new CLog("sbnc.motd");

	if (m_IRC) {
		m_IRC->InternalWriteLine("AWAY");

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
		if (!Motd->IsEmpty())
			Motd->PlayToUser(this, Log_Motd);

		ScheduleReconnect(0);
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

	for (int i = 0; i < Modules->Count(); i++) {
		Modules->Get(i)->AttachClient(GetUsername());
	}

	if (m_IRC == NULL && GetServer() == NULL)
		Notice("You haven't set a server yet. Use /sbnc set server <Hostname> <Port> to do that now.");

	if (!GetLog()->IsEmpty())
		Notice("You have new messages. Use '/msg -sBNC read' to view them.");
}

bool CUser::Validate(const char* Password) {
	const char* RealPass = m_Config->ReadString("user.password");

	if (!RealPass || strlen(RealPass) == 0)
		return false;

	if (g_Bouncer->GetConfig()->ReadInteger("system.md5"))
		Password = g_Bouncer->MD5(Password);

	return (strcmp(RealPass, Password) == 0);
}

const char* CUser::GetNick(void) {
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

const char* CUser::GetUsername(void) {
	return m_Name;
}

const char* CUser::GetRealname(void) {
	const char* Out = m_Config->ReadString("user.realname");

	if (!Out)
		return "shroudBNC User";
	else
		return Out;
}

CConfig* CUser::GetConfig(void) {
	return m_Config;
}

void CUser::Simulate(const char* Command, CClientConnection* FakeClient) {
	if (!Command)
		return;

	char* C = strdup(Command);

	if (C == NULL) {
		LOGERROR("strdup() failed. Could not simulate command (%s, %s).", m_Name, Command);

		return;
	}

	if (m_Client)
		m_Client->ParseLine(C);
	else {
		if (FakeClient == NULL) {
			sockaddr_in null_peer;
			memset(&null_peer, 0, sizeof(null_peer));
			FakeClient = new CClientConnection(INVALID_SOCKET, null_peer);

			if (FakeClient == NULL) {
				LOGERROR("new operator failed. Could not simulate command (%s, %s).", m_Name, Command);
			} else {
				FakeClient->m_Owner = this;

				FakeClient->ParseLine(C);

				FakeClient->m_Owner = NULL;
				delete FakeClient;
			}
		} else {
			CUser* Owner = FakeClient->m_Owner;
			CClientConnection* Client = m_Client;

			m_Client = FakeClient;
			FakeClient->m_Owner = this;
			FakeClient->ParseLine(C);
			FakeClient->m_Owner = Owner;
			m_Client = Client;
		}
	}

	free(C);
}

void CUser::Reconnect(void) {
	if (m_IRC) {
		m_IRC->Kill("Reconnecting");

		SetIRCConnection(NULL);
	}

	char* Out;

	if (m_ReconnectTimer) {
		m_ReconnectTimer->Destroy();
		m_ReconnectTimer = NULL;
	}

	const char* Server = GetServer();
	int Port = GetPort();

	if (!Server || !Port) {
		ScheduleReconnect(120);

		g_Bouncer->GetLog()->WriteLine("%s has no default server. Can't (re)connect.", m_Name);

		return;
	}

	asprintf(&Out, "Trying to reconnect to %s:%d", Server, Port);

	if (Out == NULL) {
		LOGERROR("asprintf() failed.");
	} else {
		g_Bouncer->Log("%s", Out);
		Notice(Out);
		free(Out);
	}

	asprintf(&Out, "Trying to reconnect to %s:%d for %s", Server, Port, m_Name);
	if (Out == NULL) {
		LOGERROR("asprintf() failed.");
	} else {
		g_Bouncer->GlobalNotice(Out, true);
		free(Out);
	}

	m_LastReconnect = time(NULL);

	if (GetIdent() != NULL)
		g_Bouncer->SetIdent(GetIdent());
	else
		g_Bouncer->SetIdent(m_Name);

	const char* BindIp = BindIp = m_Config->ReadString("user.ip");

	if (!BindIp || BindIp[0] == '\0') {
		BindIp = g_Bouncer->GetConfig()->ReadString("system.vhost");

		if (BindIp && BindIp[0] == '\0')
			BindIp = NULL;
	}

	CIRCConnection* Connection = CreateIRCConnection(Server, Port, this, BindIp, GetSSL());

	if (!Connection) {
		Notice("Can't connect..");
		g_Bouncer->Log("Can't connect..");
		g_Bouncer->GlobalNotice("Failed to connect.", true);
	} else {
		SetIRCConnection(Connection);

		g_Bouncer->Log("Connection initialized. Waiting for response...");
		g_Bouncer->GlobalNotice("Connection initialized. Waiting for response...", true);
	}
}

bool CUser::ShouldReconnect(void) {
	int Interval = g_Bouncer->GetConfig()->ReadInteger("system.interval");

	if (Interval == 0)
		Interval = 15;

	if (!m_IRC && !m_Config->ReadInteger("user.quitted") && m_ReconnectTime < time(NULL) && time(NULL) - m_LastReconnect > 120 && time(NULL) - g_LastReconnect > Interval)
		return true;
	else
		return false;
}

void CUser::ScheduleReconnect(int Delay) {
	if (m_IRC)
		return;

	if (m_Config->ReadInteger("user.quitted") && Delay == 0)
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

		m_ReconnectTime = time(NULL) + MaxDelay;
	}

	if (GetServer()) {
		char* Out;
		asprintf(&Out, "Scheduled reconnect in %d seconds.", m_ReconnectTime - time(NULL));

		if (Out == NULL) {
			LOGERROR("asprintf() failed.");
		} else {
			Notice(Out);
			free(Out);
		}
	}
}

void CUser::Notice(const char* Text) {
	const char* Nick = GetNick();

	if (m_Client && Nick)
		m_Client->WriteLine(":-sBNC!bouncer@shroudbnc.org PRIVMSG %s :%s", Nick, Text);
}

void CUser::RealNotice(const char* Text) {
	const char* Nick = GetNick();

	if (m_Client && Nick)
		m_Client->WriteLine(":-sBNC!bouncer@shroudbnc.org NOTICE %s :%s", Nick, Text);
}

int CUser::IRCUptime(void) {
	return m_IRC ? (time(NULL) - m_LastReconnect) : 0;
}

CLog* CUser::GetLog(void) {
	return m_Log;
}

void CUser::Log(const char* Format, ...) {
	char* Out;
	va_list marker;

	va_start(marker, Format);
	vasprintf(&Out, Format, marker);
	va_end(marker);

	if (Out == NULL) {
		LOGERROR("vasprintf() failed.");
	} else {
		m_Log->WriteLine("%s", Out);
		free(Out);
	}
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
		Notice("Disconnected from the server.");

		char* Out;

		asprintf(&Out, "%s disconnected from the server.", GetUsername());

		if (Out == NULL) {
			LOGERROR("asprintf() failed.");
		} else {
			g_Bouncer->Log("%s", Out);
			g_Bouncer->GlobalNotice(Out, true);

			free(Out);
		}

		if (m_Client == NULL)
			Log("Disconnected from the server.");

		for (int i = 0; i < Modules->Count(); i++) {
			Modules->Get(i)->ServerDisconnect(GetUsername());
		}
	} else if (IRC) {
		for (int i = 0; i < Modules->Count(); i++) {
			Modules->Get(i)->ServerConnect(GetUsername());
		}

		if (m_ReconnectTimer) {
			m_ReconnectTimer->Destroy();
			m_ReconnectTimer = NULL;
		}

		m_LastReconnect = time(NULL);

		IRC->AttachStats(m_IRCStats);
	}
}

void CUser::SetClientConnection(CClientConnection* Client, bool DontSetAway) {
	if (!m_Client && !Client)
		return;

	char* Out;

	if (!m_Client) {
		asprintf(&Out, "User %s logged on (from %s).", GetUsername(), Client->GetPeerName());

		if (Out == NULL) {
			LOGERROR("asprintf() failed.");
		} else {	
			g_Bouncer->Log("%s", Out);
			g_Bouncer->GlobalNotice(Out, true);

			free(Out);
		}

		m_Config->WriteInteger("user.seen", time(NULL));
	}


	if (m_Client && Client) {
		asprintf(&Out, "Seamless transition for %s", GetUsername());

		if (Out == NULL) {
			LOGERROR("asprintf() failed.");
		} else {
			g_Bouncer->GlobalNotice(Out, true);
			g_Bouncer->Log("%s", Out);

			free(Out);
		}

		m_Client->SetOwner(NULL);
		m_Client->Kill("Another client has connected.");

		SetClientConnection(NULL, true);
	}

	m_Client = Client;

	if (!Client) {
		if (!DontSetAway) {
			asprintf(&Out, "User %s logged off.", GetUsername());

			if (Out == NULL) {
				LOGERROR("asprintf() failed.");
			} else {
				g_Bouncer->GlobalNotice(Out, true);
				g_Bouncer->Log("%s", Out);

				free(Out);
			}

			m_Config->WriteInteger("user.seen", time(NULL));
		}

		CVector<CModule *> *Modules = g_Bouncer->GetModules();

		for (int i = 0; i < Modules->Count(); i++) {
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
	} else
		Client->AttachStats(m_ClientStats);
}

void CUser::SetAdmin(bool Admin) {
	m_Config->WriteString("user.admin", Admin ? "1" : "0");
	m_IsAdminCache = (int)Admin;
}

bool CUser::IsAdmin(void) {
	if (m_IsAdminCache == -1)
		m_IsAdminCache = m_Config->ReadInteger("user.admin");

	return m_IsAdminCache != 0;
}

void CUser::SetPassword(const char* Password) {
	if (g_Bouncer->GetConfig()->ReadInteger("system.md5"))
		Password = g_Bouncer->MD5(Password);

	m_Config->WriteString("user.password", Password);
}

void CUser::SetServer(const char* Server) {
	m_Config->WriteString("user.server", Server);
}

const char* CUser::GetServer(void) {
	return m_Config->ReadString("user.server");
}

void CUser::SetPort(int Port) {
	m_Config->WriteInteger("user.port", Port);
}

int CUser::GetPort(void) {
	int Port = m_Config->ReadInteger("user.port");

	if (Port == 0)
		return 6667;
	else
		return Port;
}

bool CUser::IsLocked(void) {
	return m_Config->ReadInteger("user.lock") != 0;
}

void CUser::SetNick(const char* Nick) {
	m_Config->WriteString("user.nick", Nick);
}

void CUser::SetRealname(const char* Realname) {
	if (Realname)
		m_Config->WriteString("user.realname", Realname);
}

void CUser::MarkQuitted(void) {
	m_Config->WriteInteger("user.quitted", 1);
}

void CUser::LogBadLogin(sockaddr_in Peer) {
	badlogin_t *BadLogins;

	for (unsigned int i = 0; i < m_BadLoginCount; i++) {
#ifdef _WIN32
		if (m_BadLogins[i].Address.sin_addr.S_un.S_addr == Peer.sin_addr.S_un.S_addr && m_BadLogins[i].Count < 3) {
#else
		if (m_BadLogins[i].Address.sin_addr.s_addr == Peer.sin_addr.s_addr && m_BadLogins[i].Count < 3) {
#endif
			m_BadLogins[i].Count++;

			return;
		}
	}

	for (unsigned int a = 0; a < m_BadLoginCount; a++) {
		if (m_BadLogins[a].Count == 0) {
			m_BadLogins[a].Address = Peer;
			m_BadLogins[a].Count = 1;

			return;
		}
	}

	BadLogins = (badlogin_t *)realloc(m_BadLogins, sizeof(badlogin_t) * ++m_BadLoginCount);

	if (!BadLogins) {
		LOGERROR("realloc() failed. Could not add new item.");

		--m_BadLoginCount;

		return;
	}

	m_BadLogins = BadLogins;
	m_BadLogins[m_BadLoginCount - 1].Address = Peer;
	m_BadLogins[m_BadLoginCount - 1].Count = 1;
}

bool CUser::IsIpBlocked(sockaddr_in Peer) {
	for (unsigned int i = 0; i < m_BadLoginCount; i++) {
#ifdef _WIN32
		if (m_BadLogins[i].Address.sin_addr.S_un.S_addr == Peer.sin_addr.S_un.S_addr) {
#else
		if (m_BadLogins[i].Address.sin_addr.s_addr == Peer.sin_addr.s_addr) {
#endif
			if (m_BadLogins[i].Count > 2)
				return true;
			else
				return false;
		}
	}

	return false;
}

void CUser::BadLoginPulse(void) {
	for (unsigned int i = 0; i < m_BadLoginCount; i++) {
		if (m_BadLogins[i].Count > 0) {
			m_BadLogins[i].Count--;
		}
	}
}

void CUser::AddHostAllow(const char* Mask, bool UpdateConfig) {
	char** HostAllows;

	for (unsigned int i = 0; i < m_HostAllowCount; i++) {
		if (!m_HostAllows[i]) {
			m_HostAllows[i] = strdup(Mask);

			if (UpdateConfig)
				UpdateHosts();

			return;
		}
	}

	HostAllows = (char**)realloc(m_HostAllows, sizeof(char*) * ++m_HostAllowCount);

	if (HostAllows == NULL) {
		LOGERROR("realloc() failed. Host could not be added.");

		return;
	}

	m_HostAllows = HostAllows;
	m_HostAllows[m_HostAllowCount - 1] = strdup(Mask);

	if (UpdateConfig)
		UpdateHosts();
}

void CUser::RemoveHostAllow(const char* Mask, bool UpdateConfig) {
	for (unsigned int i = 0; i < m_HostAllowCount; i++) {
		if (m_HostAllows[i] && strcasecmp(m_HostAllows[i], Mask) == 0) {
			free(m_HostAllows[i]);
			m_HostAllows[i] = NULL;

			if (UpdateConfig)
				UpdateHosts();

			return;
		}
	}
}

char** CUser::GetHostAllows(void) {
	return m_HostAllows;
}

unsigned int CUser::GetHostAllowCount(void) {
	return m_HostAllowCount;
}

bool CUser::CanHostConnect(const char* Host) {
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

void CUser::UpdateHosts(void) {
	char* Out;
	int a = 0;

	for (unsigned int i = 0; i < m_HostAllowCount; i++) {
		if (m_HostAllows[i]) {
			asprintf(&Out, "user.hosts.host%d", a++);

			if (Out == NULL) {
				LOGERROR("asprintf() failed. Could not update hosts");

				g_Bouncer->Fatal();
			} else {
				m_Config->WriteString(Out, m_HostAllows[i]);
				free(Out);
			}
		}
	}

	asprintf(&Out, "user.hosts.host%d", a);

	if (Out == NULL) {
		LOGERROR("asprintf() failed. Could not update hosts.");

		g_Bouncer->Fatal();
	}

	m_Config->WriteString(Out, NULL);
	free(Out);
}

CTrafficStats* CUser::GetClientStats(void) {
	return m_ClientStats;
}

CTrafficStats* CUser::GetIRCStats(void) {
	return m_IRCStats;
}

CKeyring* CUser::GetKeyring(void) {
	return m_Keys;
}

bool BadLoginTimer(time_t Now, void* User) {
	((CUser*)User)->BadLoginPulse();

	return true;
}

bool UserReconnectTimer(time_t Now, void* User) {
	if (((CUser*)User)->GetIRCConnection())
		return false;

	((CUser*)User)->m_ReconnectTimer = NULL;

	int Interval = g_Bouncer->GetConfig()->ReadInteger("system.interval");

	if (Interval == 0)
		Interval = 15;

	if (time(NULL) - g_LastReconnect > Interval)
		((CUser*)User)->Reconnect();
	else
		((CUser*)User)->ScheduleReconnect(Interval);

	return false;
}

time_t CUser::GetLastSeen(void) {
	return m_Config->ReadInteger("user.seen");
}

const char* CUser::GetAwayNick(void) {
	return m_Config->ReadString("user.awaynick");
}

void CUser::SetAwayNick(const char* Nick) {
	m_Config->WriteString("user.awaynick", Nick);

	if (m_Client == NULL && m_IRC != NULL)
		m_IRC->WriteLine("Nick :%s", Nick);
}

const char* CUser::GetAwayText(void) {
	return m_Config->ReadString("user.away");
}

void CUser::SetAwayText(const char* Reason) {
	m_Config->WriteString("user.away", Reason);

	if (m_Client == NULL && m_IRC != NULL)
		m_IRC->WriteLine("AWAY :%s", Reason);
}

const char* CUser::GetVHost(void) {
	return m_Config->ReadString("user.ip");
}

void CUser::SetVHost(const char* VHost) {
	m_Config->WriteString("user.ip", VHost);
}

int CUser::GetDelayJoin(void) {
	return m_Config->ReadInteger("user.delayjoin");
}

void CUser::SetDelayJoin(int DelayJoin) {
	m_Config->WriteInteger("user.delayjoin", DelayJoin);
}

const char* CUser::GetConfigChannels(void) {
	return m_Config->ReadString("user.channels");
}

void CUser::SetConfigChannels(const char* Channels) {
	m_Config->WriteString("user.channels", Channels);
}

const char* CUser::GetSuspendReason(void) {
	return m_Config->ReadString("user.suspend");
}

void CUser::SetSuspendReason(const char* Reason) {
	m_Config->WriteString("user.suspend", Reason);
}

const char* CUser::GetServerPassword(void) {
	return m_Config->ReadString("user.spass");
}

void CUser::SetServerPassword(const char* Password) {
	m_Config->WriteString("user.spass", Password);
}

const char* CUser::GetAutoModes(void) {
	return m_Config->ReadString("user.automodes");
}

void CUser::SetAutoModes(const char* AutoModes) {
	m_Config->WriteString("user.automodes", AutoModes);
}

const char* CUser::GetDropModes(void) {
	return m_Config->ReadString("user.dropmodes");
}

void CUser::SetDropModes(const char* DropModes) {
	m_Config->WriteString("user.dropmodes", DropModes);
}

X509* CUser::GetClientCertificate(int Index) {
#ifdef USESSL
	if (Index >= m_ClientCertificateCount || Index < 0)
		return NULL;
	else
		return m_ClientCertificates[Index];
#else
	return NULL;
#endif
}

bool CUser::AddClientCertificate(X509* Certificate) {
#ifdef USESSL
	for (int i = 0; i < m_ClientCertificateCount; i++) {
		if (X509_cmp(m_ClientCertificates[i], Certificate) == 0)
			return true;
	}

	m_ClientCertificates = (X509**)realloc(m_ClientCertificates, sizeof(X509*) * ++m_ClientCertificateCount);
	m_ClientCertificates[m_ClientCertificateCount - 1] = Certificate;

	return PersistCertificates();
#else
	return false;
#endif
}

bool CUser::RemoveClientCertificate(X509* Certificate) {
#ifdef USESSL
	for (int i = 0; i < m_ClientCertificateCount; i++) {
		if (X509_cmp(m_ClientCertificates[i], Certificate) == 0) {
			m_ClientCertificates[i] = m_ClientCertificates[m_ClientCertificateCount - 1];

			m_ClientCertificates = (X509**)realloc(m_ClientCertificates, sizeof(X509*) * --m_ClientCertificateCount);

			return PersistCertificates();
		}
	}
#endif

	return false;
}

#ifdef USESSL
bool CUser::PersistCertificates(void) {
	char *Out;
	FILE *CertFile;

	asprintf(&Out, "users/%s.pem", m_Name);

	if (Out == NULL) {
		LOGERROR("asprintf() failed.");

		return false;
	}

	if (m_ClientCertificateCount == 0)
		unlink(Out);
	else {
		CertFile = fopen(Out, "w");

		if (CertFile == NULL) {
			LOGERROR("Unable to open certificate file for writing.");

			return false;
		}

		for (int i = 0; i < m_ClientCertificateCount; i++) {
			X509* a = m_ClientCertificates[i];
			PEM_write_X509(CertFile, m_ClientCertificates[i]);
			fprintf(CertFile, "\n");
		}

		fclose(CertFile);
	}

	free(Out);

	return true;
}
#endif

bool CUser::FindClientCertificate(X509* Certificate) {
#ifdef USESSL
	for (int i = 0; i < m_ClientCertificateCount; i++) {
		if (X509_cmp(m_ClientCertificates[i], Certificate) == 0)
			return true;
	}
#endif

	return false;
}

void CUser::SetSSL(bool SSL) {
	m_Config->WriteInteger("user.ssl", SSL ? 1 : 0);
}

bool CUser::GetSSL(void) {
	return (m_Config->ReadInteger("user.ssl") != 0);
}

void CUser::SetIdent(const char *Ident) {
	m_Config->WriteString("user.ident", Ident);
}

const char *CUser::GetIdent(void) {
	return m_Config->ReadString("user.ident");
}
