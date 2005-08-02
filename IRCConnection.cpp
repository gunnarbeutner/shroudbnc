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
#include "BouncerUser.h"
#include "BouncerConfig.h"
#include "BouncerLog.h"
#include "BouncerCore.h"
#include "ModuleFar.h"
#include "Module.h"
#include "Channel.h"
#include "Nick.h"
#include "Queue.h"
#include "FloodControl.h"
#include "TrafficStats.h"
#include "Keyring.h"
#include "Banlist.h"
#include "utility.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIRCConnection::CIRCConnection(SOCKET Socket, sockaddr_in Peer, CBouncerUser* Owning) : CConnection(Socket, Peer) {
	m_State = State_Connecting;

	m_CurrentNick = NULL;

	m_QueueHigh = new CQueue();
	m_QueueMiddle = new CQueue();
	m_QueueLow = new CQueue();
	m_FloodControl = new CFloodControl(this);

	const char* Password = Owning->GetConfig()->ReadString("user.spass");

	if (Password)
		WriteLine("PASS :%s", Password);

	WriteLine("NICK %s", Owning->GetNick());
	WriteLine("USER %s \"\" \"fnords\" :%s", Owning->GetUsername(), Owning->GetRealname());

	m_Owner = Owning;

	m_Channels = new CHashtable<CChannel*, false, 16>();
	m_Channels->RegisterValueDestructor(DestroyCChannel);

	m_Server = NULL;

	m_ServerVersion = NULL;
	m_ServerFeat = NULL;

	m_LastBurst = time(NULL);

	m_ISupport = new CBouncerConfig(NULL);

	m_ISupport->WriteString("CHANMODES", "bIe,k,l");
	m_ISupport->WriteString("CHANTYPES", "#&+");
	m_ISupport->WriteString("PREFIX", "(ov)@+");

	m_FloodControl->AttachInputQueue(m_QueueHigh, 0);
	m_FloodControl->AttachInputQueue(m_QueueMiddle, 1);
	m_FloodControl->AttachInputQueue(m_QueueLow, 2);

	g_Bouncer->RegisterSocket(Socket, (CSocketEvents*)this);

	m_PingTimer = g_Bouncer->CreateTimer(180, true, IRCPingTimer, this);
	m_DelayJoinTimer = NULL;
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

	m_PingTimer->Destroy();
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

	bool b_Me = false;
	if (m_CurrentNick && Nick && strcmpi(Nick, m_CurrentNick) == 0)
		b_Me = true;

	free(Nick);

	if (!GetOwningClient()->GetClientConnection() && atoi(Raw) == 433) {
		bool Ret = ModuleEvent(argc, argv);

		if (Ret)
			WriteLine("NICK :%s`", argv[3]);

		return Ret;
	} else if (argc > 3 && strcmpi(Raw, "privmsg") == 0 && !GetOwningClient()->GetClientConnection()) {
		const char* Dest = argv[2];
		char* Nick = ::NickFromHostmask(Reply);

		if (argv[3][0] != '\1' && argv[3][strlen(argv[3]) - 1] != '\1' && Dest && Nick && strcmpi(Dest, m_CurrentNick) == 0 && strcmpi(Nick, m_CurrentNick) != 0) {
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
	} else if (argc > 3 && strcmpi(Raw, "notice") == 0 && !GetOwningClient()->GetClientConnection()) {
		const char* Dest = argv[2];
		char* Nick = ::NickFromHostmask(Reply);

		if (argv[3][0] != '\1' && argv[3][strlen(argv[3]) - 1] != '\1' && Dest && Nick && strcmpi(Dest, m_CurrentNick) == 0 && strcmpi(Nick, m_CurrentNick) != 0) {
			char* Dup = strdup(Reply);

			char* Delim = strstr(Dup, "!");

			*Delim = '\0';

			const char* Host = Delim + 1;

			GetOwningClient()->Log("%s (notice): %s", Reply, argv[3]);

			free(Dup);
		}

		free(Nick);
	} else if (argc > 2 && strcmpi(Raw, "join") == 0) {
		if (b_Me) {
			AddChannel(argv[2]);

			if (!m_Owner->GetClientConnection())
				WriteLine("MODE %s", argv[2]);
		}

		CChannel* Chan = GetChannel(argv[2]);

		if (Chan) {
			Nick = NickFromHostmask(Reply);
			Chan->AddUser(Nick, '\0');
			free(Nick);
		}

		UpdateHostHelper(Reply);
	} else if (argc > 2 && strcmpi(Raw, "part") == 0) {
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
	} else if (argc > 2 && strcmpi(Raw, "kick") == 0) {
		bool bRet = ModuleEvent(argc, argv);

		if (strcmpi(argv[3], m_CurrentNick) == 0) {
			RemoveChannel(argv[2]);

			if (GetOwningClient()->GetClientConnection() == NULL) {
				char Out[1024];

				snprintf(Out, sizeof(Out), "JOIN %s", argv[2]);

				m_Owner->Simulate(Out);

				char* Dup = strdup(Reply);
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
	} else if (argc > 2 && atoi(Raw) == 1) {
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
	} else if (argc > 1 && strcmpi(Raw, "nick") == 0) {
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
	} else if (argc > 1 && strcmpi(Raw, "quit") == 0) {
		bool bRet = ModuleEvent(argc, argv);

		Nick = NickFromHostmask(argv[0]);

		int i = 0;

		while (xhash_t<CChannel*>* Chan = m_Channels->Iterate(i++)) {
			Chan->Value->GetNames()->Remove(Nick);
		}

		free(Nick);
		
		return bRet;
	} else if (argc > 1 && (atoi(Raw) == 422 || atoi(Raw) == 376)) {
		if (m_Owner->GetConfig()->ReadInteger("user.delayjoin"))
			m_DelayJoinTimer = g_Bouncer->CreateTimer(5, false, DelayJoinTimer, this);
		else
			JoinChannels();

		if (m_State != State_Connected) {
			for (int i = 0; i < g_Bouncer->GetModuleCount(); i++) {
				CModule* M = g_Bouncer->GetModules()[i];

				if (M) {
					M->ServerLogon(m_Owner->GetUsername());
				}
			}

			GetOwningClient()->Notice("Connected to an IRC server.");

			g_Bouncer->Log("Connected to an IRC server. (%s)", m_Owner->GetUsername());
		}

		if (GetOwningClient()->GetClientConnection() == NULL) {
			bool AppendTS = (GetOwningClient()->GetConfig()->ReadInteger("user.ts") != 0);
			const char* AwayReason = GetOwningClient()->GetConfig()->ReadString("user.away");

			if (AwayReason)
				WriteLine(AppendTS ? "AWAY :%s (Away since the dawn of time)" : "AWAY :%s", AwayReason);
		}

		m_State = State_Connected;
	} else if (argc > 1 && strcmpi(Reply, "error") == 0) {
		if (strstr(Raw, "throttle") != NULL)
			GetOwningClient()->ScheduleReconnect(50);
		else
			GetOwningClient()->ScheduleReconnect(5);

		char Out[1024];

		snprintf(Out, sizeof(Out), "Error received for %s: %s", GetOwningClient()->GetUsername(), argv[1]);

		g_Bouncer->GlobalNotice(Out, true);
		g_Bouncer->Log("%s", Out);

		if (!GetOwningClient()->GetClientConnection())
			GetOwningClient()->GetLog()->InternalWriteLine(Out);
	} else if (argc > 3 && atoi(Raw) == 465) {
		char Out[1024];

		snprintf(Out, sizeof(Out), "G/K-line reason for %s: %s", GetOwningClient()->GetUsername(), argv[3]);

		g_Bouncer->GlobalNotice(Out, true);
		g_Bouncer->Log("%s", Out);

		if (!GetOwningClient()->GetClientConnection())
			GetOwningClient()->GetLog()->InternalWriteLine(Out);
	} else if (argc > 3 && atoi(Raw) == 351) {
		free(m_ServerVersion);
		m_ServerVersion = strdup(argv[3]);

		free(m_ServerFeat);
		m_ServerFeat = strdup(argv[5]);
	} else if (argc > 3 && atoi(Raw) == 5) {
		for (int i = 3; i < argc - 1; i++) {
			char* Dup = strdup(argv[i]);
			char* Eq = strstr(Dup, "=");

			if (Eq) {
				*Eq = '\0';

				m_ISupport->WriteString(Dup, ++Eq);
			} else
				m_ISupport->WriteString(Dup, "");

			free(Dup);
		}
	} else if (argc > 4 && atoi(Raw) == 324) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan) {
			Chan->ClearModes();
			Chan->ParseModeChange(argv[0], argv[4], argc - 5, &argv[5]);
			Chan->SetModesValid(true);
		}
	} else if (argc > 3 && strcmpi(Raw, "mode") == 0) {
		CChannel* Chan = GetChannel(argv[2]);

		if (Chan)
			Chan->ParseModeChange(argv[0], argv[3], argc - 4, &argv[4]);

		UpdateHostHelper(Reply);
	} else if (argc > 4 && atoi(Raw) == 329) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan)
			Chan->SetCreationTime(atoi(argv[4]));
	} else if (argc > 4 && atoi(Raw) == 332) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan)
			Chan->SetTopic(argv[4]);
	} else if (argc > 5 && atoi(Raw) == 333) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan) {
			Chan->SetTopicNick(argv[4]);
			Chan->SetTopicStamp(atoi(argv[5]));
		}
	} else if (argc > 3 && atoi(Raw) == 331) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan) {
			Chan->SetNoTopic();
		}
	} else if (argc > 3 && strcmpi(Raw, "topic") == 0) {
		CChannel* Chan = GetChannel(argv[2]);

		if (Chan) {
			Chan->SetTopic(argv[3]);
			Chan->SetTopicStamp(time(NULL));
			Chan->SetTopicNick(argv[0]);
		}

		UpdateHostHelper(Reply);
	} else if (argc > 5 && atoi(Raw) == 353) {
		CChannel* Chan = GetChannel(argv[4]);

		const char* nicks = ArgTokenize(argv[5]);
		const char** nickv = ArgToArray(nicks);
		int nickc = ArgCount(nicks);

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
	} else if (argc > 3 && atoi(Raw) == 366) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan)
			Chan->SetHasNames();
	} else if (argc > 7 && atoi(Raw) == 352) {
		const char* Ident = argv[4];
		const char* Host = argv[5];
		const char* Nick = argv[7];

		char* Mask = (char*)malloc(strlen(Nick) + 1 + strlen(Ident) + 1 + strlen(Host) + 1);

		snprintf(Mask, strlen(Nick) + 1 + strlen(Ident) + 1 + strlen(Host) + 1, "%s!%s@%s", Nick, Ident, Host);

		UpdateHostHelper(Mask);

		free(Mask);
	} else if (argc > 6 && atoi(Raw) == 367) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan)
			Chan->GetBanlist()->SetBan(argv[4], argv[5], atoi(argv[6]));
	} else if (argc > 1 && atoi(Raw) == 368) {
		CChannel* Chan = GetChannel(argv[3]);

		if (Chan)
			Chan->SetHasBans();
	} else if (argc > 3 && strcmpi(Raw, "PONG") == 0 && strcmpi(argv[3], "sbnc") == 0) {
		return false;
	}

	return ModuleEvent(argc, argv);
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

	const char** argv = ArgToArray(Args);
	int argc = ArgCount(Args);

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
	m_Channels->Add(Channel, new CChannel(Channel, this));

/*	for (int i = 0; i < m_ChannelCount; i++) {
		if (m_Channels[i] == NULL) {
			m_Channels[i] = new CChannel(Channel, this);

			UpdateChannelConfig();

			return;
		}
	}

	m_Channels = (CChannel**)realloc(m_Channels, ++m_ChannelCount * sizeof(CChannel*));
	m_Channels[m_ChannelCount - 1] = new CChannel(Channel, this);*/

	UpdateChannelConfig();
}

void CIRCConnection::RemoveChannel(const char* Channel) {
	m_Channels->Remove(Channel);

/*	for (int i = 0; i < m_ChannelCount; i++) {
		if (m_Channels[i] && strcmpi(m_Channels[i]->GetName(), Channel) == 0) {
			delete m_Channels[i];
			m_Channels[i] = NULL;
		}
	}*/

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

		if (!WasNull)
			strcat(Out, ",");
		else
			Out[0] = '\0';

		strcat(Out, Chan->Name);
	}

	m_Owner->GetConfig()->WriteString("user.channels", Out);

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

	for (unsigned int i = 0; i < strlen(Modes); i++) {
		if (Modes[i] == ',')
			Modes[i] = ' ';
	}

	const char* Args = ArgTokenize(Modes);

	free(Modes);

	const char** argv = ArgToArray(Args);
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
	char* pref = strstr(Prefixes, ")");

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
	const char* Nick = Copy;
	char* Site = strstr(Copy, "!");

	if (Site) {
		*Site = '\0';
		Site++;
	} else {
		free(Copy);
		return;
	}

	int i = 0;

	while (xhash_t<CChannel*>* Chan = m_Channels->Iterate(i++)) {
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

void CIRCConnection::JoinChannels(void) {
	if (m_DelayJoinTimer) {
		m_DelayJoinTimer->Destroy();
		m_DelayJoinTimer = NULL;
	}

	const char* Chans = GetOwningClient()->GetConfig()->ReadString("user.channels");

	if (Chans && *Chans) {
		char* dup = strdup(Chans);
		char* tok = strtok(dup, ",");
		char* Keys = NULL;
		CKeyring* Keyring = m_Owner->GetKeyring();

		while (tok) {
			const char* Key = Keyring->GetKey(tok);

			if (Key) {
				bool Valid = (Keys != NULL);

				Keys = (char*)realloc(Keys, (Keys ? strlen(Keys) : 0) + strlen(Key) + 2);
				
				if (Valid)
					strcat(Keys, ",");
				else
					*Keys = '\0';

				strcat(Keys, Key);
			}

			tok = strtok(NULL, ",");
		}

		free(dup);

		WriteLine(Keys ? "JOIN %s %s" : "JOIN %s", Chans, Keys);
	}
}

const char* CIRCConnection::ClassName(void) {
	return "CIRCConnection";
}

bool CIRCConnection::Read(void) {
	bool Ret = CConnection::Read();

	if (Ret && recvq_size > 5120) {
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
	((CIRCConnection*)IRCConnection)->WriteLine("PING :sbnc");

	return true;
}

CHashtable<CChannel*, false, 16>* CIRCConnection::GetChannels(void) {
	return m_Channels;
}
