/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007,2010 Gunnar Beutner                                 *
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

bool DelayJoinTimer(time_t Now, void *IRCConnection);
bool IRCPingTimer(time_t Now, void *IRCConnection);

extern time_t g_LastReconnect;

/**
 * CIRCConnection
 *
 * Constructs a new IRC connection object.
 *
 * @param Host the server's host name
 * @param Port the server's port
 * @param Owner the owner of the IRC connection
 * @param BindIp bind address (or NULL)
 * @param SSL whether to use SSL
 * @param Family socket family (either AF_INET or AF_INET6)
 */
CIRCConnection::CIRCConnection(const char *Host, unsigned int Port, CUser *Owner, const char *BindIp, bool SSL, int Family) : CConnection(Host, Port, BindIp, SSL, Family) {
	const char *Ident;

	SetRole(Role_Client);
	SetOwner(Owner);

	g_LastReconnect = g_CurrentTime;
	m_LastResponse = g_LastReconnect;

	m_State = State_Connecting;

	m_CurrentNick = NULL;
	m_Server = NULL;
	m_ServerVersion = NULL;
	m_ServerFeat = NULL;
	m_Site = NULL;
	m_Usermodes = NULL;
	m_EatPong = false;

	m_QueueHigh = new CQueue();

	if (AllocFailed(m_QueueHigh)) {
		g_Bouncer->Fatal();
	}

	m_QueueMiddle = new CQueue();

	if (AllocFailed(m_QueueMiddle)) {
		g_Bouncer->Fatal();
	}

	m_QueueLow = new CQueue();

	if (AllocFailed(m_QueueLow)) {
		g_Bouncer->Fatal();
	}

	m_FloodControl = new CFloodControl();

	if (AllocFailed(m_FloodControl)) {
		g_Bouncer->Fatal();
	}

	if (Host != NULL) {
		const char *Password = Owner->GetServerPassword();

		if (Password != NULL) {
			WriteLine("PASS :%s", Password);
		}

		WriteLine("NICK %s", Owner->GetNick());

		if (Owner->GetIdent() != NULL) {
			Ident = Owner->GetIdent();
		} else {
			Ident = Owner->GetUsername();
		}

		WriteLine("USER %s \"\" \"fnords\" :%s", Ident, Owner->GetRealname());
	}

	m_Channels = new CHashtable<CChannel *, false>();

	if (AllocFailed(m_Channels)) {
		g_Bouncer->Fatal();
	}

	m_Channels->RegisterValueDestructor(DestroyObject<CChannel>);

	m_ISupport = new CHashtable<char *, false>();

	if (AllocFailed(m_ISupport)) {
		g_Bouncer->Fatal();
	}

	m_ISupport->RegisterValueDestructor(FreeString);

	m_ISupport->Add("CHANMODES", strdup("bIe,k,l"));
	m_ISupport->Add("CHANTYPES", strdup("#&+"));
	m_ISupport->Add("PREFIX", strdup("(ov)@+"));
	m_ISupport->Add("NAMESX", strdup(""));

	m_FloodControl->AttachInputQueue(m_QueueHigh, 0);
	m_FloodControl->AttachInputQueue(m_QueueMiddle, 1);
	m_FloodControl->AttachInputQueue(m_QueueLow, 2);

	m_PingTimer = g_Bouncer->CreateTimer(180, true, IRCPingTimer, this);
	m_DelayJoinTimer = NULL;
	m_NickCatchTimer = NULL;
}

/**
 * ~CIRCConnection
 *
 * Destructs a connection object.
 */
CIRCConnection::~CIRCConnection(void) {
	free(m_CurrentNick);
	free(m_Site);
	free(m_Usermodes);

	delete m_Channels;

	free(m_Server);
	free(m_ServerVersion);
	free(m_ServerFeat);

	delete m_ISupport;

	delete m_QueueLow;
	delete m_QueueMiddle;
	delete m_QueueHigh;
	delete m_FloodControl;

	if (m_DelayJoinTimer != NULL) {
		m_DelayJoinTimer->Destroy();
	}

	if (m_PingTimer != NULL) {
		m_PingTimer->Destroy();
	}

	if (m_NickCatchTimer != NULL) {
		m_NickCatchTimer->Destroy();
	}
}

/**
 * ParseLineArgV
 *
 * Parses and processes a line which was sent by the server.
 *
 * @param argc number of tokens
 * @param argv the tokens
 */
bool CIRCConnection::ParseLineArgV(int argc, const char **argv) {
	CChannel *Channel;
	CClientConnection *Client;

	m_LastResponse = g_CurrentTime;

	if (argc < 2) {
		return true;
	}

	const char *Reply = argv[0];
	const char *Raw = argv[1];
	char *Nick = ::NickFromHostmask(Reply);
	int iRaw = atoi(Raw);

	bool b_Me = false;
	if (m_CurrentNick != NULL && Nick != NULL && strcasecmp(Nick, m_CurrentNick) == 0) {
		b_Me = true;
	}

	free(Nick);

	Client = GetOwner()->GetClientConnectionMultiplexer();

	// HASH values
	CHashCompare hashRaw(argv[1]);
	static CHashCompare hashPrivmsg("PRIVMSG");
	static CHashCompare hashNotice("NOTICE");
	static CHashCompare hashJoin("JOIN");
	static CHashCompare hashPart("PART");
	static CHashCompare hashKick("KICK");
	static CHashCompare hashNick("NICK");
	static CHashCompare hashQuit("QUIT");
	static CHashCompare hashMode("MODE");
	static CHashCompare hashTopic("TOPIC");
	static CHashCompare hashPong("PONG");
	// END of HASH values

	if (argc > 3 && iRaw == 433) {
		bool ReturnValue = ModuleEvent(argc, argv);

		if (ReturnValue) {
			if (GetCurrentNick() == NULL) {
				WriteLine("NICK :%s_", argv[3]);
			}

			if (m_NickCatchTimer == NULL) {
				m_NickCatchTimer = new CTimer(30, false, NickCatchTimer, this);
			}
		}

		return ReturnValue;
	} else if (argc > 3 && hashRaw == hashPrivmsg && Client == NULL) {
		const char *Host;
		const char *Dest = argv[2];
		char *Nick = ::NickFromHostmask(Reply);

		Channel = GetChannel(Dest);

		if (Channel != NULL) {
			CNick *User = Channel->GetNames()->Get(Nick);

			if (User != NULL) {
				User->SetIdleSince(g_CurrentTime);
			}

			Channel->AddBacklogLine(argv[0], argv[3]);
		}

		if (!ModuleEvent(argc, argv)) {
			free(Nick);
			return false;
		}

		/* don't log ctcp requests */
		if (argv[3][0] != '\1' && argv[3][strlen(argv[3]) - 1] != '\1' && Dest != NULL && Nick != NULL && m_CurrentNick != NULL && strcasecmp(Dest, m_CurrentNick) == 0 && strcasecmp(Nick, m_CurrentNick) != 0) {
			char *Dup;
			char *Delim;

			Dup = strdup(Reply);

			if (AllocFailed(Dup)) {
				free(Nick);

				return true;
			}

			Delim = strchr(Dup, '!');

			if (Delim != NULL) {
				*Delim = '\0';

				Host = Delim + 1;
			}

			GetOwner()->Log("%s (%s): %s", Dup, Delim ? Host : "<unknown host>", argv[3]);

			free(Dup);
		}

		free(Nick);

		UpdateHostHelper(Reply);

		return true;
	} else if (argc > 3 && hashRaw == hashPrivmsg && Client != NULL) {
		Channel = GetChannel(argv[2]);

		if (Channel != NULL) {
			Channel->AddBacklogLine(argv[0], argv[3]);
		}
	} else if (argc > 3 && hashRaw == hashNotice && Client == NULL) {
		const char *Dest = argv[2];
		char *Nick;
		
		if (!ModuleEvent(argc, argv)) {
			return false;
		}

		Nick = ::NickFromHostmask(Reply);

		/* don't log ctcp replies */
		if (argv[3][0] != '\1' && argv[3][strlen(argv[3]) - 1] != '\1' && Dest != NULL && Nick != NULL && m_CurrentNick != NULL && strcasecmp(Dest, m_CurrentNick) == 0 && strcasecmp(Nick, m_CurrentNick) != 0) {
			GetOwner()->Log("%s (notice): %s", Reply, argv[3]);
		}

		free(Nick);

		return true;
	} else if (argc > 2 && hashRaw == hashJoin) {
		if (b_Me) {
			AddChannel(argv[2]);

			/* GetOwner() can be NULL if AddChannel failed */
			if (GetOwner() != NULL && Client == NULL) {
				WriteLine("MODE %s", argv[2]);
			}
		}

		Channel = GetChannel(argv[2]);

		if (Channel != NULL) {
			Nick = NickFromHostmask(Reply);

			if (AllocFailed(Nick)) {
				return false;
			}

			Channel->AddUser(Nick, '\0');
			free(Nick);
		}

		UpdateHostHelper(Reply);
	} else if (argc > 2 && hashRaw == hashPart) {
		bool bRet = ModuleEvent(argc, argv);

		if (b_Me) {
			RemoveChannel(argv[2]);
		} else {
			Channel = GetChannel(argv[2]);

			if (Channel != NULL) {
				Nick = ::NickFromHostmask(Reply);

				if (AllocFailed(Nick)) {
					return false;
				}

				Channel->RemoveUser(Nick);

				free(Nick);
			}
		}

		UpdateHostHelper(Reply);

		return bRet;
	} else if (argc > 3 && hashRaw == hashKick) {
		bool bRet = ModuleEvent(argc, argv);

		if (m_CurrentNick != NULL && strcasecmp(argv[3], m_CurrentNick) == 0) {
			RemoveChannel(argv[2]);

			if (Client == NULL) {
				char *Dup = strdup(Reply);

				if (AllocFailed(Dup)) {
					return bRet;
				}

				char *Delim = strchr(Dup, '!');
				const char *Host = NULL;

				if (Delim) {
					*Delim = '\0';

					Host = Delim + 1;
				}

				GetOwner()->Log("%s (%s) kicked you from %s (%s)", Dup, Delim ? Host : "<unknown host>", argv[2], argc > 4 ? argv[4] : "");

				free(Dup);
			}
		} else {
			Channel = GetChannel(argv[2]);

			if (Channel != NULL) {
				Channel->RemoveUser(argv[3]);
			}
		}

		UpdateHostHelper(Reply);

		return bRet;
	} else if (argc > 2 && iRaw == 1) {
		if (Client != NULL) {
			if (strcmp(Client->GetNick(), argv[2]) != 0) {
				Client->WriteLine(":%s!%s NICK :%s", Client->GetNick(), m_Site ? m_Site : "unknown@unknown.host", argv[2]);
			}
		}

		free(m_CurrentNick);
		m_CurrentNick = strdup(argv[2]);

		free(m_Server);
		m_Server = strdup(Reply);
	} else if (argc > 2 && hashRaw == hashNick) {
		if (b_Me) {
			free(m_CurrentNick);
			m_CurrentNick = strdup(argv[2]);
		}

		Nick = NickFromHostmask(argv[0]);

		int i = 0;

		if (!b_Me && GetOwner()->GetClientConnectionMultiplexer() == NULL) {
			const char *AwayNick = GetOwner()->GetAwayNick();

			if (AwayNick != NULL && strcasecmp(AwayNick, Nick) == 0) {
				WriteLine("NICK %s", AwayNick);
			}
		}

		while (hash_t<CChannel *> *ChannelHash = m_Channels->Iterate(i++)) {
			ChannelHash->Value->RenameUser(Nick, argv[2]);
		}

		free(Nick);
	} else if (argc > 1 && hashRaw == hashQuit) {
		bool bRet = ModuleEvent(argc, argv);

		Nick = NickFromHostmask(argv[0]);

		int i = 0;

		while (hash_t<CChannel *> *ChannelHash = m_Channels->Iterate(i++)) {
			ChannelHash->Value->RemoveUser(Nick);
		}

		free(Nick);

		return bRet;
	} else if (argc > 1 && (iRaw == 422 || iRaw == 376)) {
		int DelayJoin = GetOwner()->GetDelayJoin();
		if (m_State != State_Connected) {
			const CVector<CModule *> *Modules = g_Bouncer->GetModules();

			for (int i = 0; i < Modules->GetLength(); i++) {
				(*Modules)[i]->ServerLogon(GetOwner()->GetUsername());
			}

			const char *ClientNick;

			if (Client != NULL) {
				ClientNick = Client->GetNick();

				if (strcmp(m_CurrentNick, ClientNick) != 0) {
					Client->ChangeNick(m_CurrentNick);
				}
			}

			GetOwner()->Log("You were successfully connected to an IRC server.");
			g_Bouncer->Log("User %s connected to an IRC server.",
				GetOwner()->GetUsername());
		}

		if (DelayJoin == 1) {
			m_DelayJoinTimer = g_Bouncer->CreateTimer(5, false, DelayJoinTimer, this);
		} else if (DelayJoin == 0) {
			JoinChannels();
		}

		if (Client == NULL) {
			bool AppendTS = (GetOwner()->GetConfig()->ReadInteger("user.ts") != 0);
			const char *AwayReason = GetOwner()->GetAwayText();

			if (AwayReason != NULL) {
				WriteLine(AppendTS ? "AWAY :%s (Away since the dawn of time)" : "AWAY :%s", AwayReason);
			}
		}

		const char *AutoModes = GetOwner()->GetAutoModes();
		const char *DropModes = GetOwner()->GetDropModes();

		if (AutoModes != NULL) {
			WriteLine("MODE %s +%s", GetCurrentNick(), AutoModes);
		}

		if (DropModes != NULL && Client == NULL) {
			WriteLine("MODE %s -%s", GetCurrentNick(), DropModes);
		}

		m_State = State_Connected;
	} else if (argc > 1 && strcasecmp(Reply, "ERROR") == 0) {
		if (strstr(Raw, "throttle") != NULL) {
			GetOwner()->ScheduleReconnect(120);
		} else {
			GetOwner()->ScheduleReconnect(5);
		}

		if (GetCurrentNick() != NULL && GetSite() != NULL) {
			g_Bouncer->LogUser(GetUser(), "Error received for user %s [%s!%s]: %s",
				GetOwner()->GetUsername(), GetCurrentNick(), GetSite(), argv[1]);
		} else {
			g_Bouncer->LogUser(GetUser(), "Error received for user %s: %s",
				GetOwner()->GetUsername(), argv[1]);
		}
	} else if (argc > 3 && iRaw == 465) {
		if (GetCurrentNick() != NULL && GetSite() != NULL) {
			g_Bouncer->LogUser(GetUser(), "G/K-line reason for user %s [%s!%s]: %s",
				GetOwner()->GetUsername(), GetCurrentNick(), GetSite(), argv[3]);
		} else {
			g_Bouncer->LogUser(GetUser(), "G/K-line reason for user %s: %s",
				GetOwner()->GetUsername(), argv[3]);
		}
	} else if (argc > 5 && iRaw == 351) {
		free(m_ServerVersion);
		m_ServerVersion = strdup(argv[3]);

		free(m_ServerFeat);
		m_ServerFeat = strdup(argv[5]);
	} else if (argc > 3 && iRaw	== 5) {
		for (int i = 3; i < argc - 1; i++) {
			char *Dup = strdup(argv[i]);

			if (AllocFailed(Dup)) {
				return false;
			}

			char *Eq = strchr(Dup, '=');

			if (strcasecmp(Dup, "NAMESX") == 0) {
				WriteLine("PROTOCTL NAMESX");
			}

			char *Value;

			if (Eq) {
				*Eq = '\0';

				Value = strdup(++Eq);
			} else {
				Value = strdup("");
			}

			m_ISupport->Add(Dup, Value);

			free(Dup);
		}
	} else if (argc > 4 && iRaw == 324) {
		Channel = GetChannel(argv[3]);

		if (Channel != NULL) {
			Channel->ClearModes();
			Channel->ParseModeChange(argv[0], argv[4], argc - 5, &argv[5]);
			Channel->SetModesValid(true);
		}
	} else if (argc > 3 && hashRaw == hashMode) {
		Channel = GetChannel(argv[2]);

		if (Channel != NULL) {
			Channel->ParseModeChange(argv[0], argv[3], argc - 4, &argv[4]);
		} else if (strcmp(m_CurrentNick, argv[2]) == 0) {
			bool Flip = true, WasNull;
			const char *Modes = argv[3];
			size_t Length = strlen(Modes) + 1;

			if (m_Usermodes != NULL) {
				Length += strlen(m_Usermodes);
			}

			WasNull = (m_Usermodes != NULL) ? false : true;
			m_Usermodes = (char *)realloc(m_Usermodes, Length);

			if (AllocFailed(m_Usermodes)) {
				return false;
			}

			if (WasNull) {
				m_Usermodes[0] = '\0';
			}

			while (*Modes != '\0') {
				if (*Modes == '+') {
					Flip = true;
				} else if (*Modes == '-') {
					Flip = false;
				} else {
					if (Flip) {
						size_t Position = strlen(m_Usermodes);
						m_Usermodes[Position] = *Modes;
						m_Usermodes[Position + 1] = '\0';
					} else {
						char *CurrentModes = m_Usermodes;
						size_t a = 0;

						while (*CurrentModes != '\0') {
							*CurrentModes = m_Usermodes[a];

							if (*CurrentModes != *Modes) {
								CurrentModes++;
							}

							a++;
						}
					}
				}

				Modes++;
			}
		}

		UpdateHostHelper(Reply);
	} else if (argc > 4 && iRaw == 329) {
		Channel = GetChannel(argv[3]);

		if (Channel != NULL) {
			Channel->SetCreationTime(atoi(argv[4]));
		}
	} else if (argc > 4 && iRaw == 332) {
		Channel = GetChannel(argv[3]);

		if (Channel != NULL) {
			Channel->SetTopic(argv[4]);
		}
	} else if (argc > 5 && iRaw == 333) {
		Channel = GetChannel(argv[3]);

		if (Channel != NULL) {
			Channel->SetTopicNick(argv[4]);
			Channel->SetTopicStamp(atoi(argv[5]));
		}
	} else if (argc > 3 && iRaw == 331) {
		Channel = GetChannel(argv[3]);

		if (Channel != NULL) {
			Channel->SetNoTopic();
		}
	} else if (argc > 3 && hashRaw == hashTopic) {
		Channel = GetChannel(argv[2]);

		if (Channel != NULL) {
			Channel->SetTopic(argv[3]);
			Channel->SetTopicStamp(g_CurrentTime);
			Channel->SetTopicNick(argv[0]);
		}

		UpdateHostHelper(Reply);
	} else if (argc > 5 && iRaw == 353) {
		Channel = GetChannel(argv[4]);

		if (Channel != NULL) {
			const char *nicks;
			const char **nickv;

			nicks = ArgTokenize(argv[5]);

			if (AllocFailed(nicks)) {
				return false;
			}

			nickv = ArgToArray(nicks);

			if (AllocFailed(nickv)) {
				ArgFree(nicks);

				return false;
			}

			int nickc = ArgCount(nicks);

			for (int i = 0; i < nickc; i++) {
				char *Nick = strdup(nickv[i]);
				char *BaseNick = Nick;

				if (AllocFailed(Nick)) {
					ArgFree(nicks);

					return false;
				}

				StrTrim(Nick);

				while (IsNickPrefix(*Nick)) {
					Nick++;
				}

				char *Modes = NULL;

				if (BaseNick != Nick) {
					Modes = (char *)malloc(Nick - BaseNick + 1);

					if (!AllocFailed(Modes)) {
						strmcpy(Modes, BaseNick, Nick - BaseNick + 1);
					}
				}

				Channel->AddUser(Nick, Modes);

				free(BaseNick);
				free(Modes);
			}

			ArgFreeArray(nickv);
			ArgFree(nicks);
		}
	} else if (argc > 3 && iRaw == 366) {
		Channel = GetChannel(argv[3]);

		if (Channel != NULL) {
			Channel->SetHasNames();
		}
	} else if (argc > 9 && iRaw == 352) {
		const char *Ident = argv[4];
		const char *Host = argv[5];
		const char *Server = argv[6];
		const char *Nick = argv[7];
		const char *Realname = argv[9];
		char *Mask;

		int rc = asprintf(&Mask, "%s!%s@%s", Nick, Ident, Host);

		if (!RcFailed(rc)) {
			UpdateHostHelper(Mask);
			UpdateWhoHelper(Nick, Realname, Server);

			free(Mask);
		}
	} else if (argc > 6 && iRaw == 367) {
		Channel = GetChannel(argv[3]);

		if (Channel != NULL) {
			Channel->GetBanlist()->SetBan(argv[4], argv[5], atoi(argv[6]));
		}
	} else if (argc > 3 && iRaw == 368) {
		Channel = GetChannel(argv[3]);

		if (Channel != NULL) {
			Channel->SetHasBans();
		}
	} else if (argc > 3 && iRaw == 396) {
		free(m_Site);
		m_Site = strdup(argv[3]);

		if (AllocFailed(m_Site)) {}
	} else if (argc > 3 && hashRaw == hashPong && m_Server != NULL && strcasecmp(argv[3], m_Server) == 0 && m_EatPong) {
		m_EatPong = false;

		return false;
	}

	if (GetOwner() != NULL) {
		return ModuleEvent(argc, argv);
	} else {
		return true;
	}
}

/**
 * ModuleEvent
 *
 * Lets the currently loaded modules process an IRC line.
 *
 * @param argc number of tokens
 * @param argv the tokens
 */
bool CIRCConnection::ModuleEvent(int argc, const char **argv) {
	const CVector<CModule *> *Modules = g_Bouncer->GetModules();

	for (int i = 0; i < Modules->GetLength(); i++) {
		if (!(*Modules)[i]->InterceptIRCMessage(this, argc, argv)) {
			return false;
		}
	}

	return true;
}

/**
 * ParseLine
 *
 * Tokenizes, parses and processes a line which was sent by the IRC server.
 *
 * @param Line the line
 */
void CIRCConnection::ParseLine(const char *Line) {
	const char *RealLine = Line;
	char *Out;

	if (GetOwner() == NULL) {
		return;
	}

	if (Line[0] == ':') {
		RealLine = Line + 1;
	} else {
		RealLine = Line;
	}

	tokendata_t Args = ArgTokenize2(RealLine);
	const char **argv = ArgToArray2(Args);
	int argc = ArgCount2(Args);

	if (argv == NULL) {
		g_Bouncer->Log("ArgToArray2 returned NULL. Could not parse line (%s).", Line);

		return;
	}

	if (ParseLineArgV(argc, argv)) {
		if (strcasecmp(argv[0], "ping") == 0 && argc > 1) {
			int rc = asprintf(&Out, "PONG :%s", argv[1]);

			if (!RcFailed(rc)) {
				m_QueueHigh->QueueItem(Out);
				free(Out);
			}

			if (m_State != State_Connected) {
				m_State = State_Pong;

				if (GetOwner()->GetClientConnectionMultiplexer() == NULL) {
					WriteUnformattedLine("VERSION");
				}
			}
		} else {
			CUser *User = GetOwner();

			if (User) {
				CClientConnection *Client = User->GetClientConnectionMultiplexer();

				if (Client != NULL) {
					if (argc > 2 && strcasecmp(argv[1], "303") == 0) {
						Client->WriteLine("%s -sBNC", Line);
					} else {
						Client->WriteUnformattedLine(Line);
					}
				}
			}
		}
	}

#ifdef _WIN32
	OutputDebugString(Line);
	OutputDebugString("\n");
#endif

	//puts(Line);

	ArgFreeArray(argv);
}

/**
 * GetCurrentNick
 *
 * Returns the current nick of the IRC user.
 */
const char *CIRCConnection::GetCurrentNick(void) const {
	return m_CurrentNick;
}

/**
 * AddChannel
 *
 * Adds a channel for this IRC connection. This does not actually
 * send a JOIN command to the IRC server.
 *
 * @param Channel the channel's name
 */
CChannel *CIRCConnection::AddChannel(const char *Channel) {
	CChannel *ChannelObj;
	bool LimitExceeded = false;

	if (g_Bouncer->GetResourceLimit("channels") < m_Channels->GetLength()) {
		LimitExceeded = true;
		ChannelObj = NULL;
	} else {
		ChannelObj = new CChannel(Channel, this);
	}

	if (AllocFailed(ChannelObj)) {
		GetUser()->Log("Out of memory. Removing channel (%s).", Channel);
		WriteLine("PART %s", Channel);

		return NULL;
	}

	m_Channels->Add(Channel, ChannelObj);

	UpdateChannelConfig();

	return ChannelObj;
}

/**
 * RemoveChannel
 *
 * Removes a channel.
 *
 * @param Channel the channel's name
 */
void CIRCConnection::RemoveChannel(const char *Channel) {
	m_Channels->Remove(Channel);

	UpdateChannelConfig();
}

/**
 * GetServer
 *
 * Returns the server's name as reported by the server.
 */
const char *CIRCConnection::GetServer(void) const {
	return m_Server;
}

/**
 * UpdateChannelConfig
 *
 * Updates the list of channels in the user's config file.
 */
void CIRCConnection::UpdateChannelConfig(void) {
	size_t Size;
	char *Out = NULL;

	int a = 0;

	while (hash_t<CChannel *> *Chan = m_Channels->Iterate(a++)) {
		bool WasNull = (Out == NULL);

		Size = (Out ? strlen(Out) : 0) + strlen(Chan->Name) + 2;
		Out = (char *)realloc(Out, Size);

		if (AllocFailed(Out)) {
			return;
		}

		if (!WasNull) {
			strmcat(Out, ",", Size);
		} else {
			Out[0] = '\0';
		}

		strmcat(Out, Chan->Name, Size);
	}

	/* m_Owner can be NULL if the last channel was not created successfully */
	if (GetOwner() != NULL) {
		GetOwner()->SetConfigChannels(Out);
	}

	free(Out);
}

/**
 * HasQueuedData
 *
 * Checks whether there is data which can be sent to the IRC server.
 */
bool CIRCConnection::HasQueuedData(void) const {
	if (m_FloodControl->GetQueueSize() > 0) {
		return true;
	} else {
		return CConnection::HasQueuedData();
	}
}

/**
 * Write
 *
 * Writes data for the socket.
 */
int CIRCConnection::Write(void) {
	char *Line = m_FloodControl->DequeueItem();

	if (Line != NULL) {
		CConnection::WriteUnformattedLine(Line);
	}

	int ReturnValue = CConnection::Write();

	free(Line);

	return ReturnValue;
}

/**
 * GetISupport
 *
 * Returns a feature's value.
 *
 * @param Feature the name of the feature
 */
const char *CIRCConnection::GetISupport(const char *Feature) const {
	return m_ISupport->Get(Feature);
}

/**
 * SetISupport
 *
 * Sets a feature's value.
 *
 * @param Feature name of the feature
 * @param Value new value for the feature
 */
void CIRCConnection::SetISupport(const char *Feature, const char *Value) {
	m_ISupport->Add(Feature, strdup(Value));
}

/**
 * IsChanMode
 *
 * Checks whether something is a valid channel mode.
 *
 * @param Mode the mode character
 */
bool CIRCConnection::IsChanMode(char Mode) const {
	const char *Modes = GetISupport("CHANMODES");

	return strchr(Modes, Mode) != NULL;
}

/**
 * RequiresParameter
 *
 * Checks whether a channel mode requires a parameter.
 *
 * @param Mode the channel mode
 */
int CIRCConnection::RequiresParameter(char Mode) const {
	int ReturnValue = 3;
	const char *Modes = GetISupport("CHANMODES");
	size_t Len = strlen(Modes);

	for (size_t i = 0; i < Len; i++) {
		if (Modes[i] == Mode) {
			return ReturnValue;
		} else if (Modes[i] == ',') {
			ReturnValue--;
		}

		if (ReturnValue == 0) {
			return 0;
		}
	}

	return ReturnValue;
}

/**
 * GetChannel
 *
 * Returns the channel object for the specified channel name.
 *
 * @param Name the name of the channel
 */
CChannel *CIRCConnection::GetChannel(const char *Name) {
	return m_Channels->Get(Name);
}

/**
 * IsNickPrefix
 *
 * Checks whether something is a valid nick prefix.
 *
 * @param Char the nick prefix
 */
bool CIRCConnection::IsNickPrefix(char Char) const {
	const char *Prefixes = GetISupport("PREFIX");
	bool flip = false;

	if (Prefixes == NULL) {
		return false;
	}

	for (size_t i = 0; i < strlen(Prefixes); i++) {
		if (flip) {
			if (Prefixes[i] == Char) {
				return true;
			}
		} else if (Prefixes[i] == ')') {
			flip = true;
		}
	}

	return false;
}

/**
 * IsNickMode
 *
 * Checks whether something is a channel mode which can be applied
 * to nicks.
 *
 * @param Char the channelmode
 */
bool CIRCConnection::IsNickMode(char Char) const {
	const char *Prefixes = GetISupport("PREFIX");

	while (*Prefixes != '\0' && *Prefixes != ')') {
		if (*Prefixes == Char && *Prefixes != '(') {
			return true;
		} else {
			Prefixes++;
		}
	}

	return false;
}

/**
 * PrefixForChanMode
 *
 * Returns the prefix character for a mode.
 *
 * @param Mode the mode character
 */
char CIRCConnection::PrefixForChanMode(char Mode) const {
	const char *Prefixes = GetISupport("PREFIX");
	const char *ActualPrefixes = strchr(Prefixes, ')');

	Prefixes++;

	if (ActualPrefixes != NULL) {
		ActualPrefixes++;
	} else {
		return '\0';
	}

	while (*ActualPrefixes != '\0') {
		if (*Prefixes == Mode) {
			return *ActualPrefixes;
		}

		Prefixes++;
		ActualPrefixes++;
	}

	return '\0';
}

/**
 * GetServerVersion
 *
 * Returns the server's version (as reported by the server).
 */
const char *CIRCConnection::GetServerVersion(void) const {
	return m_ServerVersion;
}

/**
 * GetServerFeat
 *
 * Returns the server's features (as reported by the server).
 */
const char *CIRCConnection::GetServerFeat(void) const {
	return m_ServerFeat;
}

/**
 * GetISupportAll
 *
 * Returns a hashtable object which contains all 005 replies.
 */
const CHashtable<char *, false> *CIRCConnection::GetISupportAll(void) const {
	return m_ISupport;
}

/**
 * UpdateWhoHelper
 *
 * Updates the realname/servername for a nick.
 *
 * @param Nick the nick
 * @param Realname the realname fot the user
 * @param Server the servername for the user
 */
void CIRCConnection::UpdateWhoHelper(const char *Nick, const char *Realname, const char *Server) {
	int a = 0;

	if (GetOwner()->GetLeanMode() > 0) {
		return;
	}

	while (hash_t<CChannel *> *Chan = m_Channels->Iterate(a++)) {
		if (!Chan->Value->HasNames()) {
			return;
		}

		CNick *NickObj = Chan->Value->GetNames()->Get(Nick);

		if (NickObj) {
			NickObj->SetRealname(Realname);
			NickObj->SetServer(Server);
		}
	}
}

/**
 * UpdateHostHelper
 *
 * Updates the host for a user.
 *
 * @param Host the host (nick!ident\@host)
 */
void CIRCConnection::UpdateHostHelper(const char *Host) {
	const char *NickEnd;
	size_t Offset;
	char *Copy;

	if (GetOwner() != NULL && GetOwner()->GetLeanMode() > 0 && m_Site != NULL) {
		return;
	}

	NickEnd = strchr(Host, '!');

	if (NickEnd == NULL) {
		return;
	}

	Offset = NickEnd - Host;

	Copy = strdup(Host);

	if (AllocFailed(Copy)) {
		return;
	}

	const char *Nick = Copy;
	char *Site = Copy + Offset;

	*Site = '\0';
	Site++;

	if (m_CurrentNick && strcasecmp(Nick, m_CurrentNick) == 0) {
		free(m_Site);
		m_Site = strdup(Site);

		if (AllocFailed(m_Site)) {}
	}

	if (GetOwner()->GetLeanMode() > 0) {
		free(Copy);

		return;
	}

	int i = 0;

	while (hash_t<CChannel *> *Chan = m_Channels->Iterate(i++)) {
		if (!Chan->Value || !Chan->Value->HasNames()) {
			continue;
		}

		CNick *NickObj = Chan->Value->GetNames()->Get(Nick);

		if (NickObj && NickObj->GetSite() == NULL) {
			NickObj->SetSite(Site);
		}
	}

	free(Copy);
}

/**
 * GetFloodControl
 *
 * Returns the flood control object for the IRC connection.
 */
CFloodControl *CIRCConnection::GetFloodControl(void) {
	return m_FloodControl;
}

/**
 * WriteUnformattedLine
 *
 * Sends a line to the IRC server.
 *
 * @param In the line
 */
void CIRCConnection::WriteUnformattedLine(const char *In) {
	if (!m_Locked && strlen(In) < 512) {
		m_QueueMiddle->QueueItem(In);
	}
}

/**
 * GetQueueHigh
 *
 * Returns the high priority queue for the connection.
 */
CQueue *CIRCConnection::GetQueueHigh(void) {
	return m_QueueHigh;
}

/**
 * GetQueueMiddle
 *
 * Returns the medium priority queue for the connection.
 */
CQueue *CIRCConnection::GetQueueMiddle(void) {
	return m_QueueMiddle;
}

/**
 * GetQueueLow
 *
 * Returns the low priority queue for the connection.
 */
CQueue *CIRCConnection::GetQueueLow(void) {
	return m_QueueLow;
}

/**
 * JoinChannels
 *
 * Joins the channels which the user should be in (according to
 * the configuration file).
 */
void CIRCConnection::JoinChannels(void) {
	size_t Size;
	const char *Channels;

	if (m_DelayJoinTimer) {
		m_DelayJoinTimer->Destroy();
		m_DelayJoinTimer = NULL;
	}

	Channels = GetOwner()->GetConfigChannels();

	if (Channels != NULL && Channels[0] != '\0') {
		char *DupChannels, *newChanList, *Channel, *ChanList = NULL;
		CKeyring *Keyring;

		DupChannels = strdup(Channels);

		if (AllocFailed(DupChannels)) {
			return;
		}

		Channel = strtok(DupChannels, ",");

		Keyring = GetOwner()->GetKeyring();

		while (Channel != NULL && Channel[0] != '\0') {
			const char *Key = Keyring->GetKey(Channel);

			if (Key != NULL) {
				WriteLine("JOIN %s %s", Channel, Key);
			} else {
				if (ChanList == NULL || strlen(ChanList) > 400) {
					if (ChanList != NULL) {
						WriteLine("JOIN %s", ChanList);
						free(ChanList);
					}

					Size = strlen(Channel) + 1;
					ChanList = (char *)malloc(Size);

					if (AllocFailed(ChanList)) {
						free(DupChannels);

						return;
					}

					strmcpy(ChanList, Channel, Size);
				} else {
					Size = strlen(ChanList) + 1 + strlen(Channel) + 2;
					newChanList = (char *)realloc(ChanList, Size);

					if (AllocFailed(newChanList)) {
						continue;
					}

					ChanList = newChanList;

					strmcat(ChanList, ",", Size);
					strmcat(ChanList, Channel, Size);
				}
			}

			Channel = strtok(NULL, ",");
		}

		if (ChanList != NULL) {
			WriteLine("JOIN %s", ChanList);
			free(ChanList);
		}

		free(DupChannels);
	}
}

/**
 * GetClassName
 *
 * Returns the classname.
 */
const char *CIRCConnection::GetClassName(void) const {
	return "CIRCConnection";
}

int CIRCConnection::Read(void) {
	int ReturnValue = CConnection::Read();

	if (ReturnValue == 0 && GetRecvqSize() > 5120) {
		Kill("RecvQ exceeded.");
	}

	return ReturnValue;
}

/**
 * DelayJoinTimer
 *
 * Timer function which is used for delayed joining of channels.
 *
 * @param Now the current time
 * @param IRCConnection an CIRCConnection object
 */
bool DelayJoinTimer(time_t Now, void *IRCConnection) {
	((CIRCConnection*)IRCConnection)->m_DelayJoinTimer = NULL;
	((CIRCConnection*)IRCConnection)->JoinChannels();

	return false;
}

/**
 * IRCPingTimer
 *
 * Checks the IRC connection for timeouts.
 *
 * @param Now the current time
 * @param IRCConnection the CIRCConnection object
 */
bool IRCPingTimer(time_t Now, void *IRCConnection) {
	CIRCConnection *IRC = (CIRCConnection *)IRCConnection;

	if (IRC->GetSocket() == INVALID_SOCKET) {
		return true;
	}

	if (g_CurrentTime - IRC->m_LastResponse > 300) {
		const char *ServerName;
		
		ServerName = IRC->GetServer();

		if (ServerName == NULL) {
			ServerName = "sbnc";
		}

		IRC->WriteLine("PING :%s", ServerName);
		IRC->m_EatPong = true;

		/* we're using "Now" here, because that's the time when the
		 * check should've returned a PONG, this might be useful when
		 * we're in a time warp */
		if (Now - IRC->m_LastResponse > 600) {
			IRC->Kill("Server does not respond.");
		}
	}

	return true;
}

/**
 * GetChannels
 *
 * Returns a hashtable containing all channels which the user is currently in.
 */
CHashtable<CChannel *, false> *CIRCConnection::GetChannels(void) {
	return m_Channels;
}

/**
 * GetSite
 *
 * Returns the site (ident\@host) for the IRC connection.
 */
const char *CIRCConnection::GetSite(void) {
	return m_Site;
}

/**
 * SSLVerify
 *
 * Verifies that the IRC server's SSL certificate is valid.
 *
 * @param PreVerifyOk whether the pre-verification succeeded
 * @param Context the X509 context
 */
int CIRCConnection::SSLVerify(int PreVerifyOk, X509_STORE_CTX *Context) const {
#ifdef USESSL
	if (GetOwner()->GetClientConnectionMultiplexer() != NULL) {
		GetOwner()->GetClientConnectionMultiplexer()->Privmsg(Context->cert->name);
	}
#endif

	return 1;
}

/**
 * AsyncDnsFinished
 *
 * Called when the DNS query for the IRC server host is finished.
 *
 * @param Response the response from the DNS server
 */
void CIRCConnection::AsyncDnsFinished(hostent *Response) {
	if (Response == NULL && GetOwner() != NULL) {
		g_Bouncer->LogUser(GetOwner(), "DNS request for user %s failed.", GetOwner()->GetUsername());
	}

	CConnection::AsyncDnsFinished(Response);
}

/**
 * AsyncBindIpDnsFinished
 *
 * Called when the DNS query for the bind address is finished.
 *
 * @param Response the reponse from the DNS server
 */
void CIRCConnection::AsyncBindIpDnsFinished(hostent *Response) {
	if (Response == NULL && GetOwner() != NULL) {
		g_Bouncer->LogUser(GetOwner(), "DNS request (vhost) for user %s failed.", GetOwner()->GetUsername());
	}

	CConnection::AsyncBindIpDnsFinished(Response);
}

/**
 * Destroy
 *
 * Destroys the IRC connection object.
 */
void CIRCConnection::Destroy(void) {
	if (GetOwner() != NULL) {
		GetOwner()->SetIRCConnection(NULL);
	}

	delete this;
}

/**
 * Kill
 *
 * Disconnects from the IRC server and destroys the connection object.
 *
 * @param Error error message
 */
void CIRCConnection::Kill(const char *Error) {
	if (GetOwner() != NULL) {
		GetOwner()->SetIRCConnection(NULL);
	}

	m_FloodControl->Clear();
	m_FloodControl->Disable();
	
	WriteLine("QUIT :%s", Error);

	CConnection::Kill(Error);
}

/**
 * GetHighestUserFlag
 *
 * Returns the "highest" user prefix.
 *
 * @param Modes the prefixes (e.g. @+)
 */
/* TODO: check comment, does it actually make sense (is it really @, +?) */
char CIRCConnection::GetHighestUserFlag(const char *Modes) const {
	bool Flip = false;
	const char *Prefixes = GetISupport("PREFIX");

	if (Modes == NULL || Prefixes == NULL) {
		return '\0';
	}

	for (size_t i = 0; i < strlen(Prefixes); i++) {
		if (Flip == false) {
			if (Prefixes[i] == ')') {
				Flip = true;
			}

			continue;
		}

		if (strchr(Modes, Prefixes[i]) != NULL) {
			return Prefixes[i];
		}
	}

	return '\0';
}

/**
 * Error
 *
 * Called when an error occurred for the connection.
 *
 * @param ErrorCode error code
 */
void CIRCConnection::Error(int ErrorValue) {
	char *ErrorMsg = NULL;

	if (ErrorValue != -1) {
#ifdef _WIN32
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, ErrorValue, 0, (char *)&ErrorMsg, 0, NULL);
#else
		if (ErrorValue != 0) {
			ErrorMsg = strerror(ErrorValue);
		}
#endif
	}

	if (GetOwner() != NULL) {
		if (m_State == State_Connecting) {
			GetOwner()->SetNetworkUnreachable(true);
		}

		if (ErrorMsg == NULL || ErrorMsg[0] == '\0') {
			if (GetCurrentNick() != NULL && GetSite() != NULL) {
				g_Bouncer->LogUser(GetOwner(), "User '%s' [%s!%s] was disconnected "
					"from the IRC server.", GetOwner()->GetUsername(),
					GetCurrentNick(), GetSite());
			} else {
				g_Bouncer->LogUser(GetOwner(), "User '%s' was disconnected "
					"from the IRC server.", GetOwner()->GetUsername());
			}
		} else {
			if (GetCurrentNick() != NULL && GetSite() != NULL) {
				g_Bouncer->LogUser(GetOwner(), "User '%s' [%s!%s] was disconnected "
					"from the IRC server: %s", GetOwner()->GetUsername(),
					GetCurrentNick(), GetSite(), ErrorMsg);
			} else {
				g_Bouncer->LogUser(GetOwner(), "User '%s' was disconnected "
					"from the IRC server: %s", GetOwner()->GetUsername(),
					ErrorMsg);
			}
		}
	}

#ifdef _WIN32
	if (ErrorMsg != NULL) {
		LocalFree(ErrorMsg);
	}
#endif
}

/**
 * GetUsermodes
 *
 * Returns the usermodes for the IRC connection.
 */
const char *CIRCConnection::GetUsermodes(void) {
	if (m_Usermodes != NULL && strlen(m_Usermodes) > 0) {
		return m_Usermodes;
	} else {
		return NULL;
	}
}

/**
 * NickCatchTimer
 *
 * Used to regain a user's nickname
 *
 * @param Now current time
 * @param IRCConnection irc connection
 */
bool NickCatchTimer(time_t Now, void *IRCConnection) {
	CIRCConnection *IRC = (CIRCConnection *)IRCConnection;
	CUser *Owner = IRC->GetOwner();
	const char *AwayNick;

	if (Owner != NULL) {
		AwayNick = IRC->GetOwner()->GetAwayNick();
	} else {
		AwayNick = NULL;
	}

	if (Owner && Owner->GetClientConnectionMultiplexer() != NULL) {
		IRC->m_NickCatchTimer = NULL;

		return false;
	}

	if (IRC->GetCurrentNick() != NULL && AwayNick != NULL && strcmp(IRC->GetCurrentNick(), AwayNick) != 0) {
		IRC->WriteLine("NICK %s", AwayNick);
	}

	IRC->m_NickCatchTimer = NULL;

	return false;
}

/**
 * GetState
 *
 * Returns the state of the IRC connection.
 */
int CIRCConnection::GetState(void) {
	return m_State;
}
