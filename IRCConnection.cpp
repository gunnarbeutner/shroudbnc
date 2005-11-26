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

CIRCConnection::CIRCConnection(SOCKET Socket, CBouncerUser* Owning) : CConnection(Socket) {
	m_AdnsTimeout = NULL;
	m_AdnsQuery = NULL;

	m_BindIpCache = NULL;

	InitIrcConnection(Owning);
}

CIRCConnection::CIRCConnection(const char* Host, unsigned short Port, CBouncerUser* Owning, const char* BindIp) : CConnection(INVALID_SOCKET) {
	in_addr ip;

#ifdef _WIN32
	ip.S_un.S_addr = inet_addr(Host);

	if (ip.S_un.S_addr == INADDR_NONE) {
#else
	ip.s_addr = inet_addr(Host);

	if (ip.s_addr == INADDR_NONE) {
#endif
		m_Socket = INVALID_SOCKET;
		m_PortCache = Port;
		m_BindIpCache = BindIp ? strdup(BindIp) : NULL;

		m_AdnsQuery = (adns_query*)malloc(sizeof(adns_query));
		adns_submit(g_adns_State, Host, adns_r_addr, (adns_queryflags)0, static_cast<CDnsEvents*>(this), m_AdnsQuery);

		m_AdnsTimeout = g_Bouncer->CreateTimer(3, true, IRCAdnsTimeoutTimer, this);

		InitIrcConnection(Owning);
	} else {
		m_PortCache = 0;
		m_BindIpCache = NULL;
		m_AdnsQuery = NULL;
		m_AdnsTimeout = NULL;

		InitIrcConnection(Owning);

		m_Socket = SocketAndConnectResolved((in_addr)ip, Port, BindIp);

		g_Bouncer->RegisterSocket(m_Socket, (CSocketEvents*)this);

		InitSocket();
	}
}

void CIRCConnection::InitIrcConnection(CBouncerUser* Owning) {
	m_State = State_Connecting;

	m_CurrentNick = NULL;

	m_LatchedDestruction = false;

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

	m_FloodControl = new CFloodControl(this);

	if (m_FloodControl == NULL) {
		LOGERROR("new operator failed. Could not create flood control object.");

		g_Bouncer->Fatal();
	}

	const char* Password = Owning->GetServerPassword();

	if (Password)
		WriteLine("PASS :%s", Password);

	WriteLine("NICK %s", Owning->GetNick());
	WriteLine("USER %s \"\" \"fnords\" :%s", Owning->GetUsername(), Owning->GetRealname());

	m_Owner = Owning;

	m_Channels = new CHashtable<CChannel*, false, 16>();

	if (m_Channels == NULL) {
		LOGERROR("new operator failed. Could not create channel list.");

		g_Bouncer->Fatal();
	}

	m_Channels->RegisterValueDestructor(DestroyCChannel);

	m_Server = NULL;

	m_ServerVersion = NULL;
	m_ServerFeat = NULL;

	m_LastBurst = time(NULL);

	m_ISupport = new CBouncerConfig(NULL);

	if (m_ISupport == NULL) {
		LOGERROR("new operator failed. Could not create ISupport object.");

		g_Bouncer->Fatal();
	}

	m_ISupport->WriteString("CHANMODES", "bIe,k,l");
	m_ISupport->WriteString("CHANTYPES", "#&+");
	m_ISupport->WriteString("PREFIX", "(ov)@+");

	m_FloodControl->AttachInputQueue(m_QueueHigh, 0);
	m_FloodControl->AttachInputQueue(m_QueueMiddle, 1);
	m_FloodControl->AttachInputQueue(m_QueueLow, 2);

	if (m_Socket != INVALID_SOCKET)
		g_Bouncer->RegisterSocket(m_Socket, (CSocketEvents*)this);

	m_PingTimer = g_Bouncer->CreateTimer(180, true, IRCPingTimer, this);
	m_DelayJoinTimer = NULL;

	m_Site = NULL;
}

CIRCConnection::~CIRCConnection() {
	free(m_CurrentNick);

	delete m_Channels;

	free(m_Server);
	free(m_ServerVersion);
	free(m_ServerFeat);

	delete m_ISupport;

	delete m_QueueLow;
	delete m_QueueMiddle;
	delete m_QueueHigh;
	delete m_FloodControl;

	g_Bouncer->UnregisterSocket(m_Socket);

	if (m_DelayJoinTimer)
		m_DelayJoinTimer->Destroy();

	if (m_AdnsTimeout)
		m_AdnsTimeout->Destroy();

	if (m_AdnsQuery) {
		adns_cancel(*m_AdnsQuery);

		m_AdnsQuery = NULL;
	}

	m_PingTimer->Destroy();

	if (m_BindIpCache)
		free(m_BindIpCache);
}

connection_role_e CIRCConnection::GetRole(void) {
	return Role_IRC;
}

bool CIRCConnection::ParseLineArgV(int argc, const char** argv) {
	if (argc < 2)
		return true;

	const char* Reply = argv[0];
	const char* Raw = argv[1];
	char* Nick = ::NickFromHostmask(Reply);
	int iRaw = atoi(Raw);

	bool b_Me = false;
	if (m_CurrentNick && Nick && strcmpi(Nick, m_CurrentNick) == 0)
		b_Me = true;

	free(Nick);

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

	if (!GetOwningClient()->GetClientConnection() && iRaw == 433) {
		bool Ret = ModuleEvent(argc, argv);

		if (Ret)
			WriteLine("NICK :%s_", argv[3]);

		return Ret;
	} else if (argc > 3 && hashRaw == hashPrivmsg && !GetOwningClient()->GetClientConnection()) {
		const char* Dest = argv[2];
		char* Nick = ::NickFromHostmask(Reply);

		if (argv[3][0] != '\1' && argv[3][strlen(argv[3]) - 1] != '\1' && Dest && Nick && m_CurrentNick && strcmpi(Dest, m_CurrentNick) == 0 && strcmpi(Nick, m_CurrentNick) != 0) {
			char* Dup = strdup(Reply);

			char* Delim = strstr(Dup, "!");

			*Delim = '\0';

			const char* Host = Delim + 1;

			GetOwningClient()->Log("%s (%s): %s", Dup, Delim ? Host : "<unknown host>", argv[3]);

			free(Dup);
		}

		CChannel* Chan = GetChannel(Dest);

		if (Chan) {
			CNick* User = Chan->GetNames()->Get(Nick);

			if (User)
				User->SetIdleSince(time(NULL));
		}

		free(Nick);

		UpdateHostHelper(Reply);
	} else if (argc > 3 && hashRaw == hashNotice && !GetOwningClient()->GetClientConnection()) {
		const char* Dest = argv[2];
		char* Nick = ::NickFromHostmask(Reply);

		if (argv[3][0] != '\1' && argv[3][strlen(argv[3]) - 1] != '\1' && Dest && Nick && m_CurrentNick && strcmpi(Dest, m_CurrentNick) == 0 && strcmpi(Nick, m_CurrentNick) != 0) {
			char* Dup = strdup(Reply);

			char* Delim = strstr(Dup, "!");

			*Delim = '\0';

			const char* Host = Delim + 1;

			GetOwningClient()->Log("%s (notice): %s", Reply, argv[3]);

			free(Dup);
		}

		free(Nick);
	} else if (argc > 2 && hashRaw == hashJoin) {
		if (b_Me) {
			AddChannel(argv[2]);

			/* m_Owner can be NULL if AddChannel failed */
			if (m_Owner && !m_Owner->GetClientConnection())
				WriteLine("MODE %s", argv[2]);
		}

		CChannel* Chan = GetChannel(argv[2]);

		if (Chan) {
			Nick = NickFromHostmask(Reply);

			if (Nick == NULL) {
				LOGERROR("NickFromHostmask() failed.");

				return false;
			}

			Chan->AddUser(Nick, '\0');
			free(Nick);
		}

		UpdateHostHelper(Reply);
	} else if (argc > 2 && hashRaw == hashPart) {
		bool bRet = ModuleEvent(argc, argv);

		if (b_Me)
			RemoveChannel(argv[2]);
		else {
			CChannel* Chan = GetChannel(argv[2]);

			if (Chan) {
				Nick = NickFromHostmask(Reply);
				Chan->RemoveUser(Nick);
				free(Nick);
			}
		}

		UpdateHostHelper(Reply);

		return bRet;
	} else if (argc > 2 && hashRaw == hashKick) {
		bool bRet = ModuleEvent(argc, argv);

		if (m_CurrentNick && strcmpi(argv[3], m_CurrentNick) == 0) {
			RemoveChannel(argv[2]);

			if (GetOwningClient()->GetClientConnection() == NULL) {
				char* Out;

				asprintf(&Out, "JOIN %s", argv[2]);

				if (Out == NULL) {
					LOGERROR("asprintf() failed. Could not rejoin channel.");

					return bRet;
				}

				m_Owner->Simulate(Out);

				free(Out);

				char* Dup = strdup(Reply);

				if (Dup == NULL) {
					LOGERROR("strdup() failed. Could not log event.");

					return bRet;
				}

				char* Delim = strstr(Dup, "!");
				const char* Host;

				if (Delim) {
					*Delim = '\0';

					Host = Delim + 1;
				}

				GetOwningClient()->Log("%s (%s) kicked you from %s (%s)", Dup, Host, argv[2], argc > 4 ? argv[4] : "");

				free(Dup);
			}
		} else {
			CChannel* Chan = GetChannel(argv[2]);

			if (Chan)
				Chan->RemoveUser(argv[3]);
		}

		UpdateHostHelper(Reply);

		return bRet;
	} else if (argc > 2 && iRaw == 1) {
		CClientConnection* Client = GetOwningClient()->GetClientConnection();

		if (Client) {
			if (strcmp(Client->GetNick(), argv[2]) != 0) {
				Client->WriteLine(":%s!ident@sbnc NICK :%s", Client->GetNick(), argv[2]);
			}
		}

		free(m_CurrentNick);
		m_CurrentNick = strdup(argv[2]);

		free(m_Server);
		m_Server = strdup(Reply);
	} else if (argc > 1 && hashRaw == hashNick) {
		if (b_Me) {
			free(m_CurrentNick);
			m_CurrentNick = strdup(argv[2]);
		}

		Nick = NickFromHostmask(argv[0]);

		int i = 0;

		while (xhash_t<CChannel*>* Chan = m_Channels->Iterate(i++)) {
			CHashtable<CNick*, false, 64, true>* Nicks = Chan->Value->GetNames();

			CNick* NickObj = Nicks->Get(Nick);

			if (NickObj) {
				Nicks->Remove(Nick, true);

				NickObj->SetNick(argv[2]);

				Nicks->Add(NickObj->GetNick(), NickObj);
			}
		}

		free(Nick);
	} else if (argc > 1 && hashRaw == hashQuit) {
		bool bRet = ModuleEvent(argc, argv);

		Nick = NickFromHostmask(argv[0]);

		int i = 0;

		while (xhash_t<CChannel*>* Chan = m_Channels->Iterate(i++)) {
			Chan->Value->GetNames()->Remove(Nick);
		}

		free(Nick);
		
		return bRet;
	} else if (argc > 1 && (iRaw == 422 || iRaw == 376)) {
		int DelayJoin = m_Owner->GetDelayJoin();

		if (DelayJoin == 1)
			m_DelayJoinTimer = g_Bouncer->CreateTimer(5, false, DelayJoinTimer, this);
		else if (DelayJoin == 0)
			JoinChannels();

		if (m_State != State_Connected) {
			CModule** Modules = g_Bouncer->GetModules();

			for (int i = 0; i < g_Bouncer->GetModuleCount(); i++) {
				CModule* M = Modules[i];

				if (M)
					M->ServerLogon(m_Owner->GetUsername());
			}

			GetOwningClient()->Notice("Connected to an IRC server.");

			g_Bouncer->Log("Connected to an IRC server. (%s)", m_Owner->GetUsername());
		}

		if (GetOwningClient()->GetClientConnection() == NULL) {
			bool AppendTS = (GetOwningClient()->GetConfig()->ReadInteger("user.ts") != 0);
			const char* AwayReason = GetOwningClient()->GetAwayText();

			if (AwayReason)
				WriteLine(AppendTS ? "AWAY :%s (Away since the dawn of time)" : "AWAY :%s", AwayReason);
		}

		const char* AutoModes = GetOwningClient()->GetAutoModes();
		const char* DropModes = GetOwningClient()->GetDropModes();

		if (AutoModes && *AutoModes)
			WriteLine("MODE %s +%s", GetCurrentNick(), AutoModes);

		if (!GetOwningClient()->GetClientConnection() && DropModes && *DropModes)
			WriteLine("MODE %s -%s", GetCurrentNick(), DropModes);

		m_State = State_Connected;
	} else if (argc > 1 && strcmpi(Reply, "ERROR") == 0) {
		if (strstr(Raw, "throttle") != NULL)
			GetOwningClient()->ScheduleReconnect(50);
		else
			GetOwningClient()->ScheduleReconnect(5);

		char* Out;

		asprintf(&Out, "Error received for %s: %s", GetOwningClient()->GetUsername(), argv[1]);

		if (Out == NULL) {
			LOGERROR("asprintf() failed.");

			return false;
		}

		g_Bouncer->GlobalNotice(Out, true);
		g_Bouncer->Log("%s", Out);

		if (!GetOwningClient()->GetClientConnection())
			GetOwningClient()->GetLog()->InternalWriteLine(Out);

		free(Out);
	} else if (argc > 3 && iRaw == 465) {
		char* Out;

		asprintf(&Out, "G/K-line reason for %s: %s", GetOwningClient()->GetUsername(), argv[3]);

		if (Out == NULL) {
			LOGERROR("asprintf() failed.");

			return false;
		}

		g_Bouncer->GlobalNotice(Out, true);
		g_Bouncer->Log("%s", Out);

		if (!GetOwningClient()->GetClientConnection())
			GetOwningClient()->GetLog()->InternalWriteLine(Out);

		free(Out);
	} else if (argc > 3 && iRaw == 351) {
		free(m_ServerVersion);
		m_ServerVersion = strdup(argv[3]);

		free(m_ServerFeat);
		m_ServerFeat = strdup(argv[5]);
	} else if (argc > 3 && iRaw == 5) {
		for (int i = 3; i < argc - 1; i++) {
			char* Dup = strdup(argv[i]);

			if (Dup == NULL) {
				LOGERROR("strdup failed. Could not parse 005 reply.");

				return false;
			}

			char* Eq = strstr(Dup, "=");

			if (Eq) {
				*Eq = '\0';

				m_ISupport->WriteString(Dup, ++Eq);
			} else
				m_ISupport->WriteString(Dup, "");

			free(Dup);
		}
	} else if (argc > 4 && iRaw == 324) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan) {
			Chan->ClearModes();
			Chan->ParseModeChange(argv[0], argv[4], argc - 5, &argv[5]);
			Chan->SetModesValid(true);
		}
	} else if (argc > 3 && hashRaw == hashMode) {
		CChannel* Chan = GetChannel(argv[2]);

		if (Chan)
			Chan->ParseModeChange(argv[0], argv[3], argc - 4, &argv[4]);

		UpdateHostHelper(Reply);
	} else if (argc > 4 && iRaw == 329) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan)
			Chan->SetCreationTime(atoi(argv[4]));
	} else if (argc > 4 && iRaw == 332) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan)
			Chan->SetTopic(argv[4]);
	} else if (argc > 5 && iRaw == 333) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan) {
			Chan->SetTopicNick(argv[4]);
			Chan->SetTopicStamp(atoi(argv[5]));
		}
	} else if (argc > 3 && iRaw == 331) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan) {
			Chan->SetNoTopic();
		}
	} else if (argc > 3 && hashRaw == hashTopic) {
		CChannel* Chan = GetChannel(argv[2]);

		if (Chan) {
			Chan->SetTopic(argv[3]);
			Chan->SetTopicStamp(time(NULL));
			Chan->SetTopicNick(argv[0]);
		}

		UpdateHostHelper(Reply);
	} else if (argc > 5 && iRaw == 353) {
		CChannel* Chan = GetChannel(argv[4]);

		const char* nicks;
		const char** nickv;
		int nickc;

		nicks = ArgTokenize(argv[5]);

		if (nicks == NULL) {
			LOGERROR("ArgTokenize() failed. could not parse 353 reply for channel %s", argv[4]);

			return false;
		}

		nickv = ArgToArray(nicks);

		if (nickv == NULL) {
			LOGERROR("ArgToArray() failed. could not parse 353 reply for channel %s", argv[4]);

			ArgFree(nicks);

			return false;
		}

		nickc = ArgCount(nicks);

		if (Chan) {
			for (int i = 0; i < nickc; i++) {
				const char* n = nickv[i];

				while (IsNickPrefix(*n))
					++n;

				char* Modes = NULL;

				if (nickv[i] != n) {
					Modes = (char*)malloc(n - nickv[i] + 1);

					strncpy(Modes, nickv[i], n - nickv[i]);
					Modes[n - nickv[i]] = '\0';
				}

				Chan->AddUser(n, Modes);

				free(Modes);
			}
		}

		ArgFreeArray(nickv);
		ArgFree(nicks);
	} else if (argc > 3 && iRaw == 366) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan)
			Chan->SetHasNames();
	} else if (argc > 7 && iRaw == 352) {
		const char* Ident = argv[4];
		const char* Host = argv[5];
		const char* Nick = argv[7];

		char* Mask = (char*)malloc(strlen(Nick) + 1 + strlen(Ident) + 1 + strlen(Host) + 1);

		snprintf(Mask, strlen(Nick) + 1 + strlen(Ident) + 1 + strlen(Host) + 1, "%s!%s@%s", Nick, Ident, Host);

		UpdateHostHelper(Mask);

		free(Mask);
	} else if (argc > 6 && iRaw == 367) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan)
			Chan->GetBanlist()->SetBan(argv[4], argv[5], atoi(argv[6]));
	} else if (argc > 3 && iRaw == 368) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan)
			Chan->SetHasBans();
	} else if (argc > 3 && iRaw == 396) {
		free(m_Site);
		m_Site = strdup(argv[3]);

		if (m_Site == NULL) {
			LOGERROR("strdup() failed.");
		}
	} else if (argc > 3 && hashRaw == hashPong && strcmpi(argv[3], "sbnc") == 0) {
		return false;
	}

	if (m_Owner)
		return ModuleEvent(argc, argv);
	else
		return true;
}

bool CIRCConnection::ModuleEvent(int argc, const char** argv) {
	CModule** Modules = g_Bouncer->GetModules();
	int Count = g_Bouncer->GetModuleCount();

	for (int i = 0; i < Count; i++) {
		if (Modules[i]) {
			if (!Modules[i]->InterceptIRCMessage(this, argc, argv))
				return false;
		}
	}

	return true;
}

void CIRCConnection::ParseLine(const char* Line) {
	if (!GetOwningClient())
		return;

	const char* Args = ArgParseServerLine(Line);

	if (Args == NULL) {
		LOGERROR("ArgParseServerLine() returned NULL. Could not parse line (%s).", Line);

		return;
	}

	const char** argv = ArgToArray(Args);
	int argc = ArgCount(Args);

	if (argv == NULL) {
		LOGERROR("ArgToArray returned NULL. Could not parse line (%s).", Line);

		ArgFree(Args);

		return;
	}

	if (ParseLineArgV(argc, argv)) {
		if (strcmpi(argv[0], "ping") == 0 && argc > 1) {
			WriteLine("PONG :%s", argv[1]);

			if (m_State != State_Connected)
				m_State = State_Pong;
		} else {
			CBouncerUser* User = GetOwningClient();

			if (User) {
				CClientConnection* Client = User->GetClientConnection();

				if (Client)
					Client->InternalWriteLine(Line);
			}
		}
	}

#ifdef _WIN32
	OutputDebugString(Line);
	OutputDebugString("\n");
#endif

	//puts(Line);

	ArgFreeArray(argv);
	ArgFree(Args);
}

const char* CIRCConnection::GetCurrentNick(void) {
	return m_CurrentNick;
}

void CIRCConnection::AddChannel(const char* Channel) {
	CChannel* ChannelObj = new CChannel(Channel, this);

	if (ChannelObj == NULL) {
		LOGERROR("new operator failed. could not add channel.");

		return;
	}

	m_Channels->Add(Channel, ChannelObj);

	UpdateChannelConfig();
}

void CIRCConnection::RemoveChannel(const char* Channel) {
	m_Channels->Remove(Channel);

	UpdateChannelConfig();
}

const char* CIRCConnection::GetServer(void) {
	return m_Server;
}

void CIRCConnection::Destroy(void) {
	if (m_Owner)
		m_Owner->SetIRCConnection(NULL);

	delete this;
}

void CIRCConnection::UpdateChannelConfig(void) {
	char* Out = NULL;

	int a = 0;

	while (xhash_t<CChannel*>* Chan = m_Channels->Iterate(a++)) {
		bool WasNull = (Out == NULL);

		Out = (char*)realloc(Out, (Out ? strlen(Out) : 0) + strlen(Chan->Name) + 2);

		if (Out == NULL) {
			LOGERROR("realloc() failed. Channel config file might be out of date.");

			return;
		}

		if (!WasNull)
			strcat(Out, ",");
		else
			Out[0] = '\0';

		strcat(Out, Chan->Name);
	}

	/* m_Owner can be NULL if the last channel was not created successfully */
	if (m_Owner)
		m_Owner->SetConfigChannels(Out);

	free(Out);
}

bool CIRCConnection::IsOnChannel(const char* Channel) {
	int a = 0;

	while (xhash_t<CChannel*>* Chan = m_Channels->Iterate(a++)) {
		if (strcmpi(Chan->Name, Channel) == 0)
			return true;
	}

	return false;
}

char* CIRCConnection::NickFromHostmask(const char* Hostmask) {
	return ::NickFromHostmask(Hostmask);
}

void CIRCConnection::FreeNick(char* Nick) {
	free(Nick);
}

bool CIRCConnection::HasQueuedData(void) {
	if (m_FloodControl->GetQueueSize())
		return true;
	else
		return false;
}

void CIRCConnection::Write(void) {
	char* Line = m_FloodControl->DequeueItem();

	if (!Line)
		return;

	char* Copy = (char*)malloc(strlen(Line) + 2);

	if (Copy == NULL) {
		LOGERROR("malloc() failed. Line was lost. (%s)", Line);

		return;
	}

	snprintf(Copy, strlen(Line) + 2, "%s\n", Line);

	send(m_Socket, Copy, strlen(Copy), 0);

	if (GetTrafficStats())
		GetTrafficStats()->AddOutbound(strlen(Copy));

	free(Copy);
	free(Line);
}

const char* CIRCConnection::GetISupport(const char* Feature) {
	const char* Val = m_ISupport->ReadString(Feature);

	return Val;
}

bool CIRCConnection::IsChanMode(char Mode) {
	const char* Modes = GetISupport("CHANMODES");

	return strchr(Modes, Mode) != NULL;
}

int CIRCConnection::RequiresParameter(char Mode) {
	char* Modes = strdup(GetISupport("CHANMODES"));

	if (Modes == NULL) {
		LOGERROR("strdup() failed.");

		return 0;
	}

	for (unsigned int i = 0; i < strlen(Modes); i++) {
		if (Modes[i] == ',')
			Modes[i] = ' ';
	}

	const char* Args = ArgTokenize(Modes);

	if (Args == NULL) {
		LOGERROR("ArgTokenize() failed.");

		free(Modes);

		return 0;
	}

	free(Modes);

	const char** argv = ArgToArray(Args);

	if (argv == NULL) {
		LOGERROR("ArgToArray() failed.");

		ArgFree(Args);

		return 0;
	}

	int argc = ArgCount(Args);
	int RetVal = 0;

	if (argc > 0 && strchr(argv[0], Mode) != NULL)
		RetVal = 3;
	else if (argc > 1 && strchr(argv[1], Mode) != NULL)
		RetVal = 2;
	else if (argc > 2 && strchr(argv[2], Mode) != NULL)
		RetVal = 1;
	else if (argc > 3 && strchr(argv[3], Mode) != NULL)
		RetVal = 0;
	else if (IsNickMode(Mode))
		RetVal = 2;

	ArgFree(Args);
	ArgFreeArray(argv);
	
	return RetVal;
}

CChannel* CIRCConnection::GetChannel(const char* Name) {
	return m_Channels->Get(Name);
}

bool CIRCConnection::IsNickPrefix(char Char) {
	const char* Prefixes = GetISupport("PREFIX");
	bool flip = false;

	if (Prefixes == NULL)
		return false;

	for (unsigned int i = 0; i < strlen(Prefixes); i++) {
		if (flip) {
			if (Prefixes[i] == Char)
				return true;
		} else if (Prefixes[i] == ')')
			flip = true;
	}

	return false;
}

bool CIRCConnection::IsNickMode(char Char) {
	const char* Prefixes = GetISupport("PREFIX");

	while (*Prefixes != '\0' && *Prefixes != ')')
		if (*Prefixes == Char && *Prefixes != '(')
			return true;
		else
			Prefixes++;

	return false;
}

char CIRCConnection::PrefixForChanMode(char Mode) {
	const char* Prefixes = GetISupport("PREFIX");
	const char* pref = strstr(Prefixes, ")");

	Prefixes++;

	if (pref)
		pref++;
	else
		return '\0';

	while (*pref) {
		if (*Prefixes == Mode)
			return *pref;

		Prefixes++;
		pref++;
	}

	return '\0';
}

const char* CIRCConnection::GetServerVersion(void) {
	return m_ServerVersion;
}

const char* CIRCConnection::GetServerFeat(void) {
	return m_ServerFeat;
}

CBouncerConfig* CIRCConnection::GetISupportAll(void) {
	return m_ISupport;
}

void CIRCConnection::UpdateHostHelper(const char* Host) {
	char* Copy = strdup(Host);

	if (Copy == NULL) {
		LOGERROR("strdup() failed. Could not update hostmask. (%s)", Host);

		return;
	}

	const char* Nick = Copy;
	char* Site = strstr(Copy, "!");

	if (Site) {
		*Site = '\0';
		Site++;
	} else {
		free(Copy);
		return;
	}

	if (strcmpi(Nick, m_CurrentNick) == 0) {
		free(m_Site);

		m_Site = strdup(Site);

		if (m_Site == NULL) {
			LOGERROR("strdup() failed.");
		}
	}

	int i = 0;

	while (xhash_t<CChannel*>* Chan = m_Channels->Iterate(i++)) {
		if (Chan->Value->GetNames() == NULL) {
			free(Copy);

			return;
		}

		CNick* NickObj = Chan->Value->GetNames()->Get(Nick);

		if (NickObj && NickObj->GetSite() == NULL)
			NickObj->SetSite(Site);
	}

	free(Copy);
}

CFloodControl* CIRCConnection::GetFloodControl(void) {
	return m_FloodControl;
}

void CIRCConnection::InternalWriteLine(const char* In) {
	if (!m_Locked)
		m_QueueMiddle->QueueItem(In);
}

CQueue* CIRCConnection::GetQueueHigh(void) {
	return m_QueueHigh;
}

CQueue* CIRCConnection::GetQueueMiddle(void) {
	return m_QueueMiddle;
}

CQueue* CIRCConnection::GetQueueLow(void) {
	return m_QueueLow;
}

/* TODO: this is inconsistent with ircu .12 behaviour regarding keys. fix */
void CIRCConnection::JoinChannels(void) {
	if (m_DelayJoinTimer) {
		m_DelayJoinTimer->Destroy();
		m_DelayJoinTimer = NULL;
	}

	const char* Chans = GetOwningClient()->GetConfigChannels();

	if (Chans && *Chans) {
		char* dup, *newChanList, *tok, *ChanList = NULL;
		CKeyring* Keyring;

		dup = strdup(Chans);

		if (dup == NULL) {
			LOGERROR("strdup() failed. could not join channels (user %s)", m_Owner->GetUsername());

			return;
		}

		tok = strtok(dup, ",");

		Keyring = m_Owner->GetKeyring();

		while (tok) {
			const char* Key = Keyring->GetKey(tok);

			if (Key)
				WriteLine("JOIN %s %s", tok, Key);
			else {
				if (ChanList == NULL || strlen(ChanList) > 400) {
					if (ChanList != NULL) {
						WriteLine("JOIN %s", ChanList);
						free(ChanList);
					}

					ChanList = (char*)malloc(strlen(tok) + 1);
					strcpy(ChanList, tok);
				} else {
					newChanList = (char*)realloc(ChanList, strlen(ChanList) + 1 + strlen(tok) + 2);

					if (newChanList == NULL) {
						LOGERROR("realloc() failed. Could not join channel.");

						continue;
					}

					ChanList = newChanList;

					strcat(ChanList, ",");
					strcat(ChanList, tok);
				}
			}

			tok = strtok(NULL, ",");
		}

		if (ChanList) {
			WriteLine("JOIN %s", ChanList);
			free(ChanList);
		}

		free(dup);
	}
}

const char* CIRCConnection::ClassName(void) {
	return "CIRCConnection";
}

bool CIRCConnection::Read(void) {
	bool Ret = CConnection::Read();

	if (Ret && RecvqSize() > 5120) {
		Kill("RecvQ exceeded.");
	}

	return Ret;
}

bool DelayJoinTimer(time_t Now, void* IRCConnection) {
	((CIRCConnection*)IRCConnection)->m_DelayJoinTimer = NULL;
	((CIRCConnection*)IRCConnection)->JoinChannels();

	return false;
}

bool IRCPingTimer(time_t Now, void* IRCConnection) {
	if (((CIRCConnection*)IRCConnection)->m_Socket != INVALID_SOCKET)
		((CIRCConnection*)IRCConnection)->WriteLine("PING :sbnc");

	return true;
}

CHashtable<CChannel*, false, 16>* CIRCConnection::GetChannels(void) {
	return m_Channels;
}

void CIRCConnection::AsyncDnsFinished(adns_query* query, adns_answer* response) {
	free(m_AdnsQuery);
	m_AdnsQuery = NULL;

	if (response->status != adns_s_ok) {
		m_Owner->Notice("DNS request failed: No such hostname (NXDOMAIN).");
		g_Bouncer->Log("DNS request for %s failed. No such hostname (NXDOMAIN).", m_Owner->GetUsername());

		Destroy();

		return;
	} else {
		m_Socket = SocketAndConnectResolved(response->rrs.addr->addr.inet.sin_addr, m_PortCache, m_BindIpCache);
		free(m_BindIpCache);
		m_BindIpCache = NULL;

		g_Bouncer->RegisterSocket(m_Socket, (CSocketEvents*)this);

		InitSocket();

		m_AdnsTimeout->Destroy();
		m_AdnsTimeout = NULL;
	}
}

bool IRCAdnsTimeoutTimer(time_t Now, void* IRC) {
	((CIRCConnection*)IRC)->m_AdnsTimeout = NULL;

	((CIRCConnection*)IRC)->AdnsTimeout();

	return false;
}

void CIRCConnection::AdnsTimeout(void) {
	m_Owner->Notice("DNS request timed out. Could not connect to server.");
	g_Bouncer->Log("DNS request for %s timed out. Could not connect to server.", m_Owner->GetUsername());

	m_LatchedDestruction = true;

	if (m_AdnsQuery) {
		adns_cancel(*m_AdnsQuery);

		free(m_AdnsQuery);
		m_AdnsQuery = NULL;
	}
}

bool CIRCConnection::ShouldDestroy(void) {
	return m_LatchedDestruction;
}

const char* CIRCConnection::GetSite(void) {
	return m_Site;
}
