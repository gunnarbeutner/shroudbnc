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
#include "../StdAfx.h"

#include "tickle.h"
#include "tickleProcs.h"
#include "TclSocket.h"
#include "TclClientSocket.h"

static char* g_Context = NULL;

binding_t* g_Binds = NULL;
int g_BindCount = 0;

tcltimer_t** g_Timers = NULL;
int g_TimerCount = 0;

extern Tcl_Encoding g_Encoding;

extern Tcl_Interp* g_Interp;

extern bool g_Ret;
extern bool g_NoticeUser;

extern "C" int Bnc_Init(Tcl_Interp*);

int Tcl_ProcInit(Tcl_Interp* interp) {
	return Bnc_Init(interp);
}

void die(void) {
	g_Bouncer->Shutdown();
}

void setctx(const char* ctx) {
	free(g_Context);

	g_Context = ctx ? strdup(ctx) : NULL;
}

const char* getctx(void) {
	return g_Context;
}

const char* bncuserlist(void) {
	int Count = g_Bouncer->GetUserCount();
	CBouncerUser** Users = g_Bouncer->GetUsers();

	int argc = 0;
	const char** argv = (const char**)malloc(Count * sizeof(const char*));

	for (int i = 0; i < Count; i++) {
		if (Users[i])
			argv[argc++] = Users[i]->GetUsername();
	}

	static char* List = NULL;

	if (List)
		Tcl_Free(List);

	List = Tcl_Merge(argc, argv);

	free(argv);

	return List;
}

const char* internalchannels(void) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return NULL;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;

	CHashtable<CChannel*, false, 16>* H = IRC->GetChannels();

	if (H == NULL)
		return NULL;

	int Count = Count = H->Count();

	const char** argv = (const char**)malloc(Count * sizeof(const char*));

	int a = 0;

	while (xhash_t<CChannel*>* Chan = H->Iterate(a)) {
		argv[a] = Chan->Name;
		a++;
	}

	static char* List = NULL;

	if (List)
		Tcl_Free(List);

	List = Tcl_Merge(Count, argv);

	free(argv);

	return List;
}

const char* getchanmode(const char* Channel) {
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

void rehash(void) {
	RehashInterpreter();

	g_Bouncer->GlobalNotice("Rehashing TCL module", true);
}

int internalbind(const char* type, const char* proc, const char* pattern, const char* user) {
	for (int i = 0; i < g_BindCount; i++) {
		if (g_Binds[i].valid && strcmp(g_Binds[i].proc, proc) == 0
			&& ((!pattern && g_Binds[i].pattern == NULL) || (pattern && g_Binds[i].pattern && strcmp(pattern, g_Binds[i].pattern) == 0))
			&& ((!user && g_Binds[i].user == NULL) || (user && g_Binds[i].user && strcmpi(user, g_Binds[i].user) == 0)))

			return 0;
	}

	binding_t* Bind = NULL;

	for (int a = 0; a < g_BindCount; a++) {
		if (!g_Binds[a].valid) {
			Bind = &g_Binds[a];

			break;
		}
	}

	if (Bind == NULL) {
		g_Binds = (binding_s*)realloc(g_Binds, sizeof(binding_t) * ++g_BindCount);

		Bind = &g_Binds[g_BindCount - 1];
	}

	Bind->valid = false;

	if (strcmpi(type, "client") == 0)
		Bind->type = Type_Client;
	else if (strcmpi(type, "server") == 0)
		Bind->type = Type_Server;
	else if (strcmpi(type, "pre") == 0)
		Bind->type = Type_PreScript;
	else if (strcmpi(type, "post") == 0)
		Bind->type = Type_PostScript;
	else if (strcmpi(type, "attach") == 0)
		Bind->type = Type_Attach;
	else if (strcmpi(type, "detach") == 0)
		Bind->type = Type_Detach;
	else if (strcmpi(type, "modec") == 0)
		Bind->type = Type_SingleMode;
	else if (strcmpi(type, "unload") == 0)
		Bind->type = Type_Unload;
	else if (strcmpi(type, "svrdisconnect") == 0)
		Bind->type = Type_SvrDisconnect;
	else if (strcmpi(type, "svrconnect") == 0)
		Bind->type = Type_SvrConnect;
	else if (strcmpi(type, "svrlogon") == 0)
		Bind->type = Type_SvrLogon;
	else if (strcmpi(type, "usrload") == 0)
		Bind->type = Type_UsrLoad;
	else if (strcmpi(type, "usrcreate") == 0)
		Bind->type = Type_UsrCreate;
	else if (strcmpi(type, "usrdelete") == 0)
		Bind->type = Type_UsrDelete;
	else if (strcmpi(type, "command") == 0)
		Bind->type = Type_Command;
	else {
		Bind->type = Type_Invalid;

		throw "Invalid bind type.";
	}

	Bind->proc = strdup(proc);
	Bind->valid = true;

	if (pattern)
		Bind->pattern = strdup(pattern);
	else
		Bind->pattern = NULL;

	if (user)
		Bind->user = strdup(user);
	else
		Bind->user = NULL;

	return 1;
}

int internalunbind(const char* type, const char* proc, const char* pattern, const char* user) {
	binding_type_e bindtype;

	if (strcmpi(type, "client") == 0)
		bindtype = Type_Client;
	else if (strcmpi(type, "server") == 0)
		bindtype = Type_Server;
	else if (strcmpi(type, "pre") == 0)
		bindtype = Type_PreScript;
	else if (strcmpi(type, "post") == 0)
		bindtype = Type_PostScript;
	else if (strcmpi(type, "attach") == 0)
		bindtype = Type_Attach;
	else if (strcmpi(type, "detach") == 0)
		bindtype = Type_Detach;
	else if (strcmpi(type, "modec") == 0)
		bindtype = Type_SingleMode;
	else if (strcmpi(type, "unload") == 0)
		bindtype = Type_Unload;
	else if (strcmpi(type, "svrdisconnect") == 0)
		bindtype = Type_SvrDisconnect;
	else if (strcmpi(type, "svrconnect") == 0)
		bindtype = Type_SvrConnect;
	else if (strcmpi(type, "svrlogon") == 0)
		bindtype = Type_SvrLogon;
	else if (strcmpi(type, "usrload") == 0)
		bindtype = Type_UsrLoad;
	else if (strcmpi(type, "usrcreate") == 0)
		bindtype = Type_UsrCreate;
	else if (strcmpi(type, "usrdelete") == 0)
		bindtype = Type_UsrDelete;
	else if (strcmpi(type, "command") == 0)
		bindtype = Type_Command;
	else
		return 0;

	for (int i = 0; i < g_BindCount; i++) {
		if (g_Binds[i].valid && g_Binds[i].type == bindtype && strcmp(g_Binds[i].proc, proc) == 0
			&& ((!pattern && g_Binds[i].pattern == NULL) || (pattern && g_Binds[i].pattern && strcmp(pattern, g_Binds[i].pattern) == 0))
			&& ((!user && g_Binds[i].user == NULL) || (user && g_Binds[i].user && strcmpi(user, g_Binds[i].user) == 0))) {

			free(g_Binds[i].proc);
			free(g_Binds[i].pattern);
			free(g_Binds[i].user);
			g_Binds[i].valid = false;
		}
	}

	return 1;
}

const char* internalbinds(void) {
	char** List = (char**)malloc(g_BindCount * sizeof(char*));
	int n = 0;

	for (int i = 0; i < g_BindCount; i++) {
		if (g_Binds[i].valid) {
			const char* Bind[4];

			binding_type_e type = g_Binds[i].type;

			if (type == Type_Client)
				Bind[0] = "client";
			else if (type == Type_Server)
				Bind[0] = "server";
			else if (type == Type_PreScript)
				Bind[0] = "pre";
			else if (type == Type_PostScript)
				Bind[0] = "post";
			else if (type == Type_Attach)
				Bind[0] = "attach";
			else if (type == Type_Detach)
				Bind[0] = "detach";
			else if (type == Type_SingleMode)
				Bind[0] = "modec";
			else if (type == Type_Unload)
				Bind[0] = "unload";
			else if (type == Type_SvrDisconnect)
				Bind[0] = "svrdisconnect";
			else if (type == Type_SvrConnect)
				Bind[0] = "svrconnect";
			else if (type == Type_SvrLogon)
				Bind[0] = "svrlogon";
			else if (type == Type_UsrLoad)
				Bind[0] = "usrload";
			else if (type == Type_UsrCreate)
				Bind[0] = "usrcreate";
			else if (type == Type_UsrDelete)
				Bind[0] = "usrdelete";
			else if (type == Type_Command)
				Bind[0] = "command";
			else
				Bind[0] = "invalid";

			Bind[1] = g_Binds[i].proc;
			Bind[2] = g_Binds[i].pattern ? g_Binds[i].pattern : "*";
			Bind[3] = g_Binds[i].user ? g_Binds[i].user : "*";

			char* Item = Tcl_Merge(4, Bind);

			List[n++] = Item;
		}
	}

	static char* Out = NULL;

	if (Out)
		Tcl_Free(Out);

	Out = Tcl_Merge(n, List);

	for (int a = 0; a < n; a++)
		Tcl_Free(List[a]);

	return Out;
}

int putserv(const char* text) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return 0;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	IRC->InternalWriteLine(text);

	return 1;
}

int putclient(const char* text) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return 0;

	CClientConnection* Client = Context->GetClientConnection();

	if (!Client)
		return 0;

	Client->InternalWriteLine(text);

	return 1;
}

void jump(void) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return;

	Context->Reconnect();
	SetLatchedReturnValue(false);
}

bool onchan(const char* Nick, const char* Channel) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return false;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return false;

	if (Channel) {
		CChannel* Chan = IRC->GetChannel(Channel);

		if (Chan && Chan->GetNames()->Get(Nick))
			return true;
		else
			return false;;
	} else {
		int a = 0;

		while (xhash_t<CChannel*>* Chan = IRC->GetChannels()->Iterate(a++)) {
			if (Chan->Value->GetNames()->Get(Nick)) {
				return true;
			}
		}

		return false;
	}
}

const char* topic(const char* Channel) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return NULL;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return NULL;

	return Chan->GetTopic();
}

const char* topicnick(const char* Channel) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return NULL;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return NULL;

	return Chan->GetTopicNick();
}

int topicstamp(const char* Channel) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return 0;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return 0;

	return Chan->GetTopicStamp();
}

const char* internalchanlist(const char* Channel) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return NULL;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return NULL;

	CHashtable<CNick*, false, 64, true>* Names = Chan->GetNames();

	int Count = Names->Count();
	const char** argv = (const char**)malloc(Count * sizeof(const char*));

	int a = 0;

	while (xhash_t<CNick*>* NickHash = Names->Iterate(a++)) {
		argv[a-1] = NickHash->Name;
	}

	static char* List = NULL;

	if (List)
		Tcl_Free(List);

	List = Tcl_Merge(Count, argv);

	free(argv);

	return List;
}

bool isop(const char* Nick, const char* Channel) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return false;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return false;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (Chan) {
		CNick* User = Chan->GetNames()->Get(Nick);

		if (User)
			return User->IsOp();
		else
			return false;
	} else {
		int a = 0;

		while (xhash_t<CChannel*>* Chan = IRC->GetChannels()->Iterate(a++)) {
			if (Chan->Value->GetNames()->Get(Nick) && Chan->Value->GetNames()->Get(Nick)->IsOp()) {
				return true;
			}
		}

		return false;
	}
}

bool isvoice(const char* Nick, const char* Channel) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return false;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return false;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (Chan) {
		CNick* User = Chan->GetNames()->Get(Nick);

		if (User)
			return User->IsVoice();
		else
			return false;
	} else {
		int a = 0;

		while (xhash_t<CChannel*>* Chan = IRC->GetChannels()->Iterate(a++)) {
			if (Chan->Value->GetNames()->Get(Nick) && Chan->Value->GetNames()->Get(Nick)->IsVoice()) {
				return true;
			}
		}

		return false;
	}
}

bool ishalfop(const char* Nick, const char* Channel) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return false;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return false;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (Chan) {
		CNick* User = Chan->GetNames()->Get(Nick);

		if (User)
			return User->IsHalfop();
		else
			return false;
	} else {
		int a = 0;

		while (xhash_t<CChannel*>* Chan = IRC->GetChannels()->Iterate(a++)) {
			if (Chan->Value->GetNames()->Get(Nick) && Chan->Value->GetNames()->Get(Nick)->IsHalfop()) {
				return true;
			}
		}

		return false;
	}
}

const char* getchanprefix(const char* Channel, const char* Nick) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return NULL;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;

	CChannel* Chan = IRC->GetChannel(Nick);

	if (!Chan)
		return NULL;

	CNick* cNick = Chan->GetNames()->Get(Nick);

	if (!cNick)
		return NULL;

	static char outPref[2];

	outPref[0] = Chan->GetHighestUserFlag(cNick->GetPrefixes());
	outPref[1] = '\0';
	
	return outPref;
}

#undef sprintf

const char* getbncuser(const char* User, const char* Type, const char* Parameter2) {
	static char Buffer[1024];

	CBouncerUser* Context = g_Bouncer->GetUser(User);

	if (!Context)
		throw "Invalid user.";

	if (strcmpi(Type, "server") == 0)
		return Context->GetServer();
	else if (strcmpi(Type, "realserver") == 0) {
		CIRCConnection* IRC = Context->GetIRCConnection();;

		if (IRC)
			return IRC->GetServer();
		else
			return NULL;
	} else if (strcmpi(Type, "port") == 0) {
		sprintf(Buffer, "%d", Context->GetPort());

		return Buffer;
	} else if (strcmpi(Type, "realname") == 0)
		return Context->GetRealname();
	else if (strcmpi(Type, "nick") == 0)
		return Context->GetNick();
	else if (strcmpi(Type, "awaynick") == 0)
		return Context->GetAwayNick();
	else if (strcmpi(Type, "away") == 0)
		return Context->GetAwayText();
	else if (strcmpi(Type, "vhost") == 0)
		return Context->GetVHost() ? Context->GetVHost() : g_Bouncer->GetConfig()->ReadString("system.ip");
	else if (strcmpi(Type, "channels") == 0)
		return Context->GetConfigChannels();
	else if (strcmpi(Type, "uptime") == 0) {
		sprintf(Buffer, "%d", Context->IRCUptime());

		return Buffer;
	} else if (strcmpi(Type, "lock") == 0)
		return Context->IsLocked() ? "1" : "0";
	else if (strcmpi(Type, "admin") == 0)
		return Context->IsAdmin() ? "1" : "0";
	else if (strcmpi(Type, "hasserver") == 0)
		return Context->GetIRCConnection() ? "1" : "0";
	else if (strcmpi(Type, "hasclient") == 0)
		return Context->GetClientConnection() ? "1" : "0";
	else if (strcmpi(Type, "delayjoin") == 0)
		return Context->GetDelayJoin() ? "1" : "0";
	else if (strcmpi(Type, "client") == 0) {
		CClientConnection* Client = Context->GetClientConnection();

		if (!Client)
			return NULL;
		else
			return Client->GetPeerName();
	}
	else if (strcmpi(Type, "tag") == 0) {
		if (!Parameter2)
			return NULL;

		char* Buf = (char*)malloc(strlen(Parameter2) + 5);
		sprintf(Buf, "tag.%s", Parameter2);

		return Context->GetConfig()->ReadString(Buf);

		free(Buf);
	} else if (strcmpi(Type, "seen") == 0) {
		sprintf(Buffer, "%d", Context->GetLastSeen());

		return Buffer;
	} else if (strcmpi(Type, "appendts") == 0) {
		sprintf(Buffer, "%d", Context->GetConfig()->ReadInteger("user.ts") != 0 ? 1 : 0);

		return Buffer;
	} else if (strcmpi(Type, "quitasaway") == 0) {
		sprintf(Buffer, "%d", Context->GetConfig()->ReadInteger("user.quitaway") != 0 ? 1 : 0);

		return Buffer;
	} else if (strcmpi(Type, "automodes") == 0) {
		return Context->GetConfig()->ReadString("user.automodes");
	} else if (strcmpi(Type, "dropmodes") == 0) {
		return Context->GetConfig()->ReadString("user.dropmodes");
	} else
		throw "Type should be one of: server port client realname nick awaynick away uptime lock admin hasserver hasclient vhost channels tag delayjoin seen appendts quitasaway automodes dropmodes";
}

int setbncuser(const char* User, const char* Type, const char* Value, const char* Parameter2) {
	CBouncerUser* Context = g_Bouncer->GetUser(User);

	if (!Context)
		throw "Invalid user.";

	if (strcmpi(Type, "server") == 0)
		Context->SetServer(Value);
	else if (strcmpi(Type, "port") == 0)
		Context->SetPort(atoi(Value));
	else if (strcmpi(Type, "realname") == 0)
		Context->SetRealname(Value);
	else if (strcmpi(Type, "nick") == 0)
		Context->SetNick(Value);
	else if (strcmpi(Type, "awaynick") == 0)
		Context->SetAwayNick(Value);
	else if (strcmpi(Type, "vhost") == 0)
		Context->SetVHost(Value);
	else if (strcmpi(Type, "channels") == 0)
		Context->SetConfigChannels(Value);
	else if (strcmpi(Type, "delayjoin") == 0)
		Context->SetDelayJoin(atoi(Value) != 0);
	else if (strcmpi(Type, "away") == 0)
		Context->SetAwayText(Value);
	else if (strcmp(Type, "password") == 0)
		Context->SetPassword(Value);
	else if (strcmpi(Type, "lock") == 0) {
		if (atoi(Value))
			Context->Lock();
		else
			Context->Unlock();
	} else if (strcmpi(Type, "admin") == 0)
		Context->SetAdmin(atoi(Value) ? true : false);
	else if (strcmpi(Type, "tag") == 0 && Value) {
		char* Buf = (char*)malloc(strlen(Value) + 5);
		sprintf(Buf, "tag.%s", Value);

		Context->GetConfig()->WriteString(Buf, Parameter2);

		free(Buf);
	} else if (strcmpi(Type, "appendts") == 0)
		Context->GetConfig()->WriteString("user.ts", Value);
	else if (strcmpi(Type, "quitasaway") == 0)
		Context->GetConfig()->WriteString("user.quitaway", Value);
	else if (strcmpi(Type, "automodes") == 0)
		Context->GetConfig()->WriteString("user.automodes", Value);
	else if (strcmpi(Type, "dropmodes") == 0)
		Context->GetConfig()->WriteString("user.dropmodes", Value);
	else
		throw "Type should be one of: server port realname nick awaynick away lock admin channels tag vhost delayjoin password appendts quitasaway automodes dropmodes";

	return 1;
}

void addbncuser(const char* User, const char* Password) {
	char* Context = strdup(getctx());

	CBouncerUser* U = g_Bouncer->CreateUser(User, Password);

	if (!U)
		throw "Could not create user.";

	setctx(Context);

	free(Context);
}

void delbncuser(const char* User) {
	char* Context = strdup(getctx());

	if (!g_Bouncer->RemoveUser(User))
		throw "Could not remove user.";

	setctx(Context);

	free(Context);
}

int simul(const char* User, const char* Command) {
	CBouncerUser* Context;

	Context = g_Bouncer->GetUser(User);

	if (!Context)
		return 0;
	else {
		Context->Simulate(Command);

		return 1;
	}
}

const char* getchanhost(const char* Nick, const char*) {
	CBouncerUser* Context;

	Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return NULL;
	else {
		CIRCConnection* IRC = Context->GetIRCConnection();

		if (IRC) {
			int a = 0;

			while (xhash_t<CChannel*>* Chan = IRC->GetChannels()->Iterate(a++)) {
				CNick* U = Chan->Value->GetNames()->Get(Nick);

				if (U && U->GetSite() != NULL)
					return U->GetSite();
			}
		}

		return NULL;
	}

}

int getchanjoin(const char* Nick, const char* Channel) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return 0;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return 0;

	CNick* User = Chan->GetNames()->Get(Nick);

	if (!User)
		return 0;

	return User->GetChanJoin();
}

int internalgetchanidle(const char* Nick, const char* Channel) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return 0;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return 0;

	CNick* User = Chan->GetNames()->Get(Nick);

	if (User)
		return time(NULL) - User->GetIdleSince();
	else
		return 0;
}

int ticklerand(int limit) {
	int val = int(rand() / float(RAND_MAX) * limit);

	if (val > limit - 1)
		return limit - 1;
	else
		return val;
}

const char* bncversion(void) {
	return BNCVERSION " 0090000";
}

const char* bncnumversion(void) {
	return "0090000";
}

int bncuptime(void) {
	return g_Bouncer->GetStartup();
}

int floodcontrol(const char* Function) {
	CBouncerUser* User = g_Bouncer->GetUser(g_Context);

	if (!User)
		return 0;

	CIRCConnection* IRC = User->GetIRCConnection();

	if (!IRC)
		return 0;

	CFloodControl* FloodControl = IRC->GetFloodControl();

	int Result;

	if (strcmpi(Function, "bytes") == 0)
		Result = FloodControl->GetBytes();
	else if (strcmpi(Function, "items") == 0)
		Result = FloodControl->GetRealQueueSize();
	else if (strcmpi(Function, "on") == 0) {
		FloodControl->Enable();
		Result = 1;
	} else if (strcmpi(Function, "off") == 0) {
		FloodControl->Disable();
		Result = 1;
	} else
		throw "Function should be one of: bytes items on off";

	return Result;
}

int clearqueue(const char* Queue) {
	CBouncerUser* User = g_Bouncer->GetUser(g_Context);

	if (!User)
		return 0;

	CIRCConnection* IRC = User->GetIRCConnection();

	if (!IRC)
		return 0;

	CQueue* TheQueue = NULL;

	if (strcmpi(Queue, "mode") == 0)
		TheQueue = IRC->GetQueueHigh();
	else if (strcmpi(Queue, "server") == 0)
		TheQueue = IRC->GetQueueMiddle();
	else if (strcmpi(Queue, "help") == 0)
		TheQueue = IRC->GetQueueLow();
	else if (strcmpi(Queue, "all") == 0)
		TheQueue = (CQueue*)IRC->GetFloodControl();
	else
		throw "Queue should be one of: mode server help all";

	int Size;

	if (TheQueue == IRC->GetFloodControl())
		Size = IRC->GetFloodControl()->GetRealQueueSize();
	else
		Size = TheQueue->GetQueueSize();

	TheQueue->FlushQueue();

	return Size;
}

int queuesize(const char* Queue) {
	CBouncerUser* User = g_Bouncer->GetUser(g_Context);

	if (!User)
		return 0;

	CIRCConnection* IRC = User->GetIRCConnection();

	if (!IRC)
		return 0;

	CQueue* TheQueue = NULL;

	if (strcmpi(Queue, "mode") == 0)
		TheQueue = IRC->GetQueueHigh();
	else if (strcmpi(Queue, "server") == 0)
		TheQueue = IRC->GetQueueMiddle();
	else if (strcmpi(Queue, "help") == 0)
		TheQueue = IRC->GetQueueLow();
	else if (strcmpi(Queue, "all") == 0)
		TheQueue = (CQueue*)IRC->GetFloodControl();
	else
		throw "Queue should be one of: mode server help all";

	int Size;

	if (TheQueue == IRC->GetFloodControl())
		Size = IRC->GetFloodControl()->GetRealQueueSize();
	else
		Size = TheQueue->GetQueueSize();

	return Size;
}

int puthelp(const char* text) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return 0;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	IRC->GetQueueLow()->QueueItem(text);

	return 1;
}

int putquick(const char* text) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return 0;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	IRC->GetQueueHigh()->QueueItem(text);

	return 1;
}

const char* getisupport(const char* Feature) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return NULL;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;

	return IRC->GetISupport(Feature);
}

int requiresparam(char Mode) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return 0;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	return IRC->RequiresParameter(Mode);
}

bool isprefixmode(char Mode) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return 0;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	return IRC->IsNickMode(Mode);	
}

const char* bncmodules(void) {
	static char* Buffer = NULL;

	CModule** Modules = g_Bouncer->GetModules();
	int ModuleCount = g_Bouncer->GetModuleCount();

	if (Buffer)
		*Buffer = '\0';

	int a = 0;
	char** List = (char**)malloc(ModuleCount * sizeof(const char*));

	for (int i = 0; i < ModuleCount; i++) {
		if (Modules[i]) {
			char BufId[200], Buf1[200], Buf2[200];

			sprintf(BufId, "%d", i);
			sprintf(Buf1, "%p", Modules[i]->GetHandle());
			sprintf(Buf2, "%p", Modules[i]->GetModule());

			const char* Mod[4] = { BufId, Modules[i]->GetFilename(), Buf1, Buf2 };

			List[a++] = Tcl_Merge(4, Mod);
		}
	}

	static char* Mods = NULL;

	if (Mods)
		Tcl_Free(Mods);

	Mods = Tcl_Merge(a - 1, List);

	for (int c = 0; c < a; c++)
		Tcl_Free(List[c]);

	free(List);

	return Mods;
}

int bncsettag(const char* channel, const char* nick, const char* tag, const char* value) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return 0;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	CChannel* Chan = IRC->GetChannel(channel);

	if (!Chan)
		return 0;

	CNick* User = Chan->GetNames()->Get(nick);

	if (User) {
		User->SetTag(tag, value);

		return 1;
	} else
		return 0;
}

const char* bncgettag(const char* channel, const char* nick, const char* tag) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return NULL;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;

	CChannel* Chan = IRC->GetChannel(channel);

	if (!Chan)
		return NULL;

	CNick* User = Chan->GetNames()->Get(nick);

	if (User)
		return User->GetTag(tag);
	else
		return NULL;
}

void haltoutput(void) {
	g_Ret = false;
}

const char* bnccommand(const char* Cmd, const char* Parameters) {
	for (int i = 0; i < g_Bouncer->GetModuleCount(); i++) {
		CModule* M = g_Bouncer->GetModules()[i];

		if (M) {
			const char* Result = M->Command(Cmd, Parameters);

			if (Result)
				return Result;
		}
	}

	return NULL;
}

const char* md5(const char* String) {
	if (String)
		return g_Bouncer->MD5(String);
	else
		return NULL;
}

void debugout(const char* String) {
#ifdef _WIN32
	OutputDebugString(String);
	OutputDebugString("\n");
#endif
}

void putlog(const char* Text) {
	CBouncerUser* User = g_Bouncer->GetUser(g_Context);

	if (User && Text)
		User->GetLog()->InternalWriteLine(Text);
}

int trafficstats(const char* User, const char* ConnectionType, const char* Type) {
	CBouncerUser* Context = g_Bouncer->GetUser(User);

	if (!Context)
		return 0;

	unsigned int Bytes = 0;

	if (!ConnectionType || strcmpi(ConnectionType, "client") == 0) {
		if (!Type || strcmpi(Type, "in") == 0) {
			Bytes += Context->GetClientStats()->GetInbound();
		}

		if (!Type || strcmpi(Type, "out") == 0) {
			Bytes += Context->GetClientStats()->GetOutbound();
		}
	}

	if (!ConnectionType || strcmpi(ConnectionType, "server") == 0) {
		if (!Type || strcmpi(Type, "in") == 0) {
			Bytes += Context->GetIRCStats()->GetInbound();
		}

		if (!Type || strcmpi(Type, "out") == 0) {
			Bytes += Context->GetIRCStats()->GetOutbound();
		}
	}

	return Bytes;
}

void bncjoinchans(const char* User) {
	CBouncerUser* Context = g_Bouncer->GetUser(User);

	if (!Context)
		return;

	if (Context->GetIRCConnection())
		Context->GetIRCConnection()->JoinChannels();
}

int internallisten(unsigned short Port, const char* Type, const char* Options, const char* Flag) {
	if (strcmpi(Type, "script") == 0) {
		if (Options == NULL)
			throw "You need to specifiy a control proc.";


		const char* BindIp = g_Bouncer->GetConfig()->ReadString("system.ip");

		CTclSocket* TclSocket = new CTclSocket(BindIp, Port, Options);

		if (!TclSocket)
			throw "Could not create object.";
		else if (!TclSocket->IsValid()) {
			TclSocket->Destroy();
			throw "Could not create listener.";
		}

		return TclSocket->GetIdx();
	} else if (strcmpi(Type, "off") == 0) {
		int i = 0;
		socket_t* Socket;

		while ((Socket = g_Bouncer->GetSocketByClass("CTclSocket", i++)) != NULL) {
			sockaddr_in saddr;
			socklen_t saddrSize = sizeof(saddr);
			getsockname(Socket->Socket, (sockaddr*)&saddr, &saddrSize);

			if (ntohs(saddr.sin_port) == Port) {
				Socket->Events->Destroy();

				break;
			}
		}

		return 0;
	} else
		throw "Type must be one of: script off";
}

void control(int Socket, const char* Proc) {
	char Buf[20];
	itoa(Socket, Buf, 10);
	CTclClientSocket* SockPtr = g_TclClientSockets->Get(Buf);

	if (!SockPtr || !g_Bouncer->IsRegisteredSocket(SockPtr))
		throw "Invalid socket.";

	SockPtr->SetControlProc(Proc);
}

void internalsocketwriteln(int Socket, const char* Line) {
	char Buf[20];
	itoa(Socket, Buf, 10);
	CTclClientSocket* SockPtr = g_TclClientSockets->Get(Buf);

	if (!SockPtr || !g_Bouncer->IsRegisteredSocket(SockPtr))
		throw "Invalid socket pointer.";

	SockPtr->WriteLine(Line);
}

int internalconnect(const char* Host, unsigned short Port) {
	SOCKET Socket = g_Bouncer->SocketAndConnect(Host, Port, NULL);

	if (Socket == INVALID_SOCKET)
		throw "Could not connect.";

	CTclClientSocket* Wrapper = new CTclClientSocket(Socket);

	return Wrapper->GetIdx();
}

void internalclosesocket(int Socket) {
	char Buf[20];
	itoa(Socket, Buf, 10);
	CTclClientSocket* SockPtr = g_TclClientSockets->Get(Buf);

	if (!SockPtr || !g_Bouncer->IsRegisteredSocket(SockPtr))
		throw "Invalid socket pointer.";

	if (SockPtr->MayNotEnterDestroy())
		SockPtr->DestroyLater();
	else
		SockPtr->Destroy();
}

bool bnccheckpassword(const char* User, const char* Password) {
	CBouncerUser* Context = g_Bouncer->GetUser(User);

	if (!Context)
		return false;

	bool Ret = Context->Validate(Password);

	return Ret;
}

void bncdisconnect(const char* Reason) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return;

	CIRCConnection* Irc = Context->GetIRCConnection();

	if (!Irc)
		return;

	Irc->Kill(Reason);

	Context->MarkQuitted();
}

void bnckill(const char* Reason) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return;

	CClientConnection* Client = Context->GetClientConnection();

	if (!Client)
		return;

	Client->Kill(Reason);
}

void bncreply(const char* Text) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return;

	if (g_NoticeUser)
		Context->RealNotice(Text);
	else
		Context->Notice(Text);
}

char* chanbans(const char* Channel) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		return NULL;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return NULL;

	CBanlist* Banlist = Chan->GetBanlist();

	char** Blist = NULL;
	int Bcount = 0;

	int i = 0;
	while (const ban_t* Ban = Banlist->Iterate(i)) {
		char TS[20];

		sprintf(TS, "%d", Ban->TS);

		char* ThisBan[3] = { Ban->Mask, Ban->Nick, TS };

		char* List = Tcl_Merge(3, ThisBan);

		Blist = (char**)realloc(Blist, ++Bcount * sizeof(char*));

		Blist[Bcount - 1] = List;

		i++;
	}

	static char* AllBans = NULL;

	if (AllBans)
		Tcl_Free(AllBans);

	AllBans = Tcl_Merge(Bcount, Blist);

	for (int a = 0; a < Bcount; a++)
		Tcl_Free(Blist[a]);

	free(Blist);

	return AllBans;
}

bool TclTimerProc(time_t Now, void* RawCookie) {
	tcltimer_t* Cookie = (tcltimer_t*)RawCookie;

	if (Cookie == NULL)
		return false;

	Tcl_Obj* objv[2];
	int objc;

	if (Cookie->param)
		objc = 2;
	else
		objc = 1;

	objv[0] = Tcl_NewStringObj(Cookie->proc, -1);

	Tcl_IncrRefCount(objv[0]);

	if (Cookie->param) {
		objv[1] = Tcl_NewStringObj(Cookie->param, -1);

		Tcl_IncrRefCount(objv[1]);
	}

	Tcl_EvalObjv(g_Interp, objc, objv, TCL_EVAL_GLOBAL);

	if (Cookie->param) {
		Tcl_DecrRefCount(objv[1]);
	}

	Tcl_DecrRefCount(objv[0]);

	if (!Cookie->repeat) {
		for (int i = 0; i < g_TimerCount; i++) {
			if (g_Timers[i] == Cookie) {
				g_Timers[i] = NULL;

				break;
			}
		}

		free(Cookie->proc);
		free(Cookie->param);
		free(Cookie);
	}

	return true;
}

int internaltimer(int Interval, bool Repeat, const char* Proc, const char* Parameter) {
	internalkilltimer(Proc, Parameter);

	tcltimer_t** n = NULL;

	for (int i = 0; i < g_TimerCount; i++) {
		if (g_Timers[i] == NULL) {
			n = &g_Timers[i];

			break;
		}
	}

	if (n == NULL) {
		g_Timers = (tcltimer_t**)realloc(g_Timers, ++g_TimerCount * sizeof(tcltimer_t*));

		n = &g_Timers[g_TimerCount - 1];
	}
	
	*n = (tcltimer_t*)malloc(sizeof(tcltimer_t));

	tcltimer_t* p = *n;

	p->timer = g_Bouncer->CreateTimer(Interval, Repeat, TclTimerProc, p);

	p->proc = strdup(Proc);

	p->repeat = Repeat;

	if (Parameter) {
		p->param = strdup(Parameter);
	} else
		p->param = NULL;

	return 1;
}

int internalkilltimer(const char* Proc, const char* Parameter) {
	if (!g_Timers)
		return 0;

	for (int i = 0; i < g_TimerCount; i++) {
		if (g_Timers[i] && strcmp(g_Timers[i]->proc, Proc) == 0 && (!Parameter || !g_Timers[i]->param || strcmp(Parameter, g_Timers[i]->param) == 0)) {
			g_Timers[i]->timer->Destroy();
			free(g_Timers[i]->proc);
			free(g_Timers[i]->param);

			g_Timers[i] = NULL;

			return 1;
		}
	}

	return 0;
}

int timerstats(void) {
	return g_Bouncer->GetTimerStats();
}

const char* getcurrentnick(void) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return NULL;

	if (Context->GetIRCConnection())
		return Context->GetIRCConnection()->GetCurrentNick();
	else
		return Context->GetNick();
}

const char* bncgetmotd(void) {
	return g_Bouncer->GetMotd();
}

void bncsetmotd(const char* Motd) {
	g_Bouncer->SetMotd(Motd);
}

const char* bncgetgvhost(void) {
	return g_Bouncer->GetConfig()->ReadString("system.ip");
}

void bncsetgvhost(const char* GVHost) {
	g_Bouncer->GetConfig()->WriteString("system.ip", GVHost);
}

const char* getbnchosts(void) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return NULL;

	int Count = Context->GetHostAllowCount();
	char** Hosts = Context->GetHostAllows();

	int argc = 0;
	const char** argv = (const char**)malloc(Count * sizeof(const char*));

	for (int i = 0; i < Count; i++) {
		if (Hosts[i])
			argv[argc++] = Hosts[i];
	}

	static char* List = NULL;

	if (List)
		Tcl_Free(List);

	List = Tcl_Merge(argc, argv);

	free(argv);

	return List;
}

void delbnchost(const char* Host) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return;

	Context->RemoveHostAllow(Host, true);
}

int addbnchost(const char* Host) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return 0;

	char** Hosts = Context->GetHostAllows();
	unsigned int a = 0;

	for (unsigned int i = 0; i < Context->GetHostAllowCount(); i++) {
		if (Hosts[i])
			a++;
	}

	if (Context->CanHostConnect(Host) && a)
		return 0;

	Context->AddHostAllow(Host, true);

	return 1;
}

bool bncisipblocked(const char* Ip) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return false;

	sockaddr_in Peer;

	Peer.sin_family = AF_INET;

	unsigned long addr = inet_addr(Ip);

#ifdef _WIN32
	Peer.sin_addr.S_un.S_addr = addr;
#else
	Peer.sin_addr.s_addr = addr;
#endif

	return Context->IsIpBlocked(Peer);
}

bool bnccanhostconnect(const char* Host) {
	CBouncerUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return false;

	bool Ret = Context->CanHostConnect(Host);

	return Ret;
}

bool bncvalidusername(const char* Name) {
	return g_Bouncer->IsValidUsername(Name);
}

int bncgetsendq(void) {
	return g_Bouncer->GetSendQSize();
}

void bncsetsendq(int NewSize) {
	g_Bouncer->SetSendQSize(NewSize);
}
