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

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIRCConnection::CIRCConnection(SOCKET Socket, CUser* Owning, bool SSL) : CConnection(Socket, SSL) {
	InitIrcConnection(Owning);
}

CIRCConnection::CIRCConnection(const char* Host, unsigned short Port, CUser* Owning, const char* BindIp, bool SSL, int Family) : CConnection(Host, Port, BindIp, SSL, Family) {
	InitIrcConnection(Owning);
}

CIRCConnection::CIRCConnection(SOCKET Socket, CAssocArray *Box, CUser *Owning) : CConnection(Socket, false) {
	InitIrcConnection(Owning, true);

	m_CurrentNick = strdup(Box->ReadString("irc.nick"));
	m_Server = strdup(Box->ReadString("irc.server"));

}

void CIRCConnection::InitIrcConnection(CUser* Owning, bool Unfreezing) {
	SetRole(Role_Client);

	m_LastResponse = time(NULL);
	g_LastReconnect = time(NULL);

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
		const char* Password = Owning->GetServerPassword();

		if (Password)
			WriteLine("PASS :%s", Password);

		WriteLine("NICK %s", Owning->GetNick());
		WriteLine("USER %s \"\" \"fnords\" :%s", Owning->GetUsername(), Owning->GetRealname());
	}

	m_Owner = Owning;

	m_Channels = new CHashtable<CChannel*, false, 16>();

	if (m_Channels == NULL) {
		LOGERROR("new operator failed. Could not create channel list.");

		g_Bouncer->Fatal();
	}

	m_Channels->RegisterValueDestructor(DestroyObject<CChannel>);

	m_Server = NULL;

	m_ServerVersion = NULL;
	m_ServerFeat = NULL;

	m_LastBurst = time(NULL);

	m_ISupport = new CConfig(NULL);

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

	m_PingTimer = g_Bouncer->CreateTimer(180, true, IRCPingTimer, this);
	m_DelayJoinTimer = NULL;

	m_Site = NULL;
}

CIRCConnection::~CIRCConnection() {
	free(m_CurrentNick);
	free(m_Site);

	if (m_Channels != NULL) {
		delete m_Channels;
	}

	free(m_Server);
	free(m_ServerVersion);
	free(m_ServerFeat);

	if (m_ISupport != NULL) {
		delete m_ISupport;
	}

	if (m_QueueLow != NULL) {
		delete m_QueueLow;
	}

	if (m_QueueMiddle != NULL) {
		delete m_QueueMiddle;
	}

	if (m_QueueHigh != NULL) {
		delete m_QueueHigh;
	}

	if (m_FloodControl != NULL) {
		delete m_FloodControl;
	}

	if (m_DelayJoinTimer != NULL) {
		m_DelayJoinTimer->Destroy();
	}

	if (m_PingTimer != NULL) {
		m_PingTimer->Destroy();
	}
}

bool CIRCConnection::ParseLineArgV(int argc, const char** argv) {
	m_LastResponse = time(NULL);

	if (argc < 2)
		return true;

	const char* Reply = argv[0];
	const char* Raw = argv[1];
	char* Nick = ::NickFromHostmask(Reply);
	int iRaw = atoi(Raw);

	bool b_Me = false;
	if (m_CurrentNick && Nick && strcasecmp(Nick, m_CurrentNick) == 0)
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

	if (argc > 3 && iRaw == 433) {
		bool Ret = ModuleEvent(argc, argv);

		if (GetCurrentNick() != NULL && GetOwner()->GetClientConnection() != NULL)
			return true;

		if (Ret)
			WriteLine("NICK :%s_", argv[3]);

		return Ret;
	} else if (argc > 3 && hashRaw == hashPrivmsg && !GetOwner()->GetClientConnection()) {
		const char* Host;
		const char* Dest = argv[2];
		char* Nick = ::NickFromHostmask(Reply);

		CChannel* Chan = GetChannel(Dest);

		if (Chan) {
			CNick* User = Chan->GetNames()->Get(Nick);

			if (User)
				User->SetIdleSince(time(NULL));
		}

		if (!ModuleEvent(argc, argv)) {
			free(Nick);
			return false;
		}

		if (argv[3][0] != '\1' && argv[3][strlen(argv[3]) - 1] != '\1' && Dest && Nick && m_CurrentNick && strcasecmp(Dest, m_CurrentNick) == 0 && strcasecmp(Nick, m_CurrentNick) != 0) {
			char* Dup;
			char* Delim;

			Dup = strdup(Reply);

			if (Dup == NULL) {
				LOGERROR("strdup() failed.");

				free(Nick);

				return true;
			}

			Delim = strstr(Dup, "!");

			if (Delim) {
				*Delim = '\0';

				Host = Delim + 1;
			}

			GetOwner()->Log("%s (%s): %s", Dup, Delim ? Host : "<unknown host>", argv[3]);

			free(Dup);
		}

		free(Nick);

		UpdateHostHelper(Reply);

		return true;
	} else if (argc > 3 && hashRaw == hashNotice && !GetOwner()->GetClientConnection()) {
		const char* Dest = argv[2];
		char* Nick;
		
		if (!ModuleEvent(argc, argv)) {
			return false;
		}

		Nick = ::NickFromHostmask(Reply);

		if (argv[3][0] != '\1' && argv[3][strlen(argv[3]) - 1] != '\1' && Dest && Nick && m_CurrentNick && strcasecmp(Dest, m_CurrentNick) == 0 && strcasecmp(Nick, m_CurrentNick) != 0) {
			GetOwner()->Log("%s (notice): %s", Reply, argv[3]);
		}

		free(Nick);

		return true;
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
				Nick = ::NickFromHostmask(Reply);

				if (Nick == NULL) {
					LOGERROR("NickFromHostmask() failed.");

					return false;
				}

				Chan->RemoveUser(Nick);

				free(Nick);
			}
		}

		UpdateHostHelper(Reply);

		return bRet;
	} else if (argc > 3 && hashRaw == hashKick) {
		bool bRet = ModuleEvent(argc, argv);

		if (m_CurrentNick && strcasecmp(argv[3], m_CurrentNick) == 0) {
			RemoveChannel(argv[2]);

			if (GetOwner()->GetClientConnection() == NULL) {
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

				GetOwner()->Log("%s (%s) kicked you from %s (%s)", Dup, Delim ? Host : "<unknown host>", argv[2], argc > 4 ? argv[4] : "");

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
		CClientConnection* Client = GetOwner()->GetClientConnection();

		if (Client) {
			if (strcmp(Client->GetNick(), argv[2]) != 0) {
				Client->WriteLine(":%s!ident@sbnc NICK :%s", Client->GetNick(), argv[2]);
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

		while (hash_t<CChannel*>* Chan = m_Channels->Iterate(i++)) {
			CHashtable<CNick*, false, 64>* Nicks = Chan->Value->GetNames();

			CNick* NickObj = Nicks->Get(Nick);

			if (NickObj) {
				Nicks->Remove(Nick, true);

				NickObj->SetNick(argv[2]);

				Nicks->Add(argv[2], NickObj);
			}
		}

		free(Nick);
	} else if (argc > 1 && hashRaw == hashQuit) {
		bool bRet = ModuleEvent(argc, argv);

		Nick = NickFromHostmask(argv[0]);

		int i = 0;

		while (hash_t<CChannel*>* Chan = m_Channels->Iterate(i++)) {
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
			CVector<CModule *> *Modules = g_Bouncer->GetModules();

			for (unsigned int i = 0; i < Modules->GetLength(); i++) {
				Modules->Get(i)->ServerLogon(m_Owner->GetUsername());
			}

			GetOwner()->Log("You were successfully connected to an IRC server.");

			g_Bouncer->Log("User %s connected to an IRC server.", GetOwner()->GetUsername());
		}

		if (GetOwner()->GetClientConnection() == NULL) {
			bool AppendTS = (GetOwner()->GetConfig()->ReadInteger("user.ts") != 0);
			const char* AwayReason = GetOwner()->GetAwayText();

			if (AwayReason)
				WriteLine(AppendTS ? "AWAY :%s (Away since the dawn of time)" : "AWAY :%s", AwayReason);
		}

		const char* AutoModes = GetOwner()->GetAutoModes();
		const char* DropModes = GetOwner()->GetDropModes();

		if (AutoModes && *AutoModes)
			WriteLine("MODE %s +%s", GetCurrentNick(), AutoModes);

		if (!GetOwner()->GetClientConnection() && DropModes && *DropModes)
			WriteLine("MODE %s -%s", GetCurrentNick(), DropModes);

		m_State = State_Connected;
	} else if (argc > 1 && strcasecmp(Reply, "ERROR") == 0) {
		if (strstr(Raw, "throttle") != NULL)
			GetOwner()->ScheduleReconnect(50);
		else
			GetOwner()->ScheduleReconnect(5);

		char* Out;

		asprintf(&Out, "Error received for %s: %s", GetOwner()->GetUsername(), argv[1]);

		if (Out == NULL) {
			LOGERROR("asprintf() failed.");

			return false;
		}

		g_Bouncer->GlobalNotice(Out, true);
		g_Bouncer->Log("%s", Out);

		if (!GetOwner()->GetClientConnection())
			GetOwner()->GetLog()->WriteLine("%s", Out);

		free(Out);
	} else if (argc > 3 && iRaw == 465) {
		char* Out;

		asprintf(&Out, "G/K-line reason for %s: %s", GetOwner()->GetUsername(), argv[3]);

		if (Out == NULL) {
			LOGERROR("asprintf() failed.");

			return false;
		}

		g_Bouncer->GlobalNotice(Out, true);
		g_Bouncer->Log("%s", Out);

		if (!GetOwner()->GetClientConnection())
			GetOwner()->GetLog()->WriteLine("%s", Out);

		free(Out);
	} else if (argc > 5 && iRaw == 351) {
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
	} else if (argc > 9 && iRaw == 352) {
		const char* Ident = argv[4];
		const char* Host = argv[5];
		const char* Server = argv[6];
		const char* Nick = argv[7];
		const char* Realname = argv[9];

		char* Mask = (char*)malloc(strlen(Nick) + 1 + strlen(Ident) + 1 + strlen(Host) + 1);

		snprintf(Mask, strlen(Nick) + 1 + strlen(Ident) + 1 + strlen(Host) + 1, "%s!%s@%s", Nick, Ident, Host);

		UpdateHostHelper(Mask);
		UpdateWhoHelper(Nick, Realname, Server);

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
	} else if (argc > 3 && hashRaw == hashPong && strcasecmp(argv[3], "sbnc") == 0) {
		return false;
	}

	if (m_Owner)
		return ModuleEvent(argc, argv);
	else
		return true;
}

bool CIRCConnection::ModuleEvent(int argc, const char** argv) {
	CVector<CModule *> *Modules = g_Bouncer->GetModules();

	for (unsigned int i = 0; i < Modules->GetLength(); i++) {
		if (!Modules->Get(i)->InterceptIRCMessage(this, argc, argv)) {
			return false;
		}
	}

	return true;
}

void CIRCConnection::ParseLine(const char* Line) {
	if (!GetOwner())
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
		if (strcasecmp(argv[0], "ping") == 0 && argc > 1) {
			WriteLine("PONG :%s", argv[1]);

			if (m_State != State_Connected)
				m_State = State_Pong;
		} else {
			CUser* User = GetOwner();

			if (User) {
				CClientConnection* Client = User->GetClientConnection();

				if (Client)
					Client->WriteUnformattedLine(Line);
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

CChannel *CIRCConnection::AddChannel(const char* Channel) {
	CChannel* ChannelObj = new CChannel(Channel, this);

	if (ChannelObj == NULL) {
		LOGERROR("new operator failed. could not add channel.");

		return NULL;
	}

	m_Channels->Add(Channel, ChannelObj);

	UpdateChannelConfig();

	return ChannelObj;
}

void CIRCConnection::RemoveChannel(const char* Channel) {
	m_Channels->Remove(Channel);

	UpdateChannelConfig();
}

const char* CIRCConnection::GetServer(void) {
	return m_Server;
}

void CIRCConnection::UpdateChannelConfig(void) {
	char* Out = NULL;

	int a = 0;

	while (hash_t<CChannel*>* Chan = m_Channels->Iterate(a++)) {
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

	while (hash_t<CChannel*>* Chan = m_Channels->Iterate(a++)) {
		if (strcasecmp(Chan->Name, Channel) == 0)
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
		return CConnection::HasQueuedData();
}

void CIRCConnection::Write(void) {
	char* Line = m_FloodControl->DequeueItem();

	if (Line != NULL)
		CConnection::WriteUnformattedLine(Line);

	CConnection::Write();

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
	const char *Prefixes = GetISupport("PREFIX");

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

CConfig* CIRCConnection::GetISupportAll(void) {
	return m_ISupport;
}

void CIRCConnection::UpdateWhoHelper(const char *Nick, const char *Realname, const char *Server) {
	int a = 0, i = 0;

	while (hash_t<CChannel*>* Chan = m_Channels->Iterate(a++)) {
		if (!Chan->Value->HasNames())
			return;

		CNick* NickObj = Chan->Value->GetNames()->Get(Nick);

		if (NickObj) {
			NickObj->SetRealname(Realname);
			NickObj->SetServer(Server);
		}
	}
}

void CIRCConnection::UpdateHostHelper(const char* Host) {
	const char *NickEnd;
	int Offset;
	char* Copy;

	NickEnd = strstr(Host, "!");

	if (NickEnd == NULL)
		return;

	Offset = NickEnd - Host;

	Copy = strdup(Host);

	if (Copy == NULL) {
		LOGERROR("strdup() failed. Could not update hostmask. (%s)", Host);

		return;
	}

	const char* Nick = Copy;
	char* Site = Copy + Offset;

	*Site = '\0';
	Site++;

	if (m_CurrentNick && strcasecmp(Nick, m_CurrentNick) == 0) {
		free(m_Site);

		m_Site = strdup(Site);

		if (m_Site == NULL) {
			LOGERROR("strdup() failed.");
		}
	}

	int i = 0;

	while (hash_t<CChannel*>* Chan = m_Channels->Iterate(i++)) {
		if (!Chan->Value->HasNames())
			continue;

		CNick* NickObj = Chan->Value->GetNames()->Get(Nick);

		if (NickObj && NickObj->GetSite() == NULL)
			NickObj->SetSite(Site);
	}

	free(Copy);
}

CFloodControl* CIRCConnection::GetFloodControl(void) {
	return m_FloodControl;
}

void CIRCConnection::WriteUnformattedLine(const char* In) {
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

void CIRCConnection::JoinChannels(void) {
	if (m_DelayJoinTimer) {
		m_DelayJoinTimer->Destroy();
		m_DelayJoinTimer = NULL;
	}

	const char* Chans = GetOwner()->GetConfigChannels();

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

const char* CIRCConnection::GetClassName(void) {
	return "CIRCConnection";
}

bool CIRCConnection::Read(void) {
	bool Ret = CConnection::Read();

	if (Ret && GetRecvqSize() > 5120) {
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
	CIRCConnection *IRC = (CIRCConnection *)IRCConnection;

	if (IRC->m_Socket == INVALID_SOCKET) {
		return true;
	}

	if (time(NULL) - IRC->m_LastResponse > 300) {
		IRC->WriteLine("PING :sbnc");

		if (time(NULL) - IRC->m_LastResponse > 600) {
			IRC->Kill("Server does not respond.");
		}
	}

	return true;
}

CHashtable<CChannel*, false, 16>* CIRCConnection::GetChannels(void) {
	return m_Channels;
}

const char* CIRCConnection::GetSite(void) {
	return m_Site;
}

int CIRCConnection::SSLVerify(int PreVerifyOk, X509_STORE_CTX* Context) {
#ifdef USESSL
	m_Owner->Notice(Context->cert->name);
#endif

	return 1;
}

void CIRCConnection::AsyncDnsFinished(hostent *Response) {
	if (Response == NULL) {
		m_Owner->Notice("DNS request failed: No such hostname (NXDOMAIN).");
		g_Bouncer->Log("DNS request for %s failed. No such hostname (NXDOMAIN).", m_Owner->GetUsername());
	}

	CConnection::AsyncDnsFinished(Response);
}

void CIRCConnection::AsyncBindIpDnsFinished(hostent *Response) {
	if (Response == NULL) {
		m_Owner->Notice("DNS request (vhost) failed: No such hostname (NXDOMAIN).");
		g_Bouncer->Log("DNS request (vhost) for %s failed. No such hostname (NXDOMAIN).", m_Owner->GetUsername());
	}

	CConnection::AsyncBindIpDnsFinished(Response);
}

void CIRCConnection::Destroy(void) {
	if (m_Owner != NULL) {
		m_Owner->SetIRCConnection(NULL);
	}

	delete this;
}

// TODO: persist version and other stuff
bool CIRCConnection::Freeze(CAssocArray *Box) {
	CAssocArray *QueueHighBox, *QueueMiddleBox, *QueueLowBox;
	if (m_CurrentNick == NULL || m_Server == NULL || GetSocket() == INVALID_SOCKET || IsSSL())
		return false;

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
	g_Bouncer->UnregisterSocket(m_Socket);
	m_Socket = INVALID_SOCKET;

	Destroy();

	return true;
}

CIRCConnection *CIRCConnection::Unfreeze(CAssocArray *Box, CUser *Owner) {
	SOCKET Socket;
	CIRCConnection *Connection;
	CAssocArray *TempBox;
	char *Out;
	const char *Temp;
	unsigned int Count;

	if (Box == NULL || Owner == NULL) {
		return NULL;
	}

	Socket = Box->ReadInteger("~irc.fd");

	Connection = new CIRCConnection(Socket, Owner, false);

	Connection->m_CurrentNick = strdup(Box->ReadString("~irc.nick"));
	Connection->m_Server = strdup(Box->ReadString("~irc.server"));

	Temp = Box->ReadString("~irc.serverversion");

	if (Temp != NULL){
		Connection->m_ServerVersion = strdup(Temp);
	}

	Temp = Box->ReadString("~irc.serverfeat");

	if (Temp != NULL) {
		Connection->m_ServerFeat = strdup(Temp);
	}

	TempBox = Box->ReadBox("~irc.isupport");

	if (Connection->m_ISupport != NULL) {
		delete Connection->m_ISupport;
	}

	Connection->m_ISupport = UnfreezeObject<CConfig>(Box, "~irc.isupport");

	Count = Box->ReadInteger("~irc.channels");

	Connection->WriteLine("VERSION");

	for (unsigned int i = 0; i < Count; i++) {
		CChannel *Channel;

		asprintf(&Out, "~irc.channel%d", i);

		CHECK_ALLOC_RESULT(Out, asprintf) {
			g_Bouncer->Fatal();
		} CHECK_ALLOC_RESULT_END;

		Channel = UnfreezeObject<CChannel>(Box, Out);
		Channel->SetOwner(Connection);

		Connection->m_Channels->Add(Channel->GetName(), Channel);

		free(Out);
	}

	if (Connection->m_FloodControl != NULL) {
		delete Connection->m_FloodControl;
	}

	Connection->m_FloodControl = new CFloodControl();

	TempBox = Box->ReadBox("~irc.queuehigh");

	if (TempBox != NULL) {
		if (Connection->m_QueueHigh != NULL) {
			delete Connection->m_QueueHigh;
		}

		Connection->m_QueueHigh = CQueue::Unfreeze(TempBox);
	}

	Connection->m_FloodControl->AttachInputQueue(Connection->m_QueueHigh, 0);

	TempBox = Box->ReadBox("~irc.queuemiddle");

	if (TempBox != NULL) {
		if (Connection->m_QueueMiddle != NULL) {
			delete Connection->m_QueueMiddle;
		}

		Connection->m_QueueMiddle = CQueue::Unfreeze(TempBox);
	}

	Connection->m_FloodControl->AttachInputQueue(Connection->m_QueueMiddle, 0);

	TempBox = Box->ReadBox("~irc.queuelow");

	if (TempBox != NULL) {
		if (Connection->m_QueueLow != NULL) {
			delete Connection->m_QueueLow;
		}

		Connection->m_QueueLow = CQueue::Unfreeze(TempBox);
	}

	Connection->m_FloodControl->AttachInputQueue(Connection->m_QueueLow, 0);

	return Connection;
}

void CIRCConnection::Kill(const char *Error) {
	if (m_Owner) {
		m_Owner->SetIRCConnection(NULL);
		m_Owner = NULL;
	}

	m_FloodControl->Clear();
	m_FloodControl->Disable();
	
	WriteLine("QUIT :%s", Error);

	CConnection::Kill(Error);
}

char CIRCConnection::GetHighestUserFlag(const char *Modes) {
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

		if (strchr(Modes, Prefixes[i])) {
			return Prefixes[i];
		}
	}

	return '\0';
}

void CIRCConnection::Error(void) {
	if (m_State == State_Connecting) {
		g_Bouncer->Log("An error occured while re-connecting for user %s", GetOwner()->GetUsername());

		GetOwner()->Notice("An error occured while re-connecting to a server.");
	}
}
