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
#include "tickle.h"
#include "tickleProcs.h"
#include "../ModuleFar.h"
#include "../BouncerCore.h"
#include "../SocketEvents.h"
#include "../Connection.h"
#include "../IRCConnection.h"
#include "../ClientConnection.h"
#include "../Channel.h"
#include "../BouncerUser.h"
#include "../BouncerConfig.h"
#include "../Nick.h"

static char* g_Context = NULL;

binding_t* g_Binds = NULL;
int g_BindCount = 0;

extern "C" int Bnc_Init(Tcl_Interp*);

int Tcl_ProcInit(Tcl_Interp* interp) {
	return Bnc_Init(interp);
}

extern const char* getuser(const char* Option) {
	CBouncerUser* User = g_Bouncer->GetUser(g_Context);

	if (!User)
		return NULL;
	else
		return User->GetConfig()->ReadString(Option);
}

extern int setuser(const char* Option, const char* Value) {
	CBouncerUser* User = g_Bouncer->GetUser(g_Context);

	if (!User)
		return 0;
	else {		
		User->GetConfig()->WriteString(Option, Value);

		return 1;
	}
}

extern void die(void) {
	g_Bouncer->Shutdown();
}

extern void setctx(const char* ctx) {
	free(g_Context);
	g_Context = ctx ? strdup(ctx) : NULL;
}

extern const char* getctx(void) {
	return g_Context;
}

extern const char* users(void) {
	static char* UserList = NULL;

	UserList = (char*)realloc(UserList, 1);
	UserList[0] = '\0';

	int Count = g_Bouncer->GetUserCount();
	CBouncerUser** Users = g_Bouncer->GetUsers();

	for (int i = 0; i < Count; i++) {
		if (Users[i]) {
			const char* User = Users[i]->GetUsername();
			UserList = (char*)realloc(UserList, strlen(UserList) + strlen(User) + 4);
			if (*UserList)
				strcat(UserList, " ");

			strcat(UserList, "{");
			strcat(UserList, User);
			strcat(UserList, "}");
		}
	}

	return UserList;
}

extern const char* channels(void) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return NULL;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;

	static char* ChanList = NULL;

	ChanList = (char*)realloc(ChanList, 1);
	ChanList[0] = '\0';

	int Count = IRC->GetChannelCount();
	CChannel** Channels = IRC->GetChannels();

	for (int i = 0; i < Count; i++) {
		if (Channels[i]) {
			const char* Channel = Channels[i]->GetName();
			ChanList = (char*)realloc(ChanList, strlen(ChanList) + strlen(Channel) + 4);
			if (*ChanList)
				strcat(ChanList, " ");

			strcat(ChanList, "{");
			strcat(ChanList, Channel);
			strcat(ChanList, "}");
		}
	}

	return ChanList;
}

extern const char* gettopic(const char* Channel) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return NULL;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;


	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return NULL;
	else
		return Chan->GetTopic();
}

extern const char* getchanmodes(const char* Channel) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return NULL;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;


	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return NULL;
	else
		return Chan->GetChanModes();
}

extern void rehash(void) {
	RehashInterpreter();

	g_Bouncer->GlobalNotice("Rehashing TCL module", true);
}


int ticklebind(const char* type, const char* proc) {
	for (int i = 0; i < g_BindCount; i++) {
		if (strcmp(g_Binds[i].proc, proc) == 0)
			return 0;
	}


	g_Binds = (binding_s*)realloc(g_Binds, sizeof(binding_t) * ++g_BindCount);

	int n = g_BindCount - 1;

	if (strcmpi(type, "client") == 0)
		g_Binds[n].type = Type_Client;
	else if (strcmpi(type, "server") == 0)
		g_Binds[n].type = Type_Server;
	else if (strcmpi(type, "pulse") == 0)
		g_Binds[n].type = Type_Pulse;
	else
		g_Binds[n].type = Type_Invalid;

	g_Binds[n].proc = strdup(proc);

	return 1;
}

extern int unbind(const char* type, const char* proc) {
	return 1;
}

extern int putserv(const char* text) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return 0;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	IRC->InternalWriteLine(text);

	return 1;
}

extern int putclient(const char* text) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return 0;

	CClientConnection* Client = Context->GetClientConnection();

	if (!Client)
		return 0;

	Client->InternalWriteLine(text);

	return 1;
}

extern void jump(void) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return;

	Context->Reconnect();
	SetLatchedReturnValue(false);
}

extern const char* channel(const char* Function, const char* Channel, const char* Parameter) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return NULL;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return "-1";

	CChannel* Chan = IRC->GetChannel(Channel);

	if (strcmpi(Function, "ison") == 0) {
		if (!Parameter) {
			if (Chan)
				return "1";
			else
				return "0";
		} else {
			if (!Chan)
				return "-1";
			else {
				CNick* Nick = Chan->GetNames()->Get(Parameter);

				if (Nick)
					return "1";
				else
					return "0";
			}
		}
	}

	if (strcmpi(Function, "join") == 0) {
		IRC->WriteLine("JOIN :%s", Channel);
		return "1";
	} else if (strcmpi(Function, "part") == 0) {
		IRC->WriteLine("PART :%s", Channel);
		return "1";
	} else if (strcmpi(Function, "mode") == 0) {
		IRC->WriteLine("MODE %s %s", Channel, Parameter);
		return "1";
	}

	if (!Chan)
		return "-1";

	if (strcmpi(Function, "topic") == 0)
		return Chan->GetTopic();

	if (strcmpi(Function, "topicnick") == 0)
		return Chan->GetTopicNick();

	static char* Buffer = NULL;

	if (strcmpi(Function, "topicstamp") == 0) {
		Buffer = (char*)realloc(Buffer, 20);

		sprintf(Buffer, "%d", Chan->GetTopicStamp());

		return Buffer;
	}

	if (strcmpi(Function, "hastopic") == 0)
		if (Chan->HasTopic())
			return "1";
		else
			return "0";

	if (strcmpi(Function, "hasnames") == 0)
		if (Chan->HasNames())
			return "1";
		else
			return "0";

	if (strcmpi(Function, "modes") == 0)
		return Chan->GetChanModes();

	if (strcmpi(Function, "nicks") == 0) {
		static char* NickList = NULL;

		NickList = (char*)realloc(NickList, 1);
		NickList[0] = '\0';

		CHashtable<CNick*, false>* Names = Chan->GetNames();

		int a = 0;

		while (hash_t<CNick*>* NickHash = Names->Iterate(a++)) {
			const char* Nick = NickHash->Value->GetNick();
			NickList = (char*)realloc(NickList, strlen(NickList) + strlen(Nick) + 4);
			if (*NickList)
				strcat(NickList, " ");

			strcat(NickList, "{");
			strcat(NickList, Nick);
			strcat(NickList, "}");
		}

		return NickList;
	}

	if (strcmpi(Function, "prefix") == 0) {
		CNick* Nick = Chan->GetNames()->Get(Parameter);

		static char outPref[2];

		if (Nick) {
			outPref[0] = Chan->GetHighestUserFlag(Nick->GetPrefixes());
			outPref[1] = '\0';

			return outPref;
		} else
			return "-1";
	}

	return "Function should be one of: ison join part mode topic topicnick topicstamp hastopic hasnames modes nicks prefix";
}

extern const char* user(const char* Function, const char* User, const char* Parameter, const char* Parameter2) {
	static char Buffer[1024];

	CBouncerUser* Context;

	if (User)
		Context = g_Bouncer->GetUser(User);
	else
		Context = g_Bouncer->GetUser(g_Context);

	if (strcmpi(Function, "isvalid") == 0)
		return Context ? "1" : "0";

	if (strcmpi(Function, "add") == 0) {
		if (Context)
			return "0";
		else {
			g_Bouncer->CreateUser(User, Parameter);

			return "1";
		}
	}

	if (strcmpi(Function, "remove") == 0) {
		if (Context) {
			g_Bouncer->RemoveUser(User);
			return "0";
		} else
			return "-1";
	}

	if (!Context)
		return "-1";

	if (strcmpi(Function, "server") == 0) {
		if (!Parameter)
			return Context->GetServer();
		else {
			Context->SetServer(Parameter);

			return "1";
		}
	}

	if (strcmpi(Function, "port") == 0) {
		if (!Parameter) {
			sprintf(Buffer, "%d", Context->GetPort());

			return Buffer;
		} else {
			Context->SetPort(atoi(Parameter));

			return "1";
		}
	}

	if (strcmpi(Function, "realname") == 0)
		if (!Parameter)
			return Context->GetRealname();
		else {
			Context->SetRealname(Parameter);

			return "1";
		}
	if (strcmpi(Function, "uptime") == 0) {
		sprintf(Buffer, "%d", Context->IRCUptime());

		return Buffer;
	}

	if (strcmpi(Function, "lock") == 0) {
		if (!Parameter) {
			sprintf(Buffer, "%d", Context->IsLocked());

			return Buffer;
		} else {
			if (atoi(Parameter))
				Context->Lock();
			else
				Context->Unlock();

			return "1";
		}
	}

	if (strcmpi(Function, "admin") == 0) {
		if (!Parameter) {
			sprintf(Buffer, "%d", Context->IsAdmin());

			return Buffer;
		} else {
			if (atoi(Parameter))
				Context->SetAdmin(true);
			else
				Context->SetAdmin(false);

			return "1";
		}
	}

	if (strcmpi(Function, "hasserver") == 0)
		return Context->GetIRCConnection() ? "1" : "0";

	if (strcmpi(Function, "hasclient") == 0)
		return Context->GetClientConnection() ? "1" : "0";

	if (strcmpi(Function, "nick") == 0) {
		if (!Parameter)
			return Context->GetNick();
		else {
			CIRCConnection* IRC = Context->GetIRCConnection();

			IRC->WriteLine("NICK :%s", Parameter);

			return "1";
		}
	}

	if (strcmpi(Function, "set") == 0) {
		if (!Parameter)
			return "-1";
		else {
			Context->GetConfig()->WriteString(Parameter, Parameter2);
			return "1";
		}
	}

	if (strcmpi(Function, "get") == 0) {
		if (!Parameter)
			return "-1";
		else
			return Context->GetConfig()->ReadString(Parameter);
	}

	if (strcmpi(Function, "log") == 0) {
		Context->Log("%s", Parameter);

		return "1";
	}

	if (strcmpi(Function, "client") == 0) {
		CClientConnection* Client = Context->GetClientConnection();

		if (!Client)
			return NULL;
		else {
			SOCKET Sock = Client->GetSocket();

			sockaddr_in saddr;
			socklen_t saddr_size = sizeof(saddr);

			getpeername(Sock, (sockaddr*)&saddr, &saddr_size);

			hostent* hent = gethostbyaddr((const char*)&saddr.sin_addr, sizeof(in_addr), AF_INET);

			if (hent)
				return hent->h_name;
			else
				return "<unknown>";
		}
	}

	return "Function should be one of: add remove server port realname uptime lock admin hasserver hasclient nick set get log";
}

extern int simul(const char* User, const char* Command) {
	CBouncerUser* Context;

	Context = g_Bouncer->GetUser(User);

	if (!Context)
		return -1;
	else {
		Context->Simulate(Command);

		return 1;
	}
}

extern const char* gethost(const char* Nick) {
	CBouncerUser* Context;

	Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return NULL;
	else {
		CIRCConnection* IRC = Context->GetIRCConnection();

		if (IRC) {
			CChannel** Chans = IRC->GetChannels();
			int ChanCount = IRC->GetChannelCount();

			for (int i = 0; i < ChanCount; i++) {
				if (Chans[i]) {
					CNick* U = Chans[i]->GetNames()->Get(Nick);

					if (U && U->GetSite() != NULL)
						return U->GetSite();
				}
					
			}
		}

		return NULL;
	}

}