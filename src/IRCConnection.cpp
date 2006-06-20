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

/**
 * CIRCConnection
 *
 * Constructs a new IRC connection object. This constructor should only be used
 * by ThawObject().
 *
 * @param Socket the IRC socket
 * @param Owner the owner of this connection
 * @param SSL whether to use SSL
 */
CIRCConnection::CIRCConnection(SOCKET Socket, CUser *Owner, bool SSL) : CConnection(Socket, SSL) {
	InitIrcConnection(Owner, true);
}

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
CIRCConnection::CIRCConnection(const char *Host, unsigned short Port, CUser *Owner, const char *BindIp, bool SSL, int Family) : CConnection(Host, Port, BindIp, SSL, Family) {
	InitIrcConnection(Owner);
}

/**
 * InitIrcConnection
 *
 * Initializes the connection object.
 *
 * @param Owner the owner of the connection object
 * @param Unfreezing whether the object is being de-persisted
 */
void CIRCConnection::InitIrcConnection(CUser *Owner, bool Unfreezing) {
	const char *Ident;

	SetRole(Role_Client);
	SetOwner(Owner);

	g_LastReconnect = g_CurrentTime;
	m_LastResponse = g_LastReconnect;

	m_State = State_Connecting;

	m_CurrentNick = NULL;

	m_QueueHigh = new CQueue();

	if (m_QueueHigh == NULL) {
		LOGERROR("new operator failed. Could not create queue.");

		g_Bouncer->Fatal();
	}

	m_QueueMiddle = new CQueue();

	if (m_QueueMiddle == NULL) {
		LOGERROR("new operator failed. Could not create queue.");

		g_Bouncer->Fatal();
	}

	m_QueueLow = new CQueue();

	if (m_QueueLow == NULL) {
		LOGERROR("new operator failed. Could not create queue.");

		g_Bouncer->Fatal();
	}

	m_FloodControl = new CFloodControl();

	if (m_FloodControl == NULL) {
		LOGERROR("new operator failed. Could not create flood control object.");

		g_Bouncer->Fatal();
	}

	if (!Unfreezing) {
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

	m_Channels = new CHashtable<CChannel *, false, 16>();

	if (m_Channels == NULL) {
		LOGERROR("new operator failed. Could not create channel list.");

		g_Bouncer->Fatal();
	}

	m_Channels->RegisterValueDestructor(DestroyObject<CChannel>);

	m_Server = NULL;
	m_ServerVersion = NULL;
	m_ServerFeat = NULL;

	m_ISupport = new CConfig(NULL, GetUser());

	if (m_ISupport == NULL) {
		LOGERROR("new operator failed. Could not create ISupport object.");

		g_Bouncer->Fatal();
	}

	m_ISupport->WriteString("CHANMODES", "bIe,k,l");
	m_ISupport->WriteString("CHANTYPES", "#&+");
	m_ISupport->WriteString("PREFIX", "(ov)@+");
	m_ISupport->WriteString("NAMESX", "");

	m_FloodControl->AttachInputQueue(m_QueueHigh, 0);
	m_FloodControl->AttachInputQueue(m_QueueMiddle, 1);
	m_FloodControl->AttachInputQueue(m_QueueLow, 2);

	m_PingTimer = g_Bouncer->CreateTimer(180, true, IRCPingTimer, this);
	m_DelayJoinTimer = NULL;

	m_Site = NULL;
	m_Usermodes = NULL;
}

/**
 * ~CIRCConnection
 *
 * Destructs a connection object.
 */
CIRCConnection::~CIRCConnection(void) {
	ufree(m_CurrentNick);
	ufree(m_Site);
	ufree(m_Usermodes);

	delete m_Channels;

	ufree(m_Server);
	ufree(m_ServerVersion);
	ufree(m_ServerFeat);

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
	if (m_CurrentNick && Nick && strcasecmp(Nick, m_CurrentNick) == 0) {
		b_Me = true;
	}

	free(Nick);

	Client = GetOwner()->GetClientConnection();

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

		if (GetCurrentNick() != NULL && Client != NULL) {
			return true;
		}

		if (ReturnValue) {
			WriteLine("NICK :%s_", argv[3]);
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
		}

		if (!ModuleEvent(argc, argv)) {
			free(Nick);
			return false;
		}

		/* don't log ctcp requests */
		if (argv[3][0] != '\1' && argv[3][strlen(argv[3]) - 1] != '\1' && Dest && Nick && m_CurrentNick && strcasecmp(Dest, m_CurrentNick) == 0 && strcasecmp(Nick, m_CurrentNick) != 0) {
			char *Dup;
			char *Delim;

			Dup = strdup(Reply);

			if (Dup == NULL) {
				LOGERROR("strdup() failed.");

				free(Nick);

				return true;
			}

			Delim = strstr(Dup, "!");

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
	} else if (argc > 3 && hashRaw == hashNotice && Client == NULL) {
		const char *Dest = argv[2];
		char *Nick;
		
		if (!ModuleEvent(argc, argv)) {
			return false;
		}

		Nick = ::NickFromHostmask(Reply);

		/* don't log ctcp replies */
		if (argv[3][0] != '\1' && argv[3][strlen(argv[3]) - 1] != '\1' && Dest && Nick && m_CurrentNick && strcasecmp(Dest, m_CurrentNick) == 0 && strcasecmp(Nick, m_CurrentNick) != 0) {
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

			if (Nick == NULL) {
				LOGERROR("NickFromHostmask() failed.");

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

				if (Nick == NULL) {
					LOGERROR("NickFromHostmask() failed.");

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
				char *Out;

				asprintf(&Out, "JOIN %s", argv[2]);

				if (Out == NULL) {
 					LOGERROR("asprintf() failed. Could not rejoin channel.");

					return bRet;
				}

				GetOwner()->Simulate(Out);

				free(Out);

				char *Dup = strdup(Reply);

				if (Dup == NULL) {
					LOGERROR("strdup() failed. Could not log event.");

					return bRet;
				}

				char *Delim = strstr(Dup, "!");
				const char *Host;

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
				Client->WriteLine(":%s!ident@sbnc NICK :%s", Client->GetNick(), argv[2]);
			}
		}

		ufree(m_CurrentNick);
		m_CurrentNick = ustrdup(argv[2]);

		ufree(m_Server);
		m_Server = ustrdup(Reply);
	} else if (argc > 2 && hashRaw == hashNick) {
		if (b_Me) {
			ufree(m_CurrentNick);
			m_CurrentNick = ustrdup(argv[2]);
		}

		Nick = NickFromHostmask(argv[0]);

		int i = 0;

		while (hash_t<CChannel *> *ChannelHash = m_Channels->Iterate(i++)) {
			ChannelHash->Value->RenameUser(Nick, argv[2]);
		}

		free(Nick);
	} else if (argc > 1 && hashRaw == hashQuit) {
		bool bRet = ModuleEvent(argc, argv);

		Nick = NickFromHostmask(argv[0]);

		unsigned int i = 0;

		while (hash_t<CChannel *> *ChannelHash = m_Channels->Iterate(i++)) {
			ChannelHash->Value->RemoveUser(Nick);
		}

		free(Nick);

		return bRet;
	} else if (argc > 1 && (iRaw == 422 || iRaw == 376)) {
		int DelayJoin = GetOwner()->GetDelayJoin();
		if (m_State != State_Connected) {
			const CVector<CModule *> *Modules = g_Bouncer->GetModules();

			for (unsigned int i = 0; i < Modules->GetLength(); i++) {
				(*Modules)[i]->ServerLogon(GetOwner()->GetUsername());
			}

			GetOwner()->Log("You were successfully connected to an IRC server.");
			g_Bouncer->Log("User %s connected to an IRC server.", GetOwner()->GetUsername());
		}

		if (Client != NULL && Client->GetPreviousNick() != NULL && strcmp(Client->GetPreviousNick(), m_CurrentNick) != 0) {
			const char *Site = GetSite();

			if (Site == NULL) {
				Site = "unknown@host";
			}

			Client->WriteLine(":%s!%s NICK %s", Client->GetPreviousNick(), Site, m_CurrentNick);
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

		g_Bouncer->LogUser(GetUser(), "Error received for %s: %s", GetOwner()->GetUsername(), argv[1]);
	} else if (argc > 3 && iRaw == 465) {
		g_Bouncer->LogUser(GetUser(), "G/K-line reason for %s: %s", GetOwner()->GetUsername(), argv[3]);
	} else if (argc > 5 && iRaw == 351) {
		ufree(m_ServerVersion);
		m_ServerVersion = ustrdup(argv[3]);

		ufree(m_ServerFeat);
		m_ServerFeat = ustrdup(argv[5]);
	} else if (argc > 3 && iRaw	== 5) {
		for (int i = 3; i < argc - 1; i++) {
			char *Dup = strdup(argv[i]);

			CHECK_ALLOC_RESULT(Dup, strdup) {
				return false;
			} CHECK_ALLOC_RESULT_END;

			char *Eq = strstr(Dup, "=");

			if (strcasecmp(Dup, "NAMESX") == 0) {
				WriteLine("PROTOCTL NAMESX");
			}

			if (Eq) {
				*Eq = '\0';

				m_ISupport->WriteString(Dup, ++Eq);
			} else
				m_ISupport->WriteString(Dup, "");

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
			m_Usermodes = (char *)urealloc(m_Usermodes, Length);

			CHECK_ALLOC_RESULT(m_Usermodes, realloc) {
				return false;
			} CHECK_ALLOC_RESULT_END;

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

			CHECK_ALLOC_RESULT(nicks, ArgTokenize) {
				return false;
			} CHECK_ALLOC_RESULT_END;

			nickv = ArgToArray(nicks);

			CHECK_ALLOC_RESULT(nickv, ArgToArray) {
				ArgFree(nicks);

				return false;
			} CHECK_ALLOC_RESULT_END;

			int nickc = ArgCount(nicks);

			for (int i = 0; i < nickc; i++) {
				const char *Nick = nickv[i];

				while (IsNickPrefix(*Nick)) {
					Nick++;
				}

				char *Modes = NULL;

				if (nickv[i] != Nick) {
					Modes = (char *)malloc(Nick - nickv[i] + 1);

					CHECK_ALLOC_RESULT(Modes, malloc) {} else {
						strmcpy(Modes, nickv[i], Nick - nickv[i] + 1);
					} CHECK_ALLOC_RESULT_END;
				}

				Channel->AddUser(Nick, Modes);

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

		asprintf(&Mask, "%s!%s@%s", Nick, Ident, Host);

		UpdateHostHelper(Mask);
		UpdateWhoHelper(Nick, Realname, Server);

		free(Mask);
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
		ufree(m_Site);
		m_Site = ustrdup(argv[3]);

		if (m_Site == NULL) {
			LOGERROR("ustrdup() failed.");
		}
	} else if (argc > 3 && hashRaw == hashPong && strcasecmp(argv[3], "sbnc") == 0) {
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

	for (unsigned int i = 0; i < Modules->GetLength(); i++) {
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
		LOGERROR("ArgToArray2 returned NULL. Could not parse line (%s).", Line);

		return;
	}

	if (ParseLineArgV(argc, argv)) {
		if (strcasecmp(argv[0], "ping") == 0 && argc > 1) {
			asprintf(&Out, "PONG :%s", argv[1]);

			CHECK_ALLOC_RESULT(Out, asprintf) {} else {
				m_QueueHigh->QueueItem(Out);
				free(Out);
			} CHECK_ALLOC_RESULT_END;

			if (m_State != State_Connected) {
				m_State = State_Pong;
			}
		} else {
			CUser *User = GetOwner();

			if (User) {
				CClientConnection *Client = User->GetClientConnection();

				if (Client != NULL) {
					Client->WriteLine("%s", Line);
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
	CUser *User;
	CChannel *ChannelObj;
	bool LimitExceeded = false;

	if (g_Bouncer->GetResourceLimit("channels") < m_Channels->GetLength()) {
		LimitExceeded = true;
		ChannelObj = NULL;
	} else {
		ChannelObj = unew CChannel(Channel, this);
	}

	CHECK_ALLOC_RESULT(ChannelObj, unew) {
		WriteLine("PART %s", Channel);

		User = GetUser();

		if (User->MemoryIsLimitExceeded()) {
			LimitExceeded = true;
		}

		if (LimitExceeded) {
			User->Log("Memory/Channel limit exceeded. Removing channel (%s).", Channel);
		}
	} CHECK_ALLOC_RESULT_END;

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

		CHECK_ALLOC_RESULT(Out, realloc) {
			return;
		} CHECK_ALLOC_RESULT_END;

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
void CIRCConnection::Write(void) {
	char *Line = m_FloodControl->DequeueItem();

	if (Line != NULL) {
		CConnection::WriteUnformattedLine(Line);
	}

	CConnection::Write();

	free(Line);
}

/**
 * GetISupport
 *
 * Returns a feature's value.
 *
 * @param Feature the name of the feature
 */
const char *CIRCConnection::GetISupport(const char *Feature) const {
	return m_ISupport->ReadString(Feature);
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
	m_ISupport->WriteString(Feature, Value);
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

	for (unsigned int i = 0; i < strlen(Prefixes); i++) {
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
	const char *ActualPrefixes = strstr(Prefixes, ")");

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
 * Returns a configuration object which contains all 005 replies.
 */
const CConfig *CIRCConnection::GetISupportAll(void) const {
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
	int a = 0, i = 0;

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

	NickEnd = strstr(Host, "!");

	if (NickEnd == NULL) {
		return;
	}

	Offset = NickEnd - Host;

	Copy = strdup(Host);

	if (Copy == NULL) {
		LOGERROR("strdup() failed. Could not update hostmask. (%s)", Host);

		return;
	}

	const char *Nick = Copy;
	char *Site = Copy + Offset;

	*Site = '\0';
	Site++;

	if (m_CurrentNick && strcasecmp(Nick, m_CurrentNick) == 0) {
		ufree(m_Site);
		m_Site = ustrdup(Site);

		if (m_Site == NULL) {
			LOGERROR("ustrdup() failed.");
		}
	}

	if (GetOwner()->GetLeanMode() > 0) {
		free(Copy);

		return;
	}

	int i = 0;

	while (hash_t<CChannel *> *Chan = m_Channels->Iterate(i++)) {
		if (!Chan->Value->HasNames()) {
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
	if (!m_Locked) {
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

		CHECK_ALLOC_RESULT(DupChannels, strdup) {
			return;
		} CHECK_ALLOC_RESULT_END;

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

					CHECK_ALLOC_RESULT(ChanList, malloc) {
						free(DupChannels);

						return;
					} CHECK_ALLOC_RESULT_END;

					strmcpy(ChanList, Channel, Size);
				} else {
					Size = strlen(ChanList) + 1 + strlen(Channel) + 2;
					newChanList = (char *)realloc(ChanList, Size);

					if (newChanList == NULL) {
						LOGERROR("realloc() failed. Could not join channel.");

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

bool CIRCConnection::Read(void) {
	bool Ret = CConnection::Read();

	if (Ret && GetRecvqSize() > 5120) {
		Kill("RecvQ exceeded.");
	}

	return Ret;
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
		IRC->WriteLine("PING :sbnc");

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
CHashtable<CChannel *, false, 16> *CIRCConnection::GetChannels(void) {
	return m_Channels;
}

/**
 * GetSite
 *
 * Returns the site (ident\@host) for the IRC connection.
 */
const char *CIRCConnection::GetSite(void) const {
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
	GetOwner()->Privmsg(Context->cert->name);
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
	if (Response == NULL) {
		g_Bouncer->LogUser(GetOwner(), "DNS request for %s failed. No such hostname (NXDOMAIN).", GetOwner()->GetUsername());
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
	if (Response == NULL) {
		g_Bouncer->LogUser(GetOwner(), "DNS request (vhost) for %s failed. No such hostname (NXDOMAIN).", GetOwner()->GetUsername());
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

/* TODO: persist version and other stuff */
/**
 * Freeze
 *
 * Persists an IRC connection object.
 *
 * @param Box the box which should be used
 */
RESULT<bool> CIRCConnection::Freeze(CAssocArray *Box) {
	CAssocArray *QueueHighBox, *QueueMiddleBox, *QueueLowBox;

	if (m_CurrentNick == NULL || m_Server == NULL || GetSocket() == INVALID_SOCKET || IsSSL()) {
		THROW(bool, Generic_Unknown, "Current Nick/Server/Sock invalid or connection is using SSL.");
	}

	Box->AddString("~irc.nick", m_CurrentNick);
	Box->AddString("~irc.server", m_Server);
	Box->AddInteger("~irc.fd", GetSocket());

	Box->AddString("~irc.serverversion", m_ServerVersion);
	Box->AddString("~irc.serverfeat", m_ServerFeat);

	FreezeObject<CConfig>(Box, "~irc.isupport", m_ISupport);
	m_ISupport = NULL;

	Box->AddInteger("~irc.channels", m_Channels->GetLength());

	char *Out;
	int i = 0;
	hash_t<CChannel *> *Hash;

	while ((Hash =  m_Channels->Iterate(i++)) != NULL) {
		asprintf(&Out, "~irc.channel%d", i - 1);
		FreezeObject<CChannel>(Box, Out, Hash->Value);
		free(Out);
	}

	m_Channels->RegisterValueDestructor(NULL);
	delete m_Channels;
	m_Channels = NULL;

	QueueHighBox = Box->Create();
	m_QueueHigh->Freeze(QueueHighBox);
	m_QueueHigh = NULL;
	Box->AddBox("~irc.queuehigh", QueueHighBox);

	QueueMiddleBox = Box->Create();
	m_QueueMiddle->Freeze(QueueMiddleBox);
	m_QueueMiddle = NULL;
	Box->AddBox("~irc.queuemiddle", QueueMiddleBox);

	QueueLowBox = Box->Create();
	m_QueueLow->Freeze(QueueLowBox);
	m_QueueLow = NULL;
	Box->AddBox("~irc.queuelow", QueueLowBox);

	// protect the socket from being closed
	g_Bouncer->UnregisterSocket(GetSocket());
	SetSocket(INVALID_SOCKET);

	Destroy();

	RETURN(bool, true);
}

/**
 * Thaw
 *
 * De-persists an IRC connection object.
 *
 * @param Box the box
 * @param Owner the owner of the IRC connection
 */
RESULT<CIRCConnection *> CIRCConnection::Thaw(CAssocArray *Box, CUser *Owner) {
	SOCKET Socket;
	CIRCConnection *Connection;
	CAssocArray *TempBox;
	char *Out;
	const char *Temp;
	unsigned int Count;

	if (Box == NULL) {
		THROW(CIRCConnection *, Generic_InvalidArgument, "Box cannot be NULL.");
	}

	Socket = Box->ReadInteger("~irc.fd");

	Connection = new CIRCConnection(Socket, Owner, false);

	Connection->m_CurrentNick = nstrdup(Box->ReadString("~irc.nick"));
	Connection->m_Server = nstrdup(Box->ReadString("~irc.server"));

	Temp = Box->ReadString("~irc.serverversion");

	if (Temp != NULL){
		Connection->m_ServerVersion = nstrdup(Temp);
	}

	Temp = Box->ReadString("~irc.serverfeat");

	if (Temp != NULL) {
		Connection->m_ServerFeat = nstrdup(Temp);
	}

	TempBox = Box->ReadBox("~irc.isupport");

	delete Connection->m_ISupport;

	Connection->m_ISupport = ThawObject<CConfig>(Box, "~irc.isupport", Connection->GetUser());

	Count = Box->ReadInteger("~irc.channels");

	Connection->WriteLine("VERSION");

	for (unsigned int i = 0; i < Count; i++) {
		CChannel *Channel;

		asprintf(&Out, "~irc.channel%d", i);

		CHECK_ALLOC_RESULT(Out, asprintf) {
			g_Bouncer->Fatal();
		} CHECK_ALLOC_RESULT_END;

		Channel = ThawObject<CChannel>(Box, Out, Connection);

		Connection->m_Channels->Add(Channel->GetName(), Channel);

		free(Out);
	}

	delete Connection->m_FloodControl;

	Connection->m_FloodControl = new CFloodControl();

	TempBox = Box->ReadBox("~irc.queuehigh");

	if (TempBox != NULL) {
		delete Connection->m_QueueHigh;

		Connection->m_QueueHigh = CQueue::Thaw(TempBox);
	}

	Connection->m_FloodControl->AttachInputQueue(Connection->m_QueueHigh, 0);

	TempBox = Box->ReadBox("~irc.queuemiddle");

	if (TempBox != NULL) {
		delete Connection->m_QueueMiddle;

		Connection->m_QueueMiddle = CQueue::Thaw(TempBox);
	}

	Connection->m_FloodControl->AttachInputQueue(Connection->m_QueueMiddle, 0);

	TempBox = Box->ReadBox("~irc.queuelow");

	if (TempBox != NULL) {
		delete Connection->m_QueueLow;

		Connection->m_QueueLow = CQueue::Thaw(TempBox);
	}

	Connection->m_FloodControl->AttachInputQueue(Connection->m_QueueLow, 0);

	RETURN(CIRCConnection *, Connection);
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

		if (m_Config != NULL) {
			m_Config->SetOwner(NULL);
		}

		if (m_ISupport != NULL) {
			m_ISupport->SetOwner(NULL);
		}
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
/* TODO: check comment */
char CIRCConnection::GetHighestUserFlag(const char *Modes) const {
	bool Flip = false;
	const char *Prefixes = GetISupport("PREFIX");

	if (Modes == NULL || Prefixes == NULL) {
		return '\0';
	}

	for (unsigned int i = 0; i < strlen(Prefixes); i++) {
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
 */
void CIRCConnection::Error(void) {
	int ErrorValue;
	socklen_t ErrorValueLength = sizeof(ErrorValue);
	char *ErrorMsg = NULL;

	if (getsockopt(GetSocket(), SOL_SOCKET, SO_ERROR, (char *)&ErrorValue, &ErrorValueLength) == 0) {
#ifdef _WIN32
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, ErrorValue, 0, (char *)&ErrorMsg, 0, NULL);
#else
		ErrorMsg = strerror(ErrorValue);
#endif
	}

	if (m_State == State_Connecting && GetOwner() != NULL) {
		if (!IsConnected()) {
			if (ErrorMsg == NULL || ErrorMsg[0] == '\0') {
				g_Bouncer->LogUser(GetOwner(), "An error occured while connecting for user %s.", GetOwner()->GetUsername());
			} else {
				g_Bouncer->LogUser(GetOwner(), "An error occured while connecting for user %s: %s", GetOwner()->GetUsername(), ErrorMsg);
			}
		} else {
			if (ErrorMsg == NULL || ErrorMsg[0] == '\0') {
				g_Bouncer->LogUser(GetOwner(), "An error occured while processing a connection for user %s.", GetOwner()->GetUsername());
			} else {
				g_Bouncer->LogUser(GetOwner(), "An error occured while processing a connection for user %s: %s", GetOwner()->GetUsername(), ErrorMsg);
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
