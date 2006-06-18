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

extern time_t g_LastReconnect;

CTimer *g_ReconnectTimer = NULL;

/**
 * CUser
 *
 * Constructs a new user object.
 *
 * @param Name the name of the user
 */
CUser::CUser(const char *Name) {
	char *Out;
	X509 *Cert;
	FILE *ClientCert;

	m_ManagedMemory = 0;
	m_MemoryManager = NULL;

	m_Client = NULL;
	m_IRC = NULL;
	m_Name = ustrdup(Name);

	CHECK_ALLOC_RESULT(m_Name, strdup) {
		g_Bouncer->Fatal();
	} CHECK_ALLOC_RESULT_END;

	asprintf(&Out, "users/%s.conf", Name);

	CHECK_ALLOC_RESULT(Out, asprintf) {
		g_Bouncer->Fatal();
	} CHECK_ALLOC_RESULT_END;

	m_Config = new CConfig(g_Bouncer->BuildPath(Out), this);

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

	m_ClientStats = new CTrafficStats();
	m_IRCStats = new CTrafficStats();

	m_Keys = new CKeyring(m_Config, this);

	m_BadLoginPulse = new CTimer(200, true, BadLoginTimer, this);

	m_IsAdminCache = -1;
	m_LeanModeCache = -1;

#ifdef USESSL
	asprintf(&Out, "users/%s.pem", Name);

	CHECK_ALLOC_RESULT(Out, asprintf) {
		g_Bouncer->Fatal();
	} CHECK_ALLOC_RESULT_END;

	ClientCert = fopen(g_Bouncer->BuildPath(Out), "r");

	free(Out);

	if (ClientCert != NULL) {
		while ((Cert = PEM_read_X509(ClientCert, NULL, NULL, NULL)) != NULL) {
			m_ClientCertificates.Insert(Cert);
		}

		fclose(ClientCert);
	}

#endif

	if (m_Config->ReadInteger("user.quitted") != 2) {
		ScheduleReconnect();
	}

	if (IsAdmin()) {
		g_Bouncer->GetAdminUsers()->Insert(this);
	}
}

/**
 * ~CUser
 *
 * Destructs a user object.
 */
CUser::~CUser(void) {
	if (m_MemoryManager != NULL) {
		m_MemoryManager->RealManager = NULL;
	}

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

	ufree(m_Name);

	if (m_BadLoginPulse != NULL) {
		m_BadLoginPulse->Destroy();
	}

#ifdef USESSL
	for (unsigned int i = 0; i < m_ClientCertificates.GetLength(); i++) {
		X509_free(m_ClientCertificates[i]);
	}
#endif

	g_Bouncer->GetAdminUsers()->Remove(this);
}

/**
 * LoadEvent
 *
 * Used for notifying modules that a new user has been loaded.
 */
void CUser::LoadEvent(void) {
	const CVector<CModule *> *Modules;

	Modules = g_Bouncer->GetModules();

	for (unsigned int i = 0; i < Modules->GetLength(); i++) {
		(*Modules)[i]->UserLoad(m_Name);
	}
}

/**
 * GetClientConnection
 *
 * Returns the client connection for the user, or NULL
 * if nobody is currently logged in as the user.
 */
CClientConnection *CUser::GetClientConnection(void) {
	return m_Client;
}

/**
 * GetIRCConnection
 *
 * Returns the IRC connection for this user, or NULL
 * if the user is not connected to an IRC server.
 */
CIRCConnection *CUser::GetIRCConnection(void) {
	return m_IRC;
}

/**
 * Attach
 *
 * Attaches a client connection to the user.
 *
 * @param Client the client for this user
 */
void CUser::Attach(CClientConnection *Client) {
	char *Out;
	const char *Reason, *AutoModes, *IrcNick, *MotdText;
	CLog *Motd;
	unsigned int i;
	char **Keys;
	const CVector<CModule *> *Modules;

	if (IsLocked()) {
		Reason = GetSuspendReason();

		if (Reason == NULL) {
			Client->Kill("*** You cannot attach to this user.");
		} else {
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

	Motd = new CLog("sbnc.motd");

	if (m_IRC != NULL) {
		m_IRC->WriteLine("AWAY");

		AutoModes = m_Config->ReadString("user.automodes");

		if (AutoModes != NULL && m_IRC->GetCurrentNick() != NULL) {
			m_IRC->WriteLine("MODE %s +%s", m_IRC->GetCurrentNick(), AutoModes);
		}

		IrcNick = m_IRC->GetCurrentNick();

		if (IrcNick != NULL) {
			Client->WriteLine(":%s!%s@%s NICK :%s", Client->GetNick(), GetUsername(), Client->GetPeerName(), IrcNick);

			if (Client->GetNick() != NULL && strcmp(Client->GetNick(), IrcNick) != 0) {
				m_IRC->WriteLine("NICK :%s", Client->GetNick());
			}

			m_Client->WriteLine(":%s 001 %s :Welcome to the Internet Relay Network %s", m_IRC->GetServer(), IrcNick, IrcNick);

			if (Motd->IsEmpty()) {
				m_Client->WriteLine(":%s 422 %s :MOTD File is missing", m_IRC->GetServer(), IrcNick);
			} else{
				Motd->PlayToUser(this, Log_Motd);
			}

			m_Client->ParseLine("VERSION");

			if (m_IRC->GetUsermodes() != NULL) {
				m_Client->WriteLine(":%s!%s@%s MODE %s +%s", IrcNick, GetUsername(), Client->GetPeerName(), IrcNick, m_IRC->GetUsermodes());
			}

			Keys = m_IRC->GetChannels()->GetSortedKeys();

			i = 0;
			while (Keys != NULL && Keys[i] != NULL) {
				m_Client->WriteLine(":%s!%s@%s JOIN %s", m_IRC->GetCurrentNick(), GetUsername(), m_Client->GetPeerName(), Keys[i]);

				asprintf(&Out, "TOPIC %s", Keys[i]);

				if (Out == NULL) {
					LOGERROR("asprintf() failed.");

					m_Client->Kill("Internal error.");
				} else {
					m_Client->ParseLine(Out);
					free(Out);
				}

				asprintf(&Out, "NAMES %s", Keys[i]);

				if (Out == NULL) {
					LOGERROR("asprintf() failed.");

					m_Client->Kill("Internal error.");
				} else {
					m_Client->ParseLine(Out);
					free(Out);
				}

				i++;
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

	MotdText = g_Bouncer->GetConfig()->ReadString("system.motd");

	if (MotdText != NULL) {
		asprintf(&Out, "Message of the day: %s", MotdText);
		
		if (Out == NULL) {
			LOGERROR("asprintf() failed.");
		} else {
			Privmsg(Out);

			free(Out);
		}
	}

	Modules = g_Bouncer->GetModules();

	for (i = 0; i < Modules->GetLength(); i++) {
		(*Modules)[i]->AttachClient(GetUsername());
	}

	if (m_IRC == NULL) {
		if (GetServer() == NULL) {
			Privmsg("You haven't set an IRC server yet. Use /sbnc set server <Hostname> <Port> to do that now.");
			Privmsg("Use /sbnc help to see a list of available commands.");
		} else if (m_Config->ReadInteger("user.quitted") == 2) {
			Privmsg("You are not connected to an irc server. Use /sbnc jump to reconnect now.");
		}
	}

	if (!GetLog()->IsEmpty()) {
		Privmsg("You have new messages. Use '/msg -sBNC read' to view them.");
	}
}

/**
 * CheckPassword
 *
 * Checks whether this user's password and the supplied password match.
 *
 * @param Password a password
 */
bool CUser::CheckPassword(const char *Password) const {
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

/**
 * GetNick
 *
 * Returns the current nick of the user.
 */
const char *CUser::GetNick(void) const {
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

/**
 * GetUsername
 *
 * Returns the user's username.
 */
const char *CUser::GetUsername(void) const {
	return m_Name;
}

/**
 * GetRealname
 *
 * Returns the user's real name.
 */
const char *CUser::GetRealname(void) const {
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

/**
 * GetConfig
 *
 * Returns the config object for this user.
 */
CConfig *CUser::GetConfig(void) {
	return m_Config;
}

/**
 * Simulate
 *
 * Executes the specified command in this user's context and redirects
 * the output to the given client (or a dummy client if FakeClient is NULL).
 *
 * @param Command the command and its parameters
 * @param FakeClient the client which is used for sending replies
 */
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

/**
 * Reconnect
 *
 * Reconnects the user to an IRC server.
 */
void CUser::Reconnect(void) {
	const char *Server;
	int Port;

	if (m_IRC != NULL) {
		if (GetClientConnection() != NULL) {
			GetClientConnection()->SetPreviousNick(m_IRC->GetCurrentNick());
		}

		m_IRC->Kill("Reconnecting.");

		SetIRCConnection(NULL);
	}

	Server = GetServer();
	Port = GetPort();

	if (Server == NULL || Port == 0) {
		ScheduleReconnect(60);

		return;
	}

	if (GetIPv6()) {
		g_Bouncer->LogUser(this, "Trying to reconnect to [%s]:%d for %s", Server, Port, m_Name);
	} else {
		g_Bouncer->LogUser(this, "Trying to reconnect to %s:%d for %s", Server, Port, m_Name);
	}

	m_LastReconnect = g_CurrentTime;

	const char *BindIp = GetVHost();

	if (BindIp == NULL || BindIp[0] == '\0') {
		BindIp = g_Bouncer->GetConfig()->ReadString("system.vhost");
	}

	if (BindIp != NULL && BindIp[0] == '\0') {
		BindIp = NULL;
	}

	if (GetIdent() != NULL) {
		g_Bouncer->SetIdent(GetIdent());
	} else {
		g_Bouncer->SetIdent(m_Name);
	}

	CIRCConnection *Connection = new CIRCConnection(Server, Port, this, BindIp, GetSSL(), GetIPv6() ? AF_INET6 : AF_INET);

	CHECK_ALLOC_RESULT(Connection, new) {
		return;
	} CHECK_ALLOC_RESULT_END;

	SetIRCConnection(Connection);

	g_Bouncer->Log("Connection initialized for %s. Waiting for response...", GetUsername());

	RescheduleReconnectTimer();
}

/**
 * ShouldReconnect
 *
 * Determines whether this user should reconnect (yet).
 */
bool CUser::ShouldReconnect(void) const {
	int Interval = g_Bouncer->GetConfig()->ReadInteger("system.interval");

	if (GetServer() == NULL || GetPort() == 0) {
		return false;
	}

	if (Interval == 0) {
		Interval = 15;
	}

	if (m_IRC == NULL && m_ReconnectTime <= g_CurrentTime && g_CurrentTime - m_LastReconnect > 120 && g_CurrentTime - g_LastReconnect > Interval && m_Config->ReadInteger("user.quitted") == 0) {
		return true;
	} else {
		return false;
	}
}

/**
 * ScheduleReconnect
 *
 * Schedules a connect attempt.
 *
 * @param Delay the delay
 */
void CUser::ScheduleReconnect(int Delay) {
	int MaxDelay, Interval;

	if (m_IRC != NULL) {
		return;
	}

	m_Config->WriteInteger("user.quitted", 0);

	MaxDelay = Delay;
	Interval = g_Bouncer->GetConfig()->ReadInteger("system.interval");

	if (Interval == 0) {
		Interval = 15;
	}

	if (g_CurrentTime - g_LastReconnect < Interval && MaxDelay < Interval) {
		MaxDelay = Interval;
	}

	if (g_CurrentTime - m_LastReconnect < 120 && MaxDelay < 120 && !IsAdmin()) {
		MaxDelay = 120;
	}

	if (m_ReconnectTime < g_CurrentTime + MaxDelay) {
		m_ReconnectTime = g_CurrentTime + MaxDelay;

		RescheduleReconnectTimer();
	}

	if (GetServer() != NULL && GetClientConnection() != NULL) {
		char *Out;
		asprintf(&Out, "Scheduled reconnect in %d seconds.", m_ReconnectTime - g_CurrentTime);

		CHECK_ALLOC_RESULT(Out, asprintf) {} else {
			Privmsg(Out);

			free(Out);
		} CHECK_ALLOC_RESULT_END;
	}
}

/**
 * Privmsg
 *
 * Sends a message to the user.
 *
 * @param Text the text
 */
void CUser::Privmsg(const char *Text) {
	const char *Nick = GetNick();

	if (m_Client != NULL && Nick != NULL) {
		m_Client->WriteLine(":-sBNC!bouncer@shroudbnc.info PRIVMSG %s :%s", Nick, Text);
	}
}

/**
 * RealNotice
 *
 * Sends a notice to the user.
 *
 * @param Text the text
 */
void CUser::RealNotice(const char *Text) {
	const char *Nick = GetNick();

	if (m_Client != NULL && Nick != NULL) {
		m_Client->WriteLine(":-sBNC!bouncer@shroudbnc.info NOTICE %s :%s", Nick, Text);
	}
}

/**
 * GetIRCUptime
 *
 * Returns the number of seconds this user has been connected to an IRC server.
 */
unsigned int CUser::GetIRCUptime(void) const {
	if (m_IRC != NULL) {
		return g_CurrentTime - m_LastReconnect;
	} else {
		return 0;
	}
}

/**
 * GetLog
 *
 * Returns the log object for the user.
 */
CLog *CUser::GetLog(void) {
	return m_Log;
}

/**
 * Log
 *
 * Creates a new entry in the user's log.
 *
 * @param Format the format string
 * @param ... additional parameters which are used in the format string
 */
void CUser::Log(const char *Format, ...) {
	char *Out;
	va_list marker;

	va_start(marker, Format);
	vasprintf(&Out, Format, marker);
	va_end(marker);

	CHECK_ALLOC_RESULT(Out, vasprintf) {} else {
		if (GetClientConnection() == NULL) {
			m_Log->WriteLine(FormatTime(g_CurrentTime), "%s", Out);
		}

		Privmsg(Out);

		free(Out);
	} CHECK_ALLOC_RESULT_END;
}

/**
 * Lock
 *
 * Locks (i.e. suspends) the user.
 */
void CUser::Lock(void) {
	m_Config->WriteInteger("user.lock", 1);
}

/**
 * Unlock
 *
 * Unlocks (i.e. unsuspends) the user.
 */
void CUser::Unlock(void) {
	m_Config->WriteInteger("user.lock", 0);
}

/**
 * SetIRCConnection
 *
 * Sets the IRC connection object for the user.
 *
 * @param IRC the irc connection
 */
void CUser::SetIRCConnection(CIRCConnection *IRC) {
	const CVector<CModule *> *Modules;
	CIRCConnection *OldIRC;
	bool WasNull;

	if (m_IRC == NULL) {
		WasNull = true;
	} else {
		WasNull = false;
	}

	OldIRC = m_IRC;
	m_IRC = IRC;

	Modules = g_Bouncer->GetModules();

	if (IRC == NULL && !WasNull) {
		if (OldIRC->IsConnected()) {
			g_Bouncer->LogUser(this, "%s was disconnected from the server.", GetUsername());
		} else {
			g_Bouncer->LogUser(this, "An attempt of connecting to a server failed for user %s.", GetUsername());
		}

		for (unsigned int i = 0; i < Modules->GetLength(); i++) {
			(*Modules)[i]->ServerDisconnect(GetUsername());
		}
	} else if (IRC) {
		for (unsigned int i = 0; i < Modules->GetLength(); i++) {
			(*Modules)[i]->ServerConnect(GetUsername());
		}

		m_LastReconnect = g_CurrentTime;

		IRC->SetTrafficStats(m_IRCStats);
	}
}

/**
 * SetClientConnection
 *
 * Sets the client connection object for the user.
 *
 * @param Client the new client object
 * @param DontSetAway determines whether the /away command is
					  used for setting an away reason
 */
void CUser::SetClientConnection(CClientConnection *Client, bool DontSetAway) {
	const CVector<CModule *> *Modules;
	unsigned int i;
	const char *DropModes, *AwayNick, *AwayText, *Timestamp, *AwayMessage;
	hash_t<CChannel *> *Channel;
	sockaddr *Remote;

	if (m_Client == NULL && Client == NULL) {
		return;
	}

	if (Client != NULL) {
		Remote = Client->GetRemoteAddress();
	}

	if (m_Client == NULL) {
		g_Bouncer->Log("User %s logged on (from %s[%s]).", GetUsername(), Client->GetPeerName(), (Remote != NULL) ? IpToString(Remote) : "unknown");

		m_Config->WriteInteger("user.seen", g_CurrentTime);
	}

	if (m_Client != NULL && Client != NULL) {
		g_Bouncer->Log("User %s re-attached to an already active session. Old session was closed.", m_Name);

		m_Client->SetOwner(NULL);
		m_Client->Kill("Another client has connected.");

		SetClientConnection(NULL, true);
	}

	m_Client = Client;

	if (Client != NULL) {
		Client->SetTrafficStats(m_ClientStats);

		return;
	}

	if (!DontSetAway) {
		g_Bouncer->Log("User %s logged off.", GetUsername());

		m_Config->WriteInteger("user.seen", g_CurrentTime);

		AwayMessage = m_Config->ReadString("user.awaymessage");

		if (AwayMessage != NULL && m_IRC != NULL) {
			i = 0;
			while ((Channel = m_IRC->GetChannels()->Iterate(i++)) != NULL) {
				m_IRC->WriteLine("PRIVMSG %s :\001ACTION is now away: %s\001", Channel->Name, AwayMessage);
			}
		}
	}

	Modules = g_Bouncer->GetModules();

	for (i = 0; i < Modules->GetLength(); i++) {
		(*Modules)[i]->DetachClient(GetUsername());
	}

	if (m_IRC != NULL && !DontSetAway) {
		DropModes = m_Config->ReadString("user.dropmodes");

		if (DropModes != NULL && m_IRC->GetCurrentNick() != NULL) {
			m_IRC->WriteLine("MODE %s -%s", m_IRC->GetCurrentNick(), DropModes);
		}

		AwayNick = m_Config->ReadString("user.awaynick");

		if (AwayNick != NULL) {
			m_IRC->WriteLine("NICK %s", AwayNick);
		}

		AwayText = m_Config->ReadString("user.away");

		if (AwayText != NULL) {
			bool AppendTS = (m_Config->ReadInteger("user.ts") != 0);

			if (!AppendTS) {
				m_IRC->WriteLine("AWAY :%s", AwayText);
			} else {
				Timestamp = FormatTime(g_CurrentTime);

				m_IRC->WriteLine("AWAY :%s (Away since %s)", AwayText, Timestamp);
			}
		}
	}
}

/**
 * SetAdmin
 *
 * Sets whether the user is an admin.
 *
 * @param Admin a boolean flag
 */
void CUser::SetAdmin(bool Admin) {
	m_Config->WriteInteger("user.admin", Admin ? 1 : 0);
	m_IsAdminCache = (int)Admin;

	if (Admin) {
		g_Bouncer->GetAdminUsers()->Insert(this);
	} else {
		g_Bouncer->GetAdminUsers()->Remove(this);
	}
}

/**
 * IsAdmin
 *
 * Returns whether the user is an admin.
 */
bool CUser::IsAdmin(void) const {
	if (m_IsAdminCache == -1) {
		m_IsAdminCache = m_Config->ReadInteger("user.admin");
	}

	if (m_IsAdminCache != 0) {
		return true;
	} else {
		return false;
	}
}

/**
 * SetPassword
 *
 * Sets the user's password.
 *
 * @param Password the new password
 */
void CUser::SetPassword(const char *Password) {
	if (g_Bouncer->GetConfig()->ReadInteger("system.md5") != 0) {
		Password = g_Bouncer->MD5(Password);
	}

	m_Config->WriteString("user.password", Password);
}

/**
 * SetServer
 *
 * Sets the user's IRC server.
 *
 * @param Server the hostname of the server
 */
void CUser::SetServer(const char *Server) {
	USER_SETFUNCTION(server, Server);

	if (Server != NULL && !IsQuitted() && GetIRCConnection() == NULL) {
		ScheduleReconnect();
	}
}

/**
 * GetServer
 *
 * Returns the user's IRC server.
 */
const char *CUser::GetServer(void) const {
	return m_Config->ReadString("user.server");
}

/**
 * SetPort
 *
 * Sets the port of the user's IRC server.
 *
 * @param Port the port
 */
void CUser::SetPort(int Port) {
	m_Config->WriteInteger("user.port", Port);
}

/**
 * GetPort
 *
 * Returns the port of the user's IRC server.
 */
int CUser::GetPort(void) const {
	int Port = m_Config->ReadInteger("user.port");

	if (Port == 0) {
		return 6667;
	} else {
		return Port;
	}
}

/**
 * IsLocked
 *
 * Returns whether the user is locked.
 */
bool CUser::IsLocked(void) const {
	if (m_Config->ReadInteger("user.lock") == 0) {
		return false;
	} else {
		return true;
	}
}

/**
 * SetNick
 *
 * Sets the user's nickname.
 *
 * @param Nick the user's nickname
 */
void CUser::SetNick(const char *Nick) {
	USER_SETFUNCTION(nick, Nick);
}

/**
 * SetRealname
 *
 * Sets the user's realname.
 *
 * @param Realname the new realname
 */
void CUser::SetRealname(const char *Realname) {
	USER_SETFUNCTION(realname, Realname);
}

/**
 * MarkQuitted
 *
 * Disconnects the user from the IRC server and tells the bouncer
 * not to automatically reconnect this user.
 *
 * @param RequireManualJump specifies whether the user has to manually
 *							reconnect, if you specify "false", the user
 *							will reconnect when the bouncer is restarted
 */
void CUser::MarkQuitted(bool RequireManualJump) {
	m_Config->WriteInteger("user.quitted", RequireManualJump ? 2 : 1);
}

/**
 * IsQuitted
 *
 * Returns whether the user is marked as quitted.
 */
int CUser::IsQuitted(void) const {
	return m_Config->ReadInteger("user.quitted.");
}

/**
 * LogBadLogin
 *
 * Logs a bad login attempt.
 *
 * @param Peer the IP address of the client
 */
void CUser::LogBadLogin(sockaddr *Peer) {
	badlogin_t BadLogin;

	for (unsigned int i = 0; i < m_BadLogins.GetLength(); i++) {
		if (CompareAddress(m_BadLogins[i].Address, Peer) == 0 && m_BadLogins[i].Count < 3) {
			m_BadLogins[i].Count++;

			return;
		}
	}

	BadLogin.Count = 1;
	BadLogin.Address = (sockaddr *)umalloc(SOCKADDR_LEN(Peer->sa_family));
	memcpy(BadLogin.Address, Peer, SOCKADDR_LEN(Peer->sa_family));

	m_BadLogins.Insert(BadLogin);
}

/**
 * IsIpBlocked
 *
 * Checks whether the specified IP address is blocked.
 *
 * @param Peer the IP address
 */
bool CUser::IsIpBlocked(sockaddr *Peer) const {
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

/**
 * BadLoginPulse
 *
 * Periodically expires old "bad logins".
 */
void CUser::BadLoginPulse(void) {
	for (int i = m_BadLogins.GetLength() - 1; i >= 0; i--) {
		if (m_BadLogins[i].Count > 0) {
			m_BadLogins[i].Count--;

			if (m_BadLogins[i].Count <= 0) {
				ufree(m_BadLogins[i].Address);
				m_BadLogins.Remove(i);
			}
		}
	}
}

/**
 * GetClientStats
 *
 * Returns traffic statistics for the user's client sessions.
 */
const CTrafficStats *CUser::GetClientStats(void) const {
	return m_ClientStats;
}

/**
 * GetIRCStats
 *
 * Returns traffic statistics for the user's IRC sessions.
 */
const CTrafficStats *CUser::GetIRCStats(void) const {
	return m_IRCStats;
}

/**
 * GetKeyring
 *
 * Returns a list of known channel keys for the user.
 */
CKeyring *CUser::GetKeyring(void) {
	return m_Keys;
}

/**
 * BadLoginTimer
 *
 * Thunks calls to the BadLoginPulse() functions.
 *
 * @param Now the current time
 * @param User a CUser object
 */
bool BadLoginTimer(time_t Now, void *User) {
	((CUser *)User)->BadLoginPulse();

	return true;
}

/**
 * UserReconnectTimer
 *
 * Checks whether the user should be reconnected to an IRC server.
 *
 * @param Now the current time
 * @param User a CUser object
 */
bool UserReconnectTimer(time_t Now, void *User) {
	int Interval;

	if (((CUser *)User)->GetIRCConnection() || !(g_Bouncer->GetStatus() == STATUS_RUN || g_Bouncer->GetStatus() == STATUS_PAUSE)) {
		return false;
	}

	Interval = g_Bouncer->GetConfig()->ReadInteger("system.interval");

	if (Interval == 0) {
		Interval = 15;
	}

	if (g_CurrentTime - g_LastReconnect > Interval) {
		((CUser *)User)->Reconnect();
	} else {
		((CUser*)User)->ScheduleReconnect(Interval);
	}

	((CUser *)User)->m_ReconnectTime = g_CurrentTime;

	return false;
}

/**
 * GetLastSeen
 *
 * Returns a TS when the user was last seen.
 */
time_t CUser::GetLastSeen(void) const {
	return m_Config->ReadInteger("user.seen");
}

/**
 * GetAwayNick
 *
 * Returns the user's away nick.
 */
const char *CUser::GetAwayNick(void) const {
	return m_Config->ReadString("user.awaynick");
}

/**
 * SetAwayNick
 *
 * Sets the user's away nick.
 *
 * @param Nick the new away nick
 */
void CUser::SetAwayNick(const char *Nick) {
	USER_SETFUNCTION(awaynick, Nick);

	if (m_Client == NULL && m_IRC != NULL) {
		m_IRC->WriteLine("NICK :%s", Nick);
	}
}

/**
 * GetAwayText
 *
 * Returns the user's away reason.
 */
const char *CUser::GetAwayText(void) const {
	return m_Config->ReadString("user.away");
}

void CUser::SetAwayText(const char *Reason) {
	USER_SETFUNCTION(away, Reason);

	if (m_Client == NULL && m_IRC != NULL) {
		m_IRC->WriteLine("AWAY :%s", Reason);
	}
}

/**
 * GetVHost
 *
 * Returns the user's virtual host.
 */
const char *CUser::GetVHost(void) const {
	return m_Config->ReadString("user.ip");
}

/**
 * SetVHost
 *
 * Sets the user's virtual host.
 *
 * @param VHost the new virtual host
 */
void CUser::SetVHost(const char *VHost) {
	USER_SETFUNCTION(ip, VHost);
}

/**
 * GetDelayJoin
 *
 * Returns whether the user is "delay joined".
 */
int CUser::GetDelayJoin(void) const {
	return m_Config->ReadInteger("user.delayjoin");
}

/**
 * SetDelayJoin
 *
 * Sets whether "delay join" is enabled for the user.
 *
 * @param DelayJoin a boolean flag
 */
void CUser::SetDelayJoin(int DelayJoin) {
	m_Config->WriteInteger("user.delayjoin", DelayJoin);
}

/**
 * GetConfigChannels
 *
 * Returns a comma-seperated list of channels the user
 * automatically joins when connected to an IRC server.
 */
const char *CUser::GetConfigChannels(void) const {
	return m_Config->ReadString("user.channels");
}

/**
 * SetConfigChannels
 *
 * Sets the user's list of channels.
 *
 * @param Channels the channels
 */
void CUser::SetConfigChannels(const char *Channels) {
	USER_SETFUNCTION(channels, Channels);
}

/**
 * GetSuspendReason
 *
 * Returns the suspend reason for the user.
 */
const char *CUser::GetSuspendReason(void) const {
	return m_Config->ReadString("user.suspend");
}

/**
 * SetSuspendReason
 *
 * Sets the suspend reason for the user.
 *
 * @param Reason the new reason
 */
void CUser::SetSuspendReason(const char *Reason) {
	USER_SETFUNCTION(suspend, Reason);
}

/**
 * GetServerPassword
 *
 * Returns the user's server password.
 */
const char *CUser::GetServerPassword(void) const {
	return m_Config->ReadString("user.spass");
}

/**
 * SetServerPassword
 *
 * Sets the user's server password.
 *
 * @param Password the new password
 */
void CUser::SetServerPassword(const char *Password) {
	USER_SETFUNCTION(spass, Password);
}

/**
 * GetAutoModes
 *
 * Returns the user's "auto usermodes".
 */
const char *CUser::GetAutoModes(void) const {
	return m_Config->ReadString("user.automodes");
}

/**
 * SetAutoModes
 *
 * Sets the user's "auto modes".
 *
 * @param AutoModes the new usermodes
 */
void CUser::SetAutoModes(const char *AutoModes) {
	USER_SETFUNCTION(automodes, AutoModes);
}

/**
 * GetDropModes
 *
 * Returns the user's "drop usermodes".
 */
const char *CUser::GetDropModes(void) const {
	return m_Config->ReadString("user.dropmodes");
}

/**
 * SetDropModes
 *
 * Sets the user's "drop modes".
 *
 * @param DropModes the new usermodes
 */
void CUser::SetDropModes(const char *DropModes) {
	USER_SETFUNCTION(dropmodes, DropModes);
}

/**
 * GetClientCertificates
 *
 * Returns the user's list of client certificates.
 */
const CVector<X509 *> *CUser::GetClientCertificates(void) const {
#ifdef USESSL
	return &m_ClientCertificates;
#else
	return NULL;
#endif
}

/**
 * AddClientCertificate
 *
 * Inserts a new client certificate into the user's certificate chain.
 *
 * @param Certificate the certificate
 */
bool CUser::AddClientCertificate(const X509 *Certificate) {
#ifdef USESSL
	X509 *DuplicateCertificate;

	for (unsigned int i = 0; i < m_ClientCertificates.GetLength(); i++) {
		if (X509_cmp(m_ClientCertificates[i], Certificate) == 0) {
			return true;
		}
	}

	DuplicateCertificate = X509_dup(const_cast<X509 *>(Certificate));

	m_ClientCertificates.Insert(DuplicateCertificate);

	return PersistCertificates();
#else
	return false;
#endif
}

/**
 * RemoveClientCertificate
 *
 * Removes a certificate from the user's certificate chain.
 *
 * @param Certificate the certificate
 */
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
/**
 * PersistCertificates
 *
 * Stores the client certificates in a file.
 */
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

/**
 * FindClientCertificate
 *
 * Checks whether the specified certificate is in the user's chain of
 * client certificates.
 *
 * @param Certificate a certificate
 */
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

/**
 * GetSSL
 *
 * Returns whether the user is using SSL for IRC connections.
 */
bool CUser::GetSSL(void) const {
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

/**
 * SetSSL
 *
 * Sets whether the user should use SSL for connections to IRC servers.
 *
 * @param SSL a boolean flag
 */
void CUser::SetSSL(bool SSL) {
#ifdef USESSL
	if (SSL) {
		m_Config->WriteInteger("user.ssl", 1);
	} else {
		m_Config->WriteInteger("user.ssl", 0);
	}
#endif
}

/**
 * GetIdent
 *
 * Returns the user's custom ident, or NULL if there is no custom ident.
 */
const char *CUser::GetIdent(void) const {
	return m_Config->ReadString("user.ident");
}

/**
 * SetIdent
 *
 * Sets the user's custom ident.
 *
 * @param Ident the new ident
 */
void CUser::SetIdent(const char *Ident) {
	USER_SETFUNCTION(ident, Ident);
}

/**
 * GetTagString
 *
 * Returns a tag's value as a string.
 *
 * @param Tag the name of the tag
 */
const char *CUser::GetTagString(const char *Tag) const {
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

/**
 * GetTagInteger
 *
 * Returns a tag's value as an integer.
 *
 * @param Tag the name of the tag
 */
int CUser::GetTagInteger(const char *Tag) const {
	const char *Value = GetTagString(Tag);

	if (Value != NULL) {
		return atoi(Value);
	} else {
		return 0;
	}
}

/**
 * SetTagString
 *
 * Sets a tag's value.
 *
 * @param Tag the name of the tag
 * @param Value the new value
 */
bool CUser::SetTagString(const char *Tag, const char *Value) {
	bool ReturnValue;
	char *Setting;
	const CVector<CModule *> *Modules;

	if (Tag == NULL) {
		return false;
	}

	asprintf(&Setting, "tag.%s", Tag);

	CHECK_ALLOC_RESULT(Setting, asprintf) {
		return false;
	} CHECK_ALLOC_RESULT_END;

	Modules = g_Bouncer->GetModules();

	for (unsigned int i = 0; i < Modules->GetLength(); i++) {
		(*Modules)[i]->UserTagModified(Tag, Value);
	}

	ReturnValue = m_Config->WriteString(Setting, Value);

	Modules = g_Bouncer->GetModules();

	return ReturnValue;
}

/**
 * SetTagInteger
 *
 * Sets a tag's value.
 *
 * @param Tag the name of the tag
 * @param Value the new value
 */
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

/**
 * GetTagName
 *
 * Returns the Index-th tag.
 *
 * @param Index the index
 */
const char *CUser::GetTagName(int Index) const {
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

/**
 * GetIPv6
 *
 * Returns whether the user is using IPv6 for IRC connections.
 */
bool CUser::GetIPv6(void) const {
	if (m_Config->ReadInteger("user.ipv6") != 0) {
		return true;
	} else {
		return false;
	}
}

/**
 * SetIPv6
 *
 * Sets whether the user should use IPv6 for connections to IRC servers.
 *
 * @param IPv6 a boolean flag
 */
void CUser::SetIPv6(bool IPv6) {
	m_Config->WriteInteger("user.ipv6", IPv6 ? 1 : 0);
}

/**
 * FormatTime
 *
 * Formats a timestamp while considering the user's GMT offset.
 *
 * @param Timestamp the timestamp
 */
const char *CUser::FormatTime(time_t Timestamp) const {
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

/**
 * SetGmtOffset
 *
 * Sets the user's GMT offset (in minutes).
 *
 * @param Offset the new offset
 */
void CUser::SetGmtOffset(int Offset) {
	m_Config->WriteInteger("user.tz", Offset);
}

/**
 * GetGmtOffset
 *
 * Returns the user's GMT offset (in minutes).
 */
int CUser::GetGmtOffset(void) const {
	const char *Offset;
	tm GMTime;
	tm *CurrentTime;

	Offset = m_Config->ReadString("user.tz");

	if (Offset == NULL) {
		CurrentTime = gmtime(&g_CurrentTime);
		memcpy(&GMTime, CurrentTime, sizeof(GMTime));

		CurrentTime = localtime(&g_CurrentTime);

		return (CurrentTime->tm_hour - GMTime.tm_hour) * 60 + (CurrentTime->tm_min - GMTime.tm_min);
	} else {
		return atoi(Offset);
	}
}

/**
 * SetSystemNotices
 *
 * Sets whether the user should receive system notices.
 *
 * @param SystemNotices a boolean flag
 */
void CUser::SetSystemNotices(bool SystemNotices) {
	m_Config->WriteInteger("user.ignsysnotices", SystemNotices ? 0 : 1);
}

/**
 * GetSystemNotices
 *
 * Returns whether the user should receive system notices.
 */
bool CUser::GetSystemNotices(void) const {
	if (m_Config->ReadInteger("user.ignsysnotices") == 0) {
		return true;
	} else {
		return false;
	}
}

/**
 * SetAwayMessage
 *
 * Sets the user's away message which is spammed to every channel the
 * user is in when he disconnects.
 *
 * @param Text the new away message
 */
void CUser::SetAwayMessage(const char *Text) {
	m_Config->WriteString("user.awaymessage", Text);
}

/**
 * GetAwayMessage
 *
 * Returns the user's away message or NULL if none is set.
 */
const char *CUser::GetAwayMessage(void) const {
	return m_Config->ReadString("user.awaymessage");
}

/**
 * SetAwayMessage
 *
 * Sets the "lean" mode for a user.
 *
 * @param Mode the mode (0, 1 or 2)
 */
void CUser::SetLeanMode(unsigned int Mode) {
	m_Config->WriteInteger("user.lean", Mode);
	m_LeanModeCache = Mode;
}

/**
 * GetLeanMode
 *
 * Returns the state of the "lean" flag.
 */
unsigned int CUser::GetLeanMode(void) {
	if (m_LeanModeCache == -1) {
		m_LeanModeCache = m_Config->ReadInteger("user.lean");
	}

	return m_LeanModeCache;
}

bool CUser::MemoryAddBytes(size_t Bytes) {
	if (rand() < RAND_MAX / 6) {
		//return false;
	}

	if (m_ManagedMemory + Bytes > g_Bouncer->GetResourceLimit("memory")) {
		return false;
	}

	m_ManagedMemory += Bytes;

	return true;
}

void CUser::MemoryRemoveBytes(size_t Bytes) {
	m_ManagedMemory -= Bytes;
}

size_t CUser::MemoryGetSize(void) {
	return m_ManagedMemory;
}

size_t CUser::MemoryGetLimit(void) {
	return g_Bouncer->GetResourceLimit("memory");
}

mmanager_t *CUser::MemoryGetManager(void) {
	if (m_MemoryManager == NULL) {
		m_MemoryManager = (mmanager_t *)malloc(sizeof(mmanager_t));

		m_MemoryManager->RealManager = this;
		m_MemoryManager->ReferenceCount = 0;
	}

	return m_MemoryManager;
}

const char *CUser::SimulateWithResult(const char *Command) {
	static CFakeClient *FakeClient = NULL;

	delete FakeClient;

	FakeClient = new CFakeClient();

	Simulate(Command, FakeClient);

	return FakeClient->GetData();
}

bool GlobalUserReconnectTimer(time_t Now, void *Null) {
	int i = 0;

	while (hash_t<CUser *> *UserHash = g_Bouncer->GetUsers()->Iterate(i++)) {
		if (UserHash->Value->ShouldReconnect()) {
			UserHash->Value->Reconnect();

			break;
		}
	}

	CUser::RescheduleReconnectTimer();

	return true;
}

void CUser::RescheduleReconnectTimer(void) {
	int i = 0;
	time_t ReconnectTime;

	if (g_ReconnectTimer == NULL) {
		g_ReconnectTimer = new CTimer(60, true, GlobalUserReconnectTimer, NULL);
	}

	ReconnectTime = g_ReconnectTimer->GetNextCall();

	while (hash_t<CUser *> *UserHash = g_Bouncer->GetUsers()->Iterate(i++)) {
		if (UserHash->Value->m_ReconnectTime > g_CurrentTime && UserHash->Value->m_ReconnectTime < ReconnectTime && UserHash->Value->GetIRCConnection() == NULL) {
			ReconnectTime = UserHash->Value->m_ReconnectTime;
		}
	}

	g_ReconnectTimer->Reschedule(ReconnectTime);
}
