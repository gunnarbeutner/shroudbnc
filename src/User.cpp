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
#ifdef HAVE_LIBSSL
	X509 *Cert;
	FILE *ClientCert;
#endif
	int rc;

	m_PrimaryClient = NULL;
	m_ClientMultiplexer = new CClientConnectionMultiplexer(this);
	m_IRC = NULL;
	m_Name = strdup(Name);

	if (AllocFailed(m_Name)) {
		g_Bouncer->Fatal();
	}

	rc = asprintf(&Out, "users/%s.conf", Name);

	if (RcFailed(rc)) {
		g_Bouncer->Fatal();
	}

	m_Config = new CConfig(Out, this);

	free(Out);

	if (AllocFailed(m_Config)) {
		g_Bouncer->Fatal();
	}

	CacheInitialize(m_ConfigCache, m_Config, "user.");

	m_IRC = NULL;

	m_ReconnectTime = 0;
	m_LastReconnect = 0;
	m_NextProtocolFamily = AF_UNSPEC;

	rc = asprintf(&Out, "users/%s.log", Name);

	if (RcFailed(rc)) {
		g_Bouncer->Fatal();
	}

	m_Log = new CLog(g_Bouncer->BuildPathConfig(Out));

	free(Out);

	if (AllocFailed(m_Log)) {
		g_Bouncer->Fatal();
	}

	m_ClientStats = new CTrafficStats();
	m_IRCStats = new CTrafficStats();

	m_Keys = new CKeyring(m_Config, this);

	m_BadLoginPulse = new CTimer(200, true, BadLoginTimer, this);

#ifdef HAVE_LIBSSL
	rc = asprintf(&Out, "users/%s.pem", Name);

	if (RcFailed(rc)) {
		g_Bouncer->Fatal();
	}

	ClientCert = fopen(g_Bouncer->BuildPathConfig(Out), "r");

	free(Out);

	if (ClientCert != NULL) {
		while ((Cert = PEM_read_X509(ClientCert, NULL, NULL, NULL)) != NULL) {
			m_ClientCertificates.Insert(Cert);
		}

		fclose(ClientCert);
	}

#endif

	if (IsQuitted() != 2) {
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
	m_ClientMultiplexer->Kill("Removing user.");

	if (m_IRC != NULL) {
		m_IRC->Kill("-)(- If you can't see the fnords, they can't eat you.");
	}

	m_Config->Destroy();
	delete m_Log;

	delete m_ClientStats;
	delete m_IRCStats;

	delete m_Keys;

	free(m_Name);

	if (m_BadLoginPulse != NULL) {
		m_BadLoginPulse->Destroy();
	}

#ifdef HAVE_LIBSSL
	for (int i = 0; i < m_ClientCertificates.GetLength(); i++) {
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

	for (int i = 0; i < Modules->GetLength(); i++) {
		(*Modules)[i]->UserLoad(m_Name);
	}
}

/**
 * GetPrimaryClientConnection
 *
 * Returns the primary client connection for the user, or NULL
 * if nobody is currently logged in as the user.
 */
CClientConnection *CUser::GetPrimaryClientConnection(void) {
	return m_PrimaryClient;
}

/**
 * GetClientConnectionMultiplexer
 *
 * Returns the client multiplexer for this user.
 */
CClientConnection *CUser::GetClientConnectionMultiplexer(void) {
	if (m_Clients.GetLength() == 0) {
		return NULL;
	} else {
		return m_ClientMultiplexer;
	}
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
	int i;
	bool Added = false;
	bool FirstClient;
	int rc;

	if (IsLocked()) {
		Reason = GetSuspendReason();

		if (Reason == NULL) {
			Client->Kill("*** You cannot attach to this user.");
		} else {
			rc = asprintf(&Out, "*** Your account is suspended. Reason: %s", Reason);

			if (RcFailed(rc)) {
				Client->Kill("*** Your account is suspended.");
			} else {
				Client->Kill(Out);

				free(Out);
			}
		}

		g_Bouncer->Log("Login for user %s failed: User is locked.", m_Name);

		return;
	}

	FirstClient = (m_Clients.GetLength() == 0);

	Client->SetOwner(this);

	Motd = new CLog("sbnc.motd");

	if (m_IRC != NULL) {
		if (FirstClient) {
			m_IRC->WriteLine("AWAY");
		}

		AutoModes = CacheGetString(m_ConfigCache, automodes);

		if (AutoModes != NULL && m_IRC->GetCurrentNick() != NULL && FirstClient) {
			m_IRC->WriteLine("MODE %s +%s", m_IRC->GetCurrentNick(), AutoModes);
		}

		IrcNick = m_IRC->GetCurrentNick();

		if (IrcNick != NULL) {
			if (Client->GetNick() != NULL && strcmp(Client->GetNick(), IrcNick) != 0) {
				m_IRC->WriteLine("NICK :%s", Client->GetNick());
			}

			Client->ChangeNick(IrcNick);

			Client->WriteLine(":%s 001 %s :Welcome to the Internet Relay Network %s", m_IRC->GetServer(), IrcNick, IrcNick);
                        Client->WriteLine(":%s 002 %s :Your host is %s, running %s", m_IRC->GetServer(), IrcNick, m_IRC->GetServer(), m_IRC->GetServerVersion());
                        Client->WriteLine(":%s 004 %s %s %s %s %s", m_IRC->GetServer(), IrcNick, m_IRC->GetServer(), m_IRC->GetServerVersion(), m_IRC->GetServerUserModes(), m_IRC->GetServerChanModes());

			if (Motd->IsEmpty()) {
				Client->WriteLine(":%s 422 %s :MOTD File is missing", m_IRC->GetServer(), IrcNick);
			} else{
				Motd->PlayToUser(Client, Log_Motd);
			}

			Client->ParseLine("SYNTH VERSION-FORCEREPLY");

			if (m_IRC->GetUsermodes() != NULL) {
				const char *Site = m_IRC->GetSite();
				Client->WriteLine(":%s!%s MODE %s +%s", IrcNick, Site ? Site : "unknown@unknown.host", IrcNick, m_IRC->GetUsermodes());
			}

			AddClientConnection(Client);
			Added = true;

			CChannel **Channels;

			Channels = (CChannel **)malloc(sizeof(CChannel *) * m_IRC->GetChannels()->GetLength());

			if (AllocFailed(Channels)) {
				delete Motd;

				return;
			}

			i = 0;
			while (hash_t<CChannel *> *ChannelHash = m_IRC->GetChannels()->Iterate(i++)) {
				Channels[i - 1] = ChannelHash->Value;
			}

			int (*SortFunction)(const void *p1, const void *p2) = NULL;

			const char *SortMode = CacheGetString(m_ConfigCache, channelsort);

			if (SortMode == NULL || strcasecmp(SortMode, "cts") == 0 || strcasecmp(SortMode, "") == 0) {
				SortFunction = ChannelTSCompare;
			} else if (strcasecmp(SortMode, "alpha") == 0) {
				SortFunction = ChannelNameCompare;
			} else if (strcasecmp(SortMode, "custom") == 0) {
				const CVector<CModule *> *Modules = g_Bouncer->GetModules();

				for (i = 0; i < Modules->GetLength(); i++) {
					SortFunction = (int (*)(const void *p1, const void *p2))(*Modules)[i]->Command("sorthandler", GetUsername());

					if (SortFunction != NULL) {
						break;
					}
				}
			}

			if (SortFunction == NULL) {
				SortFunction = ChannelTSCompare;
			}

			qsort(Channels, m_IRC->GetChannels()->GetLength(), sizeof(Channels[0]), SortFunction);

			const char *Site = m_IRC->GetSite();

			i = 0;
			for (i = 0; i < m_IRC->GetChannels()->GetLength(); i++) {
				Client->WriteLine(":%s!%s JOIN %s", m_IRC->GetCurrentNick(), Site ? Site : "unknown@unknown.host", Channels[i]->GetName());

				rc = asprintf(&Out, "TOPIC %s", Channels[i]->GetName());

				if (RcFailed(rc)) {
					Client->Kill("Internal error.");
				} else {
					Client->ParseLine(Out);
					free(Out);
				}

				rc = asprintf(&Out, "NAMES %s", Channels[i]->GetName());

				if (RcFailed(rc)) {
					Client->Kill("Internal error.");
				} else {
					Client->ParseLine(Out);
					free(Out);
				}

				if (Client->HasCapability("znc.in/server-time-iso") || (GetAutoBacklog() != NULL && strcasecmp(GetAutoBacklog(), "off") != 0)) {
					Channels[i]->PlayBacklog(Client);
				}
			}

			free(Channels);
		}
	} else {
		Client->WriteLine(":sbnc.beutner.name 001 %s :Welcome to the Internet Relay Network %s", Client->GetNick(), Client->GetNick());

		if (!Motd->IsEmpty()) {
			Motd->PlayToUser(Client, Log_Motd);
		} else {
			Client->WriteLine(":sbnc.beutner.name 422 %s :MOTD File is missing", Client->GetNick());
		}

		if (IsQuitted() != 2) {
			ScheduleReconnect(0);
		}
	}

	delete Motd;

	if (!Added) {
		AddClientConnection(Client);
	}

	MotdText = g_Bouncer->GetMotd();

	if (MotdText != NULL) {
		rc = asprintf(&Out, "Message of the day: %s", MotdText);

		if (!RcFailed(rc)) {
			Client->Privmsg(Out);

			free(Out);
		}
	}

	if (m_IRC == NULL) {
		if (GetServer() == NULL) {
			Client->Privmsg("You haven't set an IRC server yet. Use "
				"/msg -sBNC set server <Hostname> <Port> to do that now.");
			Client->Privmsg("Use /msg -sBNC help to see a list of available commands.");
		} else if (IsQuitted() == 2) {
			Client->Privmsg("You are not connected to an IRC server. Use "
				"/msg -sBNC jump to reconnect now.");
		} else {
			Client->Privmsg("The connection to the IRC server was lost. "
				"You will be reconnected shortly.");
		}
	}

	if (!GetLog()->IsEmpty()) {
		Client->Privmsg("You have new messages. Use '/msg -sBNC read' to view them.");
	}
}

/**
 * CheckPassword
 *
 * Checks whether this user's password and the supplied password match.
 *
 * @param Password a password
 */
bool CUser::CheckPassword(const char *Password) {
	const char *RealPass = CacheGetString(m_ConfigCache, password);

	if (RealPass == NULL || Password == NULL || strlen(Password) == 0) {
		return false;
	}

	if (g_Bouncer->GetMD5()) {
		Password = UtilMd5(Password, SaltFromHash(RealPass));
	}

	return (strcmp(RealPass, Password) == 0);
}

/**
 * GetNick
 *
 * Returns the current nick of the user.
 */
const char *CUser::GetNick(void) const {
	if (m_PrimaryClient != NULL && m_PrimaryClient->GetNick() != NULL) {
		return m_PrimaryClient->GetNick();
	} else if (m_IRC != NULL && m_IRC->GetCurrentNick() != NULL) {
		return m_IRC->GetCurrentNick();
	} else {
		const char *Nick = CacheGetString(m_ConfigCache, awaynick);

		if (Nick != NULL && Nick[0] != '\0') {
			return Nick;
		}

		Nick = CacheGetString(m_ConfigCache, nick);

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
	const char *Realname = CacheGetString(m_ConfigCache, realname);

	if (Realname == NULL) {
		return "shroudBNC User";
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
	bool FakeWasNull, FakeWasRegistered;
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

	if (AllocFailed(CommandDup)) {
		return;
	}

	if (FakeClient == NULL) {
		FakeClient = new CClientConnection(INVALID_SOCKET);

		if (AllocFailed(FakeClient)) {
			free(CommandDup);

			return;
		}
	}

	OldOwner = FakeClient->GetOwner();
	FakeClient->SetOwner(this);

	if (!IsRegisteredClientConnection(FakeClient)) {
		AddClientConnection(FakeClient, true);
		FakeWasRegistered = false;
	} else {
		FakeWasRegistered = true;
	}

	FakeClient->ParseLine(CommandDup);

	if (!FakeWasRegistered) {
		RemoveClientConnection(FakeClient, true);
	}

	FakeClient->SetOwner(OldOwner);

	if (FakeWasNull) {
		FakeClient->Destroy();
	}

	free(CommandDup);
}

/**
 * IsRegisteredClientConnection
 *
 * Checks whether the specified client connection is registered for the user.
 *
 * @param Client the client connection
 */
bool CUser::IsRegisteredClientConnection(CClientConnection *Client) {
	for (int i = 0; i < m_Clients.GetLength(); i++) {
		if (m_Clients[i].Client == Client) {
			return true;
		}
	}

	return false;
}

/**
 * Reconnect
 *
 * Reconnects the user to an IRC server.
 */
void CUser::Reconnect(void) {
	const char *Server;
	int Port, i;

	if (m_IRC != NULL) {
		m_IRC->Kill("Reconnecting.");

		SetIRCConnection(NULL);
	}

	Server = GetServer();
	Port = GetPort();

	if (Server == NULL || Port == 0) {
		ScheduleReconnect(60);

		return;
	}

	g_Bouncer->LogUser(this, "Trying to reconnect to [%s]:%d for user %s", Server, Port, m_Name);

	i = 0;
	while (hash_t<CUser *> *UserHash = g_Bouncer->GetUsers()->Iterate(i++)) {
		CIRCConnection *IRC;

		IRC = UserHash->Value->GetIRCConnection();

		if (IRC != NULL && IRC->GetState() != State_Connected) {
			IRC->Kill("Timed out.");
		}
	}

	m_LastReconnect = g_CurrentTime;

	const char *BindIp = GetVHost();

	if (BindIp == NULL || BindIp[0] == '\0') {
		BindIp = g_Bouncer->GetDefaultVHost();
	}

	if (BindIp != NULL && BindIp[0] == '\0') {
		BindIp = NULL;
	}

	if (GetIdent() != NULL) {
		g_Bouncer->SetIdent(GetIdent());
	} else {
		g_Bouncer->SetIdent(m_Name);
	}

	CIRCConnection *Connection = new CIRCConnection(Server, Port, this, BindIp, GetSSL(), m_NextProtocolFamily);

	if (AllocFailed(Connection)) {
		return;
	}

	SetIRCConnection(Connection);

	RescheduleReconnectTimer();
}

/**
 * ShouldReconnect
 *
 * Determines whether this user should reconnect (yet).
 */
bool CUser::ShouldReconnect(void) const {
	int Interval = g_Bouncer->GetInterval();

	if (GetServer() == NULL) {
		return false;
	}

	if (Interval == 0) {
		Interval = 25;
	}

	if (m_IRC == NULL && m_ReconnectTime <= g_CurrentTime &&
			(IsAdmin() || g_CurrentTime - m_LastReconnect > 120) &&
			g_CurrentTime - g_LastReconnect > Interval && IsQuitted() == 0) {
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

	UnmarkQuitted();

	MaxDelay = Delay;
	Interval = g_Bouncer->GetInterval();

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

	if (GetServer() != NULL && GetClientConnectionMultiplexer() != NULL) {
		char *Out;
		int rc = asprintf(&Out, "Scheduled reconnect in %d seconds.", (int)(m_ReconnectTime - g_CurrentTime));

		if (!RcFailed(rc)) {
			GetClientConnectionMultiplexer()->Privmsg(Out);

			free(Out);
		}
	}
}

/**
 * GetIRCUptime
 *
 * Returns the number of seconds this user has been connected to an IRC server.
 */
unsigned int CUser::GetIRCUptime(void) const {
	if (m_IRC != NULL) {
		return (unsigned int)(g_CurrentTime - m_LastReconnect);
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
	int rc = vasprintf(&Out, Format, marker);
	va_end(marker);

	if (!RcFailed(rc)) {
		if (GetClientConnectionMultiplexer() == NULL) {
			m_Log->WriteLine("%s", Out);
		} else {
			GetClientConnectionMultiplexer()->Privmsg(Out);
		}

		free(Out);
	}
}

/**
 * Lock
 *
 * Locks (i.e. suspends) the user.
 */
void CUser::Lock(void) {
	CacheSetInteger(m_ConfigCache, lock, 1);
}

/**
 * Unlock
 *
 * Unlocks (i.e. unsuspends) the user.
 */
void CUser::Unlock(void) {
	CacheSetInteger(m_ConfigCache, lock, 0);
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

	if (GetClientConnectionMultiplexer() != NULL && m_IRC != NULL) {
		GetClientConnectionMultiplexer()->SetNick(m_IRC->GetCurrentNick());
	}

	if (m_IRC == NULL) {
		WasNull = true;
	} else {
		WasNull = false;

		if (m_IRC->GetState() == State_Connecting) {
			if (m_NextProtocolFamily == AF_INET) {
				m_NextProtocolFamily = AF_UNSPEC;
			} else {
				m_NextProtocolFamily = AF_INET;
			}
		}

		m_IRC->SetOwner(NULL);
	}

	OldIRC = m_IRC;
	m_IRC = IRC;

	Modules = g_Bouncer->GetModules();

	if (IRC == NULL && !WasNull) {
		for (int i = 0; i < Modules->GetLength(); i++) {
			(*Modules)[i]->ServerDisconnect(GetUsername());
		}

		CClientConnection *Client = GetClientConnectionMultiplexer();;

		if (Client != NULL) {
			CHashtable<CChannel *, false> *Channels;
			int i;
			hash_t<CChannel *> *ChannelHash;

			Channels = OldIRC->GetChannels();

			i = 0;
			while ((ChannelHash = Channels->Iterate(i++)) != NULL) {
				Client->WriteLine(":sbnc.beutner.name KICK %s %s :Disconnected from the IRC server.", ChannelHash->Name, GetNick());
			}
		}

		if (OldIRC->GetSite() != NULL) {
			g_Bouncer->LogUser(this, "User %s [%s!%s] disconnected from the server.",
				GetUsername(), OldIRC->GetCurrentNick(), OldIRC->GetSite());
		} else {
			g_Bouncer->LogUser(this, "User %s disconnected from the server.",
				GetUsername());
		}
	} else if (IRC) {
		for (int i = 0; i < Modules->GetLength(); i++) {
			(*Modules)[i]->ServerConnect(GetUsername());
		}

		m_LastReconnect = g_CurrentTime;

		IRC->SetTrafficStats(m_IRCStats);
	}
}

/**
 * AddClientConnection
 *
 * Adds a new client connection for this user.
 *
 * @param Client the new client
 * @param Silent whether to silently add the new client
 */
void CUser::AddClientConnection(CClientConnection *Client, bool Silent) {
	sockaddr *Remote;
	client_t ClientT;
	int i;
	const CVector<CModule *> *Modules;
	char *Info;
	client_t OldestClient = {};
	time_t ThisTimestamp;

	ThisTimestamp = g_CurrentTime;

	for (i = 0; i < m_Clients.GetLength(); i++) {
		if (m_Clients[i].Creation >= ThisTimestamp) {
			ThisTimestamp = m_Clients[i].Creation + 1;
		}
	}

	if (m_Clients.GetLength() > 0 && m_Clients.GetLength() >= g_Bouncer->GetResourceLimit("clients", this)) {
		OldestClient.Creation = g_CurrentTime + 1;

		for (i = 0; i < m_Clients.GetLength(); i++) {
			if (m_Clients[i].Creation < OldestClient.Creation && m_Clients[i].Client != Client) {
				OldestClient = m_Clients[i];
			}
		}

		assert(OldestClient.Client != NULL);

		OldestClient.Client->Kill("Another client logged in. Your client has been disconnected "
			"because it was the oldest existing client connection.");
	}

	Client->SetOwner(this);

	Remote = Client->GetRemoteAddress();

	if (!Silent) {
		g_Bouncer->Log("User %s logged on (from %s[%s]).", GetUsername(),
			Client->GetPeerName(), (Remote != NULL) ? IpToString(Remote) : "unknown");

		CacheSetInteger(m_ConfigCache, seen, (int)g_CurrentTime);
	}

	ClientT.Creation = g_CurrentTime;
	ClientT.Client = Client;

	if (IsError(m_Clients.Insert(ClientT))) {
		Client->Kill("An error occured while registering the client.");

		return;
	}

	m_PrimaryClient = Client;

	Client->SetTrafficStats(m_ClientStats);

	if (!Silent) {
		Modules = g_Bouncer->GetModules();

		for (i = 0; i < Modules->GetLength(); i++) {
			(*Modules)[i]->AttachClient(Client);
		}

		int rc = asprintf(&Info, "Another client logged in from %s[%s]. The new client "
			"has been set as the primary client for this account.",
			Client->GetPeerName(), (Remote != NULL) ? IpToString(Remote) : "unknown");

		if (RcFailed(rc)) {
			return;
		}

		for (int i = 0; i < m_Clients.GetLength(); i++) {
			if (m_Clients[i].Client != Client) {
				m_Clients[i].Client->Privmsg(Info);
			}
		}

		free(Info);
	}
}

/**
 * RemoveClientConnection
 *
 * Removes a client connection for this user.
 *
 * @param Client the client which is to be removed.
 * @param Silent whether to silently remove the client
 */
void CUser::RemoveClientConnection(CClientConnection *Client, bool Silent) {
	const char *AwayMessage, *DropModes, *AwayNick, *AwayText;
	hash_t<CChannel *> *Channel;
	const CVector<CModule *> *Modules;
	int i;
	int a, rc;
	client_t *BestClient;
	sockaddr *Remote;
	char *InfoPrimary, *Info;
	bool LastClient;

	LastClient = (m_Clients.GetLength() == 1);

	if (!Silent) {
		const char *Plural = "s";

		if (m_Clients.GetLength() - 1 == 1) {
			Plural = "";
		}

		if (Client->GetQuitReason() != NULL) {
			g_Bouncer->Log("User %s logged off. %d remaining client%s for this user. (%s)",
				 GetUsername(), m_Clients.GetLength() - 1, Plural, Client->GetQuitReason());
		} else {
			g_Bouncer->Log("User %s logged off. %d remaining client%s for this user.",
				GetUsername(), m_Clients.GetLength() - 1, Plural);
		}

		CacheSetInteger(m_ConfigCache, seen, (int)g_CurrentTime);
	}

	if (!Silent && m_IRC != NULL && m_Clients.GetLength() == 1) {
		AwayMessage = GetAwayMessage();

		if (AwayMessage != NULL) {
			i = 0;

			while ((Channel = m_IRC->GetChannels()->Iterate(i++)) != NULL) {
				m_IRC->WriteLine("PRIVMSG %s :\001ACTION is now away: %s\001", Channel->Name, AwayMessage);
			}
		}
	}

	for (a = m_Clients.GetLength() - 1; a >= 0 ; a--) {
		if (m_Clients[a].Client == Client) {
			m_Clients.Remove(a);

			break;
		}
	}

	if (!Silent) {
		Modules = g_Bouncer->GetModules();

		for (i = 0; i < Modules->GetLength(); i++) {
			(*Modules)[i]->DetachClient(Client);
		}
	}

	if (m_IRC != NULL && m_Clients.GetLength() == 0) {
		DropModes = CacheGetString(m_ConfigCache, dropmodes);

		if (DropModes != NULL && m_IRC->GetCurrentNick() != NULL && LastClient) {
			m_IRC->WriteLine("MODE %s -%s", m_IRC->GetCurrentNick(), DropModes);
		}

		AwayNick = CacheGetString(m_ConfigCache, awaynick);

		if (AwayNick != NULL) {
			m_IRC->WriteLine("NICK %s", AwayNick);
		}

		AwayText = CacheGetString(m_ConfigCache, away);

		if (AwayText != NULL && LastClient) {
			m_IRC->WriteLine("AWAY :%s", AwayText);
		}
	}

	BestClient = NULL;

	for (a = m_Clients.GetLength() - 1; a >= 0 ; a--) {
		if (BestClient == NULL || m_Clients[a].Creation > BestClient->Creation) {
			BestClient = m_Clients.GetAddressOf(a);
		}
	}

	if (BestClient != NULL) {
		m_PrimaryClient = BestClient->Client;
	} else {
		m_PrimaryClient = NULL;
	}

	Remote = Client->GetRemoteAddress();

	if (!Silent) {
		rc = asprintf(&InfoPrimary, "Another client logged off from %s[%s]. Your client "
			"has been set as the primary client for this account.",
			Client->GetPeerName(), (Remote != NULL) ? IpToString(Remote) : "unknown");

		if (RcFailed(rc)) {
			return;
		}

		rc = asprintf(&Info, "Another client logged off from %s[%s].",
			Client->GetPeerName(), (Remote != NULL) ? IpToString(Remote) : "unknown");

		if (RcFailed(rc)) {
			free(Info);

			return;
		}

		for (int i = 0; i < m_Clients.GetLength(); i++) {
			if (m_Clients[i].Client == m_PrimaryClient) {
				m_Clients[i].Client->Privmsg(InfoPrimary);
			} else {
				m_Clients[i].Client->Privmsg(Info);
			}
		}

		free(Info);
		free(InfoPrimary);
	}
}

/**
 * GetClientConnections
 *
 * Returns the client connections for this user.
 */
CVector<client_t> *CUser::GetClientConnections(void) {
	return &m_Clients;
}

/**
 * SetAdmin
 *
 * Sets whether the user is an admin.
 *
 * @param Admin a boolean flag
 */
void CUser::SetAdmin(bool Admin) {
	CacheSetInteger(m_ConfigCache, admin, Admin ? 1 : 0);

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
	if (CacheGetInteger(m_ConfigCache, admin) != 0) {
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
	if (g_Bouncer->GetMD5()) {
		Password = UtilMd5(Password, GenerateSalt());
	}

	CacheSetString(m_ConfigCache, password, Password);
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
	return CacheGetString(m_ConfigCache, server);
}

/**
 * SetPort
 *
 * Sets the port of the user's IRC server.
 *
 * @param Port the port
 */
void CUser::SetPort(int Port) {
	CacheSetInteger(m_ConfigCache, port, Port);
}

/**
 * GetPort
 *
 * Returns the port of the user's IRC server.
 */
int CUser::GetPort(void) const {
	int Port = CacheGetInteger(m_ConfigCache, port);

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
	if (CacheGetInteger(m_ConfigCache, lock) == 0) {
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
	CacheSetInteger(m_ConfigCache, quitted, RequireManualJump ? 2 : 1);
}

/**
 * UnmarkQuitted
 *
 * Tells the bouncer to automatically try to connect to a server
 * when the user is not already connected to one.
 */
void CUser::UnmarkQuitted(void) {
	CacheSetInteger(m_ConfigCache, quitted, 0);
}

/**
 * IsQuitted
 *
 * Returns whether the user is marked as quitted.
 */
int CUser::IsQuitted(void) const {
	return CacheGetInteger(m_ConfigCache, quitted);
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

	for (int i = 0; i < m_BadLogins.GetLength(); i++) {
		if (CompareAddress(m_BadLogins[i].Address, Peer) == 0 && m_BadLogins[i].Count < 3) {
			m_BadLogins[i].Count++;

			return;
		}
	}

	BadLogin.Count = 1;
	BadLogin.Address = (sockaddr *)malloc(SOCKADDR_LEN(Peer->sa_family));

	if (AllocFailed(BadLogin.Address)) {
		return;
	}

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
	for (int i = 0; i < m_BadLogins.GetLength(); i++) {
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
				free(m_BadLogins[i].Address);
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

	if (((CUser *)User)->GetIRCConnection() != NULL) {
		return false;
	}

	Interval = g_Bouncer->GetInterval();

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
	return CacheGetInteger(m_ConfigCache, seen);
}

/**
 * GetAwayNick
 *
 * Returns the user's away nick.
 */
const char *CUser::GetAwayNick(void) const {
	return CacheGetString(m_ConfigCache, awaynick);
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

	if (m_Clients.GetLength() == 0 && m_IRC != NULL) {
		m_IRC->WriteLine("NICK :%s", Nick);
	}
}

/**
 * GetAwayText
 *
 * Returns the user's away reason.
 */
const char *CUser::GetAwayText(void) const {
	return CacheGetString(m_ConfigCache, away);
}

void CUser::SetAwayText(const char *Reason) {
	USER_SETFUNCTION(away, Reason);

	if (m_Clients.GetLength() == 0 && m_IRC != NULL) {
		m_IRC->WriteLine("AWAY :%s", Reason);
	}
}

/**
 * GetVHost
 *
 * Returns the user's virtual host.
 */
const char *CUser::GetVHost(void) const {
	return CacheGetString(m_ConfigCache, ip);
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
	return CacheGetInteger(m_ConfigCache, delayjoin);
}

/**
 * SetDelayJoin
 *
 * Sets whether "delay join" is enabled for the user.
 *
 * @param DelayJoin a boolean flag
 */
void CUser::SetDelayJoin(int DelayJoin) {
	CacheSetInteger(m_ConfigCache, delayjoin, DelayJoin);
}

/**
 * GetConfigChannels
 *
 * Returns a comma-seperated list of channels the user
 * automatically joins when connected to an IRC server.
 */
const char *CUser::GetConfigChannels(void) const {
	return CacheGetString(m_ConfigCache, channels);
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
	return CacheGetString(m_ConfigCache, suspend);
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
	return CacheGetString(m_ConfigCache, spass);
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
	return CacheGetString(m_ConfigCache, automodes);
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
	return CacheGetString(m_ConfigCache, dropmodes);
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
#ifdef HAVE_LIBSSL
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
#ifdef HAVE_LIBSSL
	X509 *DuplicateCertificate;

	for (int i = 0; i < m_ClientCertificates.GetLength(); i++) {
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
#ifdef HAVE_LIBSSL
	for (int i = 0; i < m_ClientCertificates.GetLength(); i++) {
		if (X509_cmp(m_ClientCertificates[i], Certificate) == 0) {
			X509_free(m_ClientCertificates[i]);

			m_ClientCertificates.Remove(i);

			return PersistCertificates();
		}
	}
#endif

	return false;
}

#ifdef HAVE_LIBSSL
/**
 * PersistCertificates
 *
 * Stores the client certificates in a file.
 */
bool CUser::PersistCertificates(void) {
	char *TempFilename;
	const char *Filename;
	FILE *CertFile;

	int rc = asprintf(&TempFilename, "users/%s.pem", m_Name);

	if (RcFailed(rc)) {
		return false;
	}

	Filename = g_Bouncer->BuildPathConfig(TempFilename);
	free(TempFilename);

	if (AllocFailed(Filename)) {
		return false;
	}

	if (m_ClientCertificates.GetLength() == 0) {
		unlink(Filename);
	} else {
		CertFile = fopen(Filename, "w");

		SetPermissions(Filename, S_IRUSR | S_IWUSR);

		if (AllocFailed(CertFile)) {
			return false;
		}

		for (int i = 0; i < m_ClientCertificates.GetLength(); i++) {
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
#ifdef HAVE_LIBSSL
	for (int i = 0; i < m_ClientCertificates.GetLength(); i++) {
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
#ifdef HAVE_LIBSSL
	if (CacheGetInteger(m_ConfigCache, ssl) != 0) {
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
#ifdef HAVE_LIBSSL
	if (SSL) {
		CacheSetInteger(m_ConfigCache, ssl, 1);
	} else {
		CacheSetInteger(m_ConfigCache, ssl, 0);
	}
#endif
}

/**
 * GetIdent
 *
 * Returns the user's custom ident, or NULL if there is no custom ident.
 */
const char *CUser::GetIdent(void) const {
	return CacheGetString(m_ConfigCache, ident);
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

	int rc = asprintf(&Setting, "tag.%s", Tag);

	if (RcFailed(rc)) {
		return NULL;
	}

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

	int rc = asprintf(&Setting, "tag.%s", Tag);

	if (RcFailed(rc)) {
		return false;
	}

	Modules = g_Bouncer->GetModules();

	for (int i = 0; i < Modules->GetLength(); i++) {
		(*Modules)[i]->UserTagModified(Tag, Value);
	}

	ReturnValue = m_Config->WriteString(Setting, Value);

	free(Setting);

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

	int rc = asprintf(&StringValue, "%d", Value);

	if (RcFailed(rc)) {
		return false;
	}

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
 * SetSystemNotices
 *
 * Sets whether the user should receive system notices.
 *
 * @param SystemNotices a boolean flag
 */
void CUser::SetSystemNotices(bool SystemNotices) {
	CacheSetInteger(m_ConfigCache, ignsysnotices, SystemNotices ? 0 : 1);
}

/**
 * GetSystemNotices
 *
 * Returns whether the user should receive system notices.
 */
bool CUser::GetSystemNotices(void) const {
	if (CacheGetInteger(m_ConfigCache, ignsysnotices) == 0) {
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
	CacheSetString(m_ConfigCache, awaymessage, Text);
}

/**
 * GetAwayMessage
 *
 * Returns the user's away message or NULL if none is set.
 */
const char *CUser::GetAwayMessage(void) const {
	return CacheGetString(m_ConfigCache, awaymessage);
}

/**
 * SetAwayMessage
 *
 * Sets the "lean" mode for a user.
 *
 * @param Mode the mode (0, 1 or 2)
 */
void CUser::SetLeanMode(unsigned int Mode) {
	CacheSetInteger(m_ConfigCache, lean, Mode);
}

/**
 * GetLeanMode
 *
 * Returns the state of the "lean" flag.
 */
unsigned int CUser::GetLeanMode(void) {
	return CacheGetInteger(m_ConfigCache, lean);
}

const char *CUser::SimulateWithResult(const char *Command) {
	static CFakeClient *FakeClient = NULL;

	delete FakeClient;

	FakeClient = new CFakeClient();

	Simulate(Command, FakeClient);

	return FakeClient->GetData();
}

bool GlobalUserReconnectTimer(time_t Now, void *Null) {
	int i = rand() % g_Bouncer->GetUsers()->GetLength();

	while (hash_t<CUser *> *UserHash = g_Bouncer->GetUsers()->Iterate(i++)) {
		if (UserHash->Value->ShouldReconnect() && g_Bouncer->GetStatus() == Status_Running) {
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
		g_ReconnectTimer = new CTimer(20, true, GlobalUserReconnectTimer, NULL);
	}

	ReconnectTime = g_ReconnectTimer->GetNextCall();

	if (g_Bouncer->GetStatus() == Status_Running) {
		while (hash_t<CUser *> *UserHash = g_Bouncer->GetUsers()->Iterate(i++)) {
			if (UserHash->Value->m_ReconnectTime >= g_CurrentTime &&
					UserHash->Value->m_ReconnectTime < ReconnectTime &&
					UserHash->Value->GetIRCConnection() == NULL) {
				ReconnectTime = UserHash->Value->m_ReconnectTime;
			} else if (UserHash->Value->ShouldReconnect()) {
				UserHash->Value->Reconnect();
			}
		}
	}

	g_ReconnectTimer->Reschedule(ReconnectTime);
}

void CUser::SetUseQuitReason(bool Value) {
	CacheSetInteger(m_ConfigCache, quitaway, Value ? 1 : 0);
}

bool CUser::GetUseQuitReason(void) {
	if (CacheGetInteger(m_ConfigCache, quitaway)) {
		return true;
	} else {
		return false;
	}
}

void CUser::SetChannelSortMode(const char *Mode) {
	CacheSetString(m_ConfigCache, channelsort, Mode);
}

const char *CUser::GetChannelSortMode(void) const {
	return CacheGetString(m_ConfigCache, channelsort);
}

void CUser::SetAutoBacklog(const char *Value) {
	CacheSetString(m_ConfigCache, autobacklog, Value);
}

const char *CUser::GetAutoBacklog(void) {
	return CacheGetString(m_ConfigCache, autobacklog);
}
