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

#include "../src/StdAfx.h"
#include "StdAfx.h"

#include "tickle.h"
#include "tickleProcs.h"
#include "TclClientSocket.h"
#include "TclSocket.h"

static char *g_Context = NULL;
CClientConnection *g_CurrentClient = NULL;

binding_t *g_Binds = NULL;
int g_BindCount = 0;

tcltimer_t **g_Timers = NULL;
int g_TimerCount = 0;

extern Tcl_Encoding g_Encoding;

extern Tcl_Interp *g_Interp;

extern bool g_Ret;
extern bool g_NoticeUser;

extern "C" int Bnc_Init(Tcl_Interp *);

int Tcl_ProcInit(Tcl_Interp *interp) {
	return Bnc_Init(interp);
}

void die(void) {
	g_Bouncer->Shutdown();
}

void setctx(const char *ctx) {
	char *Suffix;
	CUser *User;
	int i;
	time_t TS;
	char *CtxDup;

	free(g_Context);

	g_CurrentClient = NULL;

	if (ctx != NULL) {
		CtxDup = strdup(ctx);

		Suffix = strchr(CtxDup, '<');

		if (Suffix != NULL) {
			*Suffix = '\0';
			Suffix++;

			User = g_Bouncer->GetUser(CtxDup);

			if (User != NULL) {
				if (*Suffix == '*') {
					g_CurrentClient = User->GetClientConnectionMultiplexer();
				} else if (*Suffix == '0') {
					g_CurrentClient = User->GetPrimaryClientConnection();
				} else {
					TS = atoi(Suffix);

					g_CurrentClient = NULL;

					for (i = 0; i < User->GetClientConnections()->GetLength(); i++) {
						if (User->GetClientConnections()->Get(i).Creation == TS) {
							g_CurrentClient = User->GetClientConnections()->Get(i).Client;

							break;
						}
					}
				}
			}

			g_Context = strdup(CtxDup);
		} else {
			g_Context = strdup(ctx);
		}

		free(CtxDup);
	}
}

const char *getctx(int ts) {
	static char *Context = NULL;
	int i, rc;
	CUser *User;
	time_t TS;

	gfree(Context);

	if (g_CurrentClient != NULL && ts) {
		User = g_CurrentClient->GetOwner();

		if (User == NULL) {
			rc = asprintf(&Context, "");
		} else 	if (g_CurrentClient == User->GetClientConnectionMultiplexer()) {
			rc = asprintf(&Context, "%s<*", g_Context);
		} else {
			TS = 0;

			for (i = 0; i < User->GetClientConnections()->GetLength(); i++) {
				if (User->GetClientConnections()->Get(i).Client == g_CurrentClient) {
					TS = User->GetClientConnections()->Get(i).Creation;

					break;
				}
			}

			rc = asprintf(&Context, "%s<%d", g_Context, (int)TS);
		}
	} else {
		rc = asprintf(&Context, "%s", g_Context);
	}

	if (RcFailed(rc)) {
		g_Bouncer->Fatal();
	}

	return Context;
}

const char *bncuserlist(void) {
	int i;
	int Count = g_Bouncer->GetUsers()->GetLength();

	int argc = 0;
	const char** argv = (const char**)malloc(Count * sizeof(const char*));

	CHashtable<CUser *, false> *Users = g_Bouncer->GetUsers();

	i = 0;
	while (hash_t<CUser *> *User = Users->Iterate(i++)) {
		argv[argc++] = User->Name;
	}

	static char* List = NULL;

	if (List != NULL) {
		Tcl_Free(List);
	}

	List = Tcl_Merge(argc, const_cast<char **>(argv));

	free(argv);

	return List;
}

const char* internalchannels(void) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		throw "User is not connected to an IRC server.";

	CHashtable<CChannel*, false>* H = IRC->GetChannels();

	if (H == NULL)
		return NULL;

	int Count = H->GetLength();

	const char** argv = (const char**)malloc(Count * sizeof(const char*));

	int a = 0;

	while (hash_t<CChannel*>* Chan = H->Iterate(a)) {
		argv[a] = Chan->Name;
		a++;
	}

	static char* List = NULL;

	if (List != NULL) {
		Tcl_Free(List);
	}

	List = Tcl_Merge(Count, const_cast<char **>(argv));

	free(argv);

	return List;
}

const char* getchanmode(const char* Channel) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		throw "User is not connected to an IRC server.";

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return NULL;
	else
		return Chan->GetChannelModes();
}

void rehash(void) {
	RehashInterpreter();

	g_Bouncer->Log("Rehashing TCL module");
}

int internalbind(const char* type, const char* proc, const char* pattern, const char* user) {
	if (pattern == NULL) {
		pattern = "*";
	}

	if (user == NULL) {
		user = "*";
	}

	for (int i = 0; i < g_BindCount; i++) {
		if (g_Binds[i].valid && strcmp(g_Binds[i].proc, proc) == 0
			&& ((!pattern && g_Binds[i].pattern == NULL) || (pattern && g_Binds[i].pattern && strcmp(pattern, g_Binds[i].pattern) == 0))
			&& ((!user && g_Binds[i].user == NULL) || (user && g_Binds[i].user && strcasecmp(user, g_Binds[i].user) == 0)))

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

	if (strcasecmp(type, "client") == 0)
		Bind->type = Type_Client;
	else if (strcasecmp(type, "server") == 0)
		Bind->type = Type_Server;
	else if (strcasecmp(type, "pre") == 0)
		Bind->type = Type_PreScript;
	else if (strcasecmp(type, "post") == 0)
		Bind->type = Type_PostScript;
	else if (strcasecmp(type, "attach") == 0)
		Bind->type = Type_Attach;
	else if (strcasecmp(type, "detach") == 0)
		Bind->type = Type_Detach;
	else if (strcasecmp(type, "modec") == 0)
		Bind->type = Type_SingleMode;
	else if (strcasecmp(type, "unload") == 0)
		Bind->type = Type_Unload;
	else if (strcasecmp(type, "svrdisconnect") == 0)
		Bind->type = Type_SvrDisconnect;
	else if (strcasecmp(type, "svrconnect") == 0)
		Bind->type = Type_SvrConnect;
	else if (strcasecmp(type, "svrlogon") == 0)
		Bind->type = Type_SvrLogon;
	else if (strcasecmp(type, "usrload") == 0)
		Bind->type = Type_UsrLoad;
	else if (strcasecmp(type, "usrcreate") == 0)
		Bind->type = Type_UsrCreate;
	else if (strcasecmp(type, "usrdelete") == 0)
		Bind->type = Type_UsrDelete;
	else if (strcasecmp(type, "command") == 0)
		Bind->type = Type_Command;
	else if (strcasecmp(type, "settag") == 0)
		Bind->type = Type_SetTag;
	else if (strcasecmp(type, "setusertag") == 0)
		Bind->type = Type_SetUserTag;
	else if (strcasecmp(type, "prerehash") == 0)
		Bind->type = Type_PreRehash;
	else if (strcasecmp(type, "postrehash") == 0)
		Bind->type = Type_PostRehash;
	else if (strcasecmp(type, "channelsort") == 0)
		Bind->type = Type_ChannelSort;
	else {
		Bind->type = Type_Invalid;

		throw "Invalid bind type.";
	}

	Bind->proc = strdup(proc);
	Bind->valid = true;

	Bind->pattern = strdup(pattern);
	Bind->user = strdup(user);

	return 1;
}

int internalunbind(const char* type, const char* proc, const char* pattern, const char* user) {
	binding_type_e bindtype;

	if (strcasecmp(type, "client") == 0)
		bindtype = Type_Client;
	else if (strcasecmp(type, "server") == 0)
		bindtype = Type_Server;
	else if (strcasecmp(type, "pre") == 0)
		bindtype = Type_PreScript;
	else if (strcasecmp(type, "post") == 0)
		bindtype = Type_PostScript;
	else if (strcasecmp(type, "attach") == 0)
		bindtype = Type_Attach;
	else if (strcasecmp(type, "detach") == 0)
		bindtype = Type_Detach;
	else if (strcasecmp(type, "modec") == 0)
		bindtype = Type_SingleMode;
	else if (strcasecmp(type, "unload") == 0)
		bindtype = Type_Unload;
	else if (strcasecmp(type, "svrdisconnect") == 0)
		bindtype = Type_SvrDisconnect;
	else if (strcasecmp(type, "svrconnect") == 0)
		bindtype = Type_SvrConnect;
	else if (strcasecmp(type, "svrlogon") == 0)
		bindtype = Type_SvrLogon;
	else if (strcasecmp(type, "usrload") == 0)
		bindtype = Type_UsrLoad;
	else if (strcasecmp(type, "usrcreate") == 0)
		bindtype = Type_UsrCreate;
	else if (strcasecmp(type, "usrdelete") == 0)
		bindtype = Type_UsrDelete;
	else if (strcasecmp(type, "command") == 0)
		bindtype = Type_Command;
	else if (strcasecmp(type, "settag") == 0)
		bindtype = Type_SetTag;
	else if (strcasecmp(type, "setusertag") == 0)
		bindtype = Type_SetUserTag;
	else if (strcasecmp(type, "prerehash") == 0)
		bindtype = Type_PreRehash;
	else if (strcasecmp(type, "postrehash") == 0)
		bindtype = Type_PostRehash;
	else if (strcasecmp(type, "channelsort") == 0)
		bindtype = Type_ChannelSort;
	else
		return 0;

	if (pattern == NULL) {
		pattern = "*";
	}

	if (user == NULL) {
		user = "*";
	}

	for (int i = 0; i < g_BindCount; i++) {
		if (g_Binds[i].valid && g_Binds[i].type == bindtype && strcmp(g_Binds[i].proc, proc) == 0
			&& (strcmp(pattern, g_Binds[i].pattern) == 0)
			&& (strcasecmp(user, g_Binds[i].user) == 0)) {

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
			else if (type == Type_SetTag)
				Bind[0] = "settag";
			else if (type == Type_SetUserTag)
				Bind[0] = "setusertag";
			else if (type == Type_PreRehash)
				Bind[0] = "prerehash";
			else if (type == Type_PostRehash)
				Bind[0] = "postrehash";
			else if (type == Type_ChannelSort)
				Bind[0] = "channelsort";
			else
				Bind[0] = "invalid";

			Bind[1] = g_Binds[i].proc;
			Bind[2] = g_Binds[i].pattern;
			Bind[3] = g_Binds[i].user;

			char* Item = Tcl_Merge(4, const_cast<char **>(Bind));

			List[n++] = Item;
		}
	}

	static char* Out = NULL;

	if (Out != NULL) {
		Tcl_Free(Out);
	}

	Out = Tcl_Merge(n, const_cast<char **>(List));

	for (int a = 0; a < n; a++)
		Tcl_Free(List[a]);

	return Out;
}

int putserv(const char* text, const char *option) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	if (option != NULL && strcasecmp(option, "-next") == 0) {
		IRC->GetQueueMiddle()->QueueItemNext(text);
	} else {
		IRC->GetQueueMiddle()->QueueItem(text);
	}

	return 1;
}

int putclient(const char* text) {
	CUser *Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		throw "Invalid user.";

	if (g_CurrentClient == NULL || Context != g_CurrentClient->GetOwner())
		g_CurrentClient = Context->GetClientConnectionMultiplexer();

	if (!g_CurrentClient)
		return 0;

	g_CurrentClient->WriteLine("%s", text);

	return 1;
}

void jump(const char *Server, unsigned int Port, const char *Password) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	if (Server)
		Context->SetServer(Server);

	if (Port != 0)
		Context->SetPort(Port);

	if (Password)
		Context->SetServerPassword(Password);

	Context->Reconnect();
	SetLatchedReturnValue(false);
}

bool onchan(const char* Nick, const char* Channel) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return false;

	if (Channel) {
		CChannel* Chan = IRC->GetChannel(Channel);

		if (Chan && Chan->GetNames()->Get(Nick))
			return true;
		else
			return false;
	} else {
		int a = 0;

		if (IRC->GetChannels() == NULL)
			return false;

		while (hash_t<CChannel*>* Chan = IRC->GetChannels()->Iterate(a++)) {
			if (Chan->Value->GetNames()->Get(Nick)) {
				return true;
			}
		}

		return false;
	}
}

const char* topic(const char* Channel) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return NULL;

	return Chan->GetTopic();
}

const char* topicnick(const char* Channel) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return NULL;

	return Chan->GetTopicNick();
}

int topicstamp(const char* Channel) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return 0;

	return (int)Chan->GetTopicStamp();
}

const char* internalchanlist(const char* Channel) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return NULL;

	const CHashtable<CNick*, false>* Names = Chan->GetNames();

	int Count = Names->GetLength();
	const char** argv = (const char**)malloc(Count * sizeof(const char*));

	int a = 0;

	while (hash_t<CNick*>* NickHash = Names->Iterate(a++)) {
		argv[a-1] = NickHash->Name;
	}

	static char* List = NULL;

	if (List != NULL) {
		Tcl_Free(List);
	}

	List = Tcl_Merge(Count, const_cast<char **>(argv));

	free(argv);

	return List;
}

bool isop(const char* Nick, const char* Channel) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

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

		if (IRC->GetChannels() == NULL)
			return false;

		while (hash_t<CChannel*>* Chan = IRC->GetChannels()->Iterate(a++)) {
			if (Chan->Value->GetNames()->Get(Nick) && Chan->Value->GetNames()->Get(Nick)->IsOp()) {
				return true;
			}
		}

		return false;
	}
}

bool isvoice(const char* Nick, const char* Channel) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

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

		if (IRC->GetChannels() == NULL)
			return false;

		while (hash_t<CChannel*>* Chan = IRC->GetChannels()->Iterate(a++)) {
			if (Chan->Value->GetNames()->Get(Nick) && Chan->Value->GetNames()->Get(Nick)->IsVoice()) {
				return true;
			}
		}

		return false;
	}
}

bool ishalfop(const char* Nick, const char* Channel) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

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

		if (IRC->GetChannels() == NULL)
			return false;

		while (hash_t<CChannel*>* Chan = IRC->GetChannels()->Iterate(a++)) {
			if (Chan->Value->GetNames()->Get(Nick) && Chan->Value->GetNames()->Get(Nick)->IsHalfop()) {
				return true;
			}
		}

		return false;
	}
}

const char* getchanprefix(const char* Channel, const char* Nick) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return NULL;

	CNick* cNick = Chan->GetNames()->Get(Nick);

	if (!cNick)
		return NULL;

	static char outPref[2];

	outPref[0] = IRC->GetHighestUserFlag(cNick->GetPrefixes());
	outPref[1] = '\0';
	
	return outPref;
}

const char* getbncuser(const char* User, const char* Type, const char* Parameter2) {
	CIRCConnection *IRC;
	static char *Buffer = NULL;

	if (Buffer != NULL) {
		gfree(Buffer);
		Buffer = NULL;
	}

	CUser* Context = g_Bouncer->GetUser(User);

	if (!Context)
		throw "Invalid user.";

	if (strcasecmp(Type, "server") == 0)
		return Context->GetServer();
	else if (strcasecmp(Type, "serverpass") == 0)
		return Context->GetServerPassword();
	else if (strcasecmp(Type, "realserver") == 0) {
		IRC = Context->GetIRCConnection();;

		if (IRC)
			return IRC->GetServer();
		else
			return NULL;
	} else if (strcasecmp(Type, "port") == 0) {
		int rc = asprintf(&Buffer, "%d", Context->GetPort());

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		return Buffer;
	} else if (strcasecmp(Type, "realname") == 0)
		return Context->GetRealname();
	else if (strcasecmp(Type, "nick") == 0)
		return Context->GetNick();
	else if (strcasecmp(Type, "awaynick") == 0)
		return Context->GetAwayNick();
	else if (strcasecmp(Type, "away") == 0)
		return Context->GetAwayText();
	else if (strcasecmp(Type, "awaymessage") == 0)
		return Context->GetAwayMessage();
	else if (strcasecmp(Type, "vhost") == 0)
		return Context->GetVHost() ? Context->GetVHost() : g_Bouncer->GetConfig()->ReadString("system.ip");
	else if (strcasecmp(Type, "channels") == 0)
		return Context->GetConfigChannels();
	else if (strcasecmp(Type, "uptime") == 0) {
		int rc = asprintf(&Buffer, "%d", Context->GetIRCUptime());

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		return Buffer;
	} else if (strcasecmp(Type, "lock") == 0)
		return Context->IsLocked() ? "1" : "0";
	else if (strcasecmp(Type, "admin") == 0)
		return Context->IsAdmin() ? "1" : "0";
	else if (strcasecmp(Type, "hasserver") == 0)
		return Context->GetIRCConnection() ? "1" : "0";
	else if (strcasecmp(Type, "hasclient") == 0)
		return Context->GetClientConnectionMultiplexer() ? "1" : "0";
	else if (strcasecmp(Type, "delayjoin") == 0) {
		int DelayJoin = Context->GetDelayJoin();

		if (DelayJoin == 1)
			return "1";
		else if (DelayJoin == 0)
			return "0";
		else
			return "-1";
	} else if (strcasecmp(Type, "client") == 0) {
		CClientConnection* Client = Context->GetPrimaryClientConnection();

		if (!Client)
			return NULL;
		else
			return Client->GetPeerName();
	} else if (strcasecmp(Type, "clientcount") == 0) {
		int rc = asprintf(&Buffer, "%d", Context->GetClientConnections()->GetLength());

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		return Buffer;
	} else if (strcasecmp(Type, "tag") == 0) {
		if (!Parameter2)
			return NULL;

		return Context->GetTagString(Parameter2);
	} else if (strcasecmp(Type, "tags") == 0) {
		CConfig *Config = Context->GetConfig();
		int Count = Config->GetLength();
		const char *Item;

		int argc = 0;
		const char** argv = (const char**)malloc(Count * sizeof(const char*));

		while ((Item = Context->GetTagName(argc)) != NULL) {
			argv[argc] = Item;
			argc++;
		}

		static char* List = NULL;

		if (List != NULL) {
			Tcl_Free(List);
		}

		List = Tcl_Merge(argc, const_cast<char **>(argv));

		free(argv);

		return List;
	} else if (strcasecmp(Type, "seen") == 0) {
		int rc = asprintf(&Buffer, "%d", (int)Context->GetLastSeen());

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		return Buffer;
	} else if (strcasecmp(Type, "quitasaway") == 0) {
		int rc = asprintf(&Buffer, "%d", Context->GetUseQuitReason() ? 1 : 0);

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		return Buffer;
	} else if (strcasecmp(Type, "automodes") == 0) {
		return Context->GetAutoModes();
	} else if (strcasecmp(Type, "dropmodes") == 0) {
		return Context->GetDropModes();
	} else if (strcasecmp(Type, "suspendreason") == 0) {
		return Context->GetSuspendReason();
	} else if (strcasecmp(Type, "ssl") == 0) {
		return Context->GetSSL() ? "1" : "0";
	} else if (strcasecmp(Type, "sslclient") == 0) {
		CClientConnection* Client = Context->GetPrimaryClientConnection();

		if (!Client)
			return NULL;
		else {
			int rc = asprintf(&Buffer, "%d", Client->IsSSL() ? 1 : 0);

			if (RcFailed(rc)) {
				g_Bouncer->Fatal();
			}

			return Buffer;
		}
	} else if (strcasecmp(Type, "ident") == 0) {
		return Context->GetIdent();
	} else if (strcasecmp(Type, "localip") == 0) {
		IRC = Context->GetIRCConnection();

		if (IRC != NULL && IRC->GetLocalAddress() != NULL) {
			return IpToString(IRC->GetLocalAddress());
		} else {
			return NULL;
		}
	} else if (strcasecmp(Type, "lean") == 0) {
		int rc = asprintf(&Buffer, "%d", Context->GetLeanMode());

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		return Buffer;
	} else if (strcasecmp(Type, "channelsort") == 0) {
		return Context->GetChannelSortMode();
	} else if (strcasecmp(Type, "sessions") == 0) {
		CVector<client_t> *Clients = Context->GetClientConnections();
		int Count = Clients->GetLength();
		char *Item;

		char** argv = (char**)malloc(Count * sizeof(const char*));

		for (int i = 0; i < Count; i++) {
			int rc = asprintf(&Item, "%s<%d", Context->GetUsername(), (int)(*Clients)[i].Creation);

			if (RcFailed(rc)) {
				g_Bouncer->Fatal();
			}

			argv[i] = Item;
		}

		static char* List = NULL;

		if (List != NULL) {
			Tcl_Free(List);
		}

		List = Tcl_Merge(Count, const_cast<char **>(argv));

		for (int i = 0; i < Count; i++) {
			free(argv[i]);
		}

		free(argv);

		return List;
	} else if (strcasecmp(Type, "autobacklog") == 0) {
		return Context->GetAutoBacklog();
	} else if (strcasecmp(Type, "sysnotices") == 0) {
		return Context->GetSystemNotices() ? "1" : "0";
	} else {
		throw "Type should be one of: server port serverpass client clientcount "
			"realname nick awaynick away awaymessage uptime lock admin hasserver "
			"hasclient vhost channels tag delayjoin seen quitasaway automodes "
			"dropmodes suspendreason ssl sslclient realserver ident tags localip "
			"lean memory memorylimit channelsort sessions autobacklog sysnotices";
	}
}

int setbncuser(const char* User, const char* Type, const char* Value, const char* Parameter2) {
	CUser* Context = g_Bouncer->GetUser(User);

	if (!Context)
		throw "Invalid user.";

	if (strcasecmp(Type, "server") == 0)
		Context->SetServer(Value);
	else if (strcasecmp(Type, "serverpass") == 0)
		Context->SetServerPassword(Value);
	else if (strcasecmp(Type, "port") == 0)
		Context->SetPort(atoi(Value));
	else if (strcasecmp(Type, "realname") == 0)
		Context->SetRealname(Value);
	else if (strcasecmp(Type, "nick") == 0)
		Context->SetNick(Value);
	else if (strcasecmp(Type, "awaynick") == 0)
		Context->SetAwayNick(Value);
	else if (strcasecmp(Type, "vhost") == 0)
		Context->SetVHost(Value);
	else if (strcasecmp(Type, "channels") == 0)
		Context->SetConfigChannels(Value);
	else if (strcasecmp(Type, "delayjoin") == 0)
		Context->SetDelayJoin(atoi(Value));
	else if (strcasecmp(Type, "away") == 0)
		Context->SetAwayText(Value);
	else if (strcasecmp(Type, "awaymessage") == 0)
		Context->SetAwayMessage(Value);
	else if (strcmp(Type, "password") == 0)
		Context->SetPassword(Value);
	else if (strcmp(Type, "ssl") == 0)
		Context->SetSSL(Value ? (atoi(Value) ? true : false) : false);
	else if (strcasecmp(Type, "lock") == 0) {
		if (atoi(Value))
			Context->Lock();
		else
			Context->Unlock();
	} else if (strcasecmp(Type, "admin") == 0)
		Context->SetAdmin(Value ? (atoi(Value) ? true : false) : false);
	else if (strcasecmp(Type, "tag") == 0 && Value)
		Context->SetTagString(Value, Parameter2);
	else if (strcasecmp(Type, "quitasaway") == 0)
		Context->SetUseQuitReason(Value ? (atoi(Value) ? true : false) : false);
	else if (strcasecmp(Type, "automodes") == 0)
		Context->SetAutoModes(Value);
	else if (strcasecmp(Type, "dropmodes") == 0)
		Context->SetDropModes(Value);
	else if (strcasecmp(Type, "suspendreason") == 0)
		Context->SetSuspendReason(Value);
	else if (strcasecmp(Type, "ident") == 0)
		Context->SetIdent(Value);
	else if (strcasecmp(Type, "lean") == 0)
		Context->SetLeanMode(atoi(Value));
	else if (strcasecmp(Type, "channelsort") == 0)
		Context->SetChannelSortMode(Value);
	else if (strcasecmp(Type, "autobacklog") == 0)
		Context->SetAutoBacklog(Value);
	else if (strcasecmp(Type, "sysnotices") == 0)
		Context->SetSystemNotices(Value);
	else
		throw "Type should be one of: server port serverpass realname nick awaynick "
			"away awaymessage lock admin channels tag vhost delayjoin password "
			"quitasaway automodes dropmodes suspendreason ident lean channelsort "
			"autobacklog sysnotices";

	return 1;
}

void addbncuser(const char* User, const char* Password) {
	RESULT<CUser *> Result;
	char *Context;

	Context = strdup(getctx());
	Result = g_Bouncer->CreateUser(User, Password);
	setctx(Context);
	free(Context);

	if (IsError(Result)) {
		throw GETDESCRIPTION(Result);
	}
}

void delbncuser(const char* User) {
	RESULT<bool> Result;
	char *Context;

	Context = strdup(getctx());
	Result = g_Bouncer->RemoveUser(User);
	setctx(Context);
	free(Context);

	if (IsError(Result)) {
		throw GETDESCRIPTION(Result);
	}
}

const char *simul(const char* User, const char* Command) {
	CUser* Context;
	const char *TempResult;
	static char *Result = NULL;

	Context = g_Bouncer->GetUser(User);

	if (Context == NULL) {
		return NULL;
	}

	free(Result);
	TempResult = Context->SimulateWithResult(Command);

	if (TempResult != NULL) {
		Result = strdup(TempResult);
	} else {
		Result = NULL;
	}

	return Result;
}

const char* getchanhost(const char* Nick, const char*) {
	CUser* Context;
	const char* Host;

	Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (IRC) {
		int a = 0;

		if (IRC->GetCurrentNick() && strcasecmp(IRC->GetCurrentNick(), Nick) == 0) {
			Host = IRC->GetSite();

			if (Host)
				return Host;
		}

		if (IRC->GetChannels() == NULL)
			return NULL;

		while (hash_t<CChannel*>* Chan = IRC->GetChannels()->Iterate(a++)) {
			CNick* U = Chan->Value->GetNames()->Get(Nick);

			if (U/* && U->GetSite() != NULL*/)
				return U->GetSite();
		}
	}

	return NULL;
}

const char* getchanrealname(const char* Nick, const char*) {
	CUser* Context;

	Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (IRC) {
		int a = 0;

		if (IRC->GetChannels() == NULL)
			return NULL;

		while (hash_t<CChannel*>* Chan = IRC->GetChannels()->Iterate(a++)) {
			CNick* U = Chan->Value->GetNames()->Get(Nick);

			if (U/* && U->GetSite() != NULL*/)
				return U->GetRealname();
		}
	}

	return NULL;
}


int getchanjoin(const char* Nick, const char* Channel) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return 0;

	CNick* User = Chan->GetNames()->Get(Nick);

	if (!User)
		return 0;

	return (int)User->GetChanJoin();
}

int internalgetchanidle(const char* Nick, const char* Channel) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	CChannel* Chan = IRC->GetChannel(Channel);

	if (!Chan)
		return 0;

	CNick* User = Chan->GetNames()->Get(Nick);

	if (User)
		return (int)(time(NULL) - User->GetIdleSince());
	else
		return 0;
}

int ticklerand(int limit) {
	if (limit == 0) {
		/* not entirely correct, but certainly better than crashing */
		return 0;
	}

	return rand() % limit;
}

const char* bncversion(void) {
	return BNCVERSION;
}

const char* bncnumversion(void) {
	return "1010000";
}

int bncuptime(void) {
	return (int)g_Bouncer->GetStartup();
}

int floodcontrol(const char* Function) {
	CUser* User = g_Bouncer->GetUser(g_Context);

	if (!User)
		throw "Invalid user.";

	CIRCConnection* IRC = User->GetIRCConnection();

	if (!IRC)
		return 0;

	CFloodControl* FloodControl = IRC->GetFloodControl();

	int Result;

	if (strcasecmp(Function, "bytes") == 0)
		Result = FloodControl->GetBytes();
	else if (strcasecmp(Function, "items") == 0)
		Result = FloodControl->GetQueueSize();
	else if (strcasecmp(Function, "on") == 0) {
		FloodControl->Enable();
		Result = 1;
	} else if (strcasecmp(Function, "off") == 0) {
		FloodControl->Disable();
		Result = 1;
	} else
		throw "Function should be one of: bytes items on off";

	return Result;
}

int clearqueue(const char* Queue) {
	int Size;
	CUser* User;
	CIRCConnection* IRC;
	
	User = g_Bouncer->GetUser(g_Context);

	if (User == NULL) {
		throw "Invalid user.";
	}

	IRC = User->GetIRCConnection();

	if (IRC == NULL) {
		return 0;
	}

	if (strcasecmp(Queue, "mode") == 0) {
		Size = IRC->GetQueueHigh()->GetLength();
		IRC->GetQueueHigh()->Clear();
	} else if (strcasecmp(Queue, "server") == 0) {
		Size = IRC->GetQueueMiddle()->GetLength();
		IRC->GetQueueMiddle()->Clear();
	} else if (strcasecmp(Queue, "help") == 0) {
		Size = IRC->GetQueueLow()->GetLength();
		IRC->GetQueueLow()->Clear();
	} else if (strcasecmp(Queue, "all") == 0) {
		Size = IRC->GetFloodControl()->GetRealLength();
		IRC->GetFloodControl()->Clear();
	} else {
		throw "Queue should be one of: mode server help all";
	}

	return Size;
}

int queuesize(const char* Queue) {
	int Size;
	CUser* User;
	
	User = g_Bouncer->GetUser(g_Context);

	if (User == NULL) {
		throw "Invalid user.";
	}

	CIRCConnection* IRC;
	
	IRC = User->GetIRCConnection();

	if (IRC == NULL) {
		return 0;
	}

	if (strcasecmp(Queue, "mode") == 0)
		Size = IRC->GetQueueHigh()->GetLength();
	else if (strcasecmp(Queue, "server") == 0)
		Size = IRC->GetQueueMiddle()->GetLength();
	else if (strcasecmp(Queue, "help") == 0)
		Size = IRC->GetQueueLow()->GetLength();
	else if (strcasecmp(Queue, "all") == 0)
		Size = IRC->GetFloodControl()->GetRealLength();
	else
		throw "Queue should be one of: mode server help all";

	return Size;
}

int puthelp(const char* text, const char *option) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		return 0;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	if (option != NULL && strcasecmp(option, "-next") == 0) {
		IRC->GetQueueLow()->QueueItemNext(text);
	} else {
		IRC->GetQueueLow()->QueueItem(text);
	}

	return 1;
}

int putquick(const char* text, const char *option) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	if (option != NULL && strcasecmp(option, "-next") == 0) {
		IRC->GetQueueHigh()->QueueItemNext(text);
	} else {
		IRC->GetQueueHigh()->QueueItem(text);
	}

	return 1;
}

const char* getisupport(const char* Feature) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return NULL;

	return IRC->GetISupport(Feature);
}

void setisupport(const char *Feature, const char *Value) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return;

	IRC->SetISupport(Feature, Value);
}

int requiresparam(char Mode) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	return IRC->RequiresParameter(Mode);
}

bool isprefixmode(char Mode) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (!IRC)
		return 0;

	return IRC->IsNickMode(Mode);	
}

const char* bncmodules(void) {
	static char* Buffer = NULL;

	const CVector<CModule *> *Modules = g_Bouncer->GetModules();

	if (Buffer)
		*Buffer = '\0';

	int a = 0;
	char** List = (char**)malloc(Modules->GetLength() * sizeof(const char*));

	for (int i = 0; i < Modules->GetLength(); i++) {
		char *BufId, *Buf1, *Buf2;
		int rc;

		rc = asprintf(&BufId, "%d", i);

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		rc = asprintf(&Buf1, "%p", Modules->Get(i)->GetHandle());

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		rc = asprintf(&Buf2, "%p", Modules->Get(i)->GetModule());

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		const char* Mod[4] = { BufId, Modules->Get(i)->GetFilename(), Buf1, Buf2 };

		List[a++] = Tcl_Merge(4, const_cast<char **>(Mod));

		free(BufId);
		free(Buf1);
		free(Buf2);
	}

	static char* Mods = NULL;

	if (Mods != NULL) {
		Tcl_Free(Mods);
	}

	Mods = Tcl_Merge(a, const_cast<char **>(List));

	for (int c = 0; c < a; c++)
		Tcl_Free(List[c]);

	free(List);

	return Mods;
}

int bncsettag(const char* channel, const char* nick, const char* tag, const char* value) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

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
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

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
	const CVector<CModule *> *Modules = g_Bouncer->GetModules();

	for (int i = 0; i < Modules->GetLength(); i++) {
		const char* Result = Modules->Get(i)->Command(Cmd, Parameters);

		if (Result)
			return Result;
	}

	return NULL;
}

const char* md5(const char* String, const char *Salt) {
	if (String)
		return g_Bouncer->MD5(String, Salt);
	else
		return NULL;
}

void debugout(const char* String) {
#ifdef _WIN32
	OutputDebugString(String);
	OutputDebugString("\n");
#endif /* _WIN32 */
}

void putlog(const char *Text) {
	CUser *User = g_Bouncer->GetUser(g_Context);

	if (User == NULL) {
		throw "Invalid user.";
	}

	if (Text != NULL) {
		User->Log("%s", Text);
	}
}

void putmainlog(const char *Text) {
	g_Bouncer->Log("%s", Text);
}


int trafficstats(const char* User, const char* ConnectionType, const char* Type) {
	CUser* Context = g_Bouncer->GetUser(User);

	if (!Context)
		throw "Invalid user.";

	unsigned int Bytes = 0;

	if (!ConnectionType || strcasecmp(ConnectionType, "client") == 0) {
		if (!Type || strcasecmp(Type, "in") == 0) {
			Bytes += Context->GetClientStats()->GetInbound();
		}

		if (!Type || strcasecmp(Type, "out") == 0) {
			Bytes += Context->GetClientStats()->GetOutbound();
		}
	}

	if (!ConnectionType || strcasecmp(ConnectionType, "server") == 0) {
		if (!Type || strcasecmp(Type, "in") == 0) {
			Bytes += Context->GetIRCStats()->GetInbound();
		}

		if (!Type || strcasecmp(Type, "out") == 0) {
			Bytes += Context->GetIRCStats()->GetOutbound();
		}
	}

	return Bytes;
}

void bncjoinchans(const char* User) {
	CUser* Context = g_Bouncer->GetUser(User);

	if (!Context)
		throw "Invalid user.";

	if (Context->GetIRCConnection())
		Context->GetIRCConnection()->JoinChannels();
}

int internallisten(unsigned short Port, const char* Type, const char* Options,
		const char* Flag, bool SSL, const char *BindIp) {
	if (strcasecmp(Type, "script") == 0) {
		if (Options == NULL)
			throw "You need to specifiy a control proc.";

		if (BindIp == NULL || BindIp[0] == '\0') {
			BindIp = g_Bouncer->GetConfig()->ReadString("system.ip");
		}

		CTclSocket* TclSocket = new CTclSocket(Port, BindIp, Options, SSL);

		if (!TclSocket)
			throw "Could not create object.";
		else if (!TclSocket->IsValid()) {
			static_cast<CSocketEvents*>(TclSocket)->Destroy();
			throw "Could not create listener.";
		}

		return TclSocket->GetIdx();
	} else if (strcasecmp(Type, "off") == 0) {
		int i = 0;
		const socket_t* Socket;

		while ((Socket = g_Bouncer->GetSocketByClass("CTclSocket", i++)) != NULL) {
			sockaddr_in saddr;
			socklen_t saddrSize = sizeof(saddr);
			getsockname(Socket->PollFd->fd, (sockaddr*)&saddr, &saddrSize);

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
	char *Buf;
	int rc = asprintf(&Buf, "%d", Socket);

	if (RcFailed(rc)) {
		g_Bouncer->Fatal();
	}

	CTclClientSocket* SockPtr = g_TclClientSockets->Get(Buf);
	gfree(Buf);

	if (!SockPtr || !g_Bouncer->IsRegisteredSocket(SockPtr))
		throw "Invalid socket.";

	SockPtr->SetControlProc(Proc);
}

int internalvalidsocket(int Socket) {
	char *Buf;
	int rc = asprintf(&Buf, "%d", Socket);

	if (RcFailed(rc)) {
		g_Bouncer->Fatal();
	}

	CTclClientSocket* SockPtr = g_TclClientSockets->Get(Buf);
	gfree(Buf);

	if (!SockPtr || !g_Bouncer->IsRegisteredSocket(SockPtr))
		return false;
	else
		return true;
}

void internalsocketwriteln(int Socket, const char* Line) {
	char *Buf;
	int rc = asprintf(&Buf, "%d", Socket);

	if (RcFailed(rc)) {
		g_Bouncer->Fatal();
	}

	CTclClientSocket* SockPtr = g_TclClientSockets->Get(Buf);
	gfree(Buf);

	if (!SockPtr || !g_Bouncer->IsRegisteredSocket(SockPtr))
		throw "Invalid socket pointer.";

	SockPtr->WriteUnformattedLine(Line);
}

int internalconnect(const char* Host, unsigned short Port, bool SSL) {
	SOCKET Socket = g_Bouncer->SocketAndConnect(Host, Port, NULL);

	if (Socket == INVALID_SOCKET)
		throw "Could not connect.";

	CTclClientSocket* Connectiton = new CTclClientSocket(Socket, SSL, Role_Client);

	return Connectiton->GetIdx();
}

const char *internalgetipforsocket(int Socket) {
	char *Buf;
	int rc = asprintf(&Buf, "%d", Socket);

	if (RcFailed(rc)) {
		g_Bouncer->Fatal();
	}

	CTclClientSocket* SockPtr = g_TclClientSockets->Get(Buf);
	gfree(Buf);

	if (!SockPtr || !g_Bouncer->IsRegisteredSocket(SockPtr))
		throw "Invalid socket pointer.";

	sockaddr *RemoteAddr = SockPtr->GetRemoteAddress();

	if (RemoteAddr == NULL) {
		return NULL;
	} else {
		return IpToString(RemoteAddr);
	}
}

void internalclosesocket(int Socket) {
	char *Buf;
	int rc = asprintf(&Buf, "%d", Socket);

	if (RcFailed(rc)) {
		g_Bouncer->Fatal();
	}

	CTclClientSocket* SockPtr = g_TclClientSockets->Get(Buf);
	gfree(Buf);

	if (!SockPtr || !g_Bouncer->IsRegisteredSocket(SockPtr))
		throw "Invalid socket pointer.";

	if (SockPtr->MayNotEnterDestroy())
		SockPtr->DestroyLater();
	else
		SockPtr->Destroy();
}

bool bnccheckpassword(const char* User, const char* Password) {
	CUser* Context = g_Bouncer->GetUser(User);

	if (!Context)
		throw "Invalid user.";

	bool Ret = Context->CheckPassword(Password);

	return Ret;
}

void bncdisconnect(const char* Reason) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	CIRCConnection* Irc = Context->GetIRCConnection();

	if (Irc) {
		Irc->Kill(Reason);
	}

	Context->MarkQuitted();
}

void bnckill(const char* Reason) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	CClientConnection* Client = Context->GetClientConnectionMultiplexer();

	if (!Client)
		return;

	Client->Kill(Reason);
}

void bncreply(const char* Text) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

	if (g_CurrentClient == NULL || Context != g_CurrentClient->GetOwner())
		g_CurrentClient = Context->GetClientConnectionMultiplexer();

	if (!g_CurrentClient)
		return;

	if (g_NoticeUser)
		g_CurrentClient->RealNotice(Text);
	else
		g_CurrentClient->Privmsg(Text);
}

char* chanbans(const char* Channel) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (!Context)
		throw "Invalid user.";

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
	while (const hash_t<ban_t *> *BanHash = Banlist->Iterate(i)) {
		char *Timestamp;
		const ban_t *Ban = BanHash->Value;

		int rc = asprintf(&Timestamp, "%d", (int)Ban->Timestamp);

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		char* ThisBan[3] = { Ban->Mask, Ban->Nick, Timestamp };

		char* List = Tcl_Merge(3, const_cast<char **>(ThisBan));

		gfree(Timestamp);

		Blist = (char**)realloc(Blist, ++Bcount * sizeof(char*));

		Blist[Bcount - 1] = List;

		i++;
	}

	static char* AllBans = NULL;

	if (AllBans)
		Tcl_Free(AllBans);

	AllBans = Tcl_Merge(Bcount, const_cast<char **>(Blist));

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

	if (!Cookie->timer->GetRepeat()) {
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
		if (g_Timers[i] && strcmp(g_Timers[i]->proc, Proc) == 0 &&
				(!Parameter || !g_Timers[i]->param || strcmp(Parameter, g_Timers[i]->param) == 0)) {
			g_Timers[i]->timer->Destroy();
			free(g_Timers[i]->proc);
			free(g_Timers[i]->param);
			free(g_Timers[i]);

			g_Timers[i] = NULL;

			return 1;
		}
	}

	return 0;
}

char *internaltimers(void) {
	char **List = (char **)malloc(g_TimerCount * sizeof(char *));
	const char *Timer[4];
	char *Temp1, *Temp2;
	int Count = 0;

	for (int i = 0; i < g_TimerCount; i++) {
		int rc;

		if (g_Timers[i] == NULL) {
			continue;
		}

		Timer[0] = g_Timers[i]->proc;
		rc = asprintf(&Temp1, "%d", g_Timers[i]->timer->GetInterval());

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		Timer[1] = Temp1;
		rc = asprintf(&Temp2, "%d", g_Timers[i]->timer->GetRepeat());

		if (RcFailed(rc)) {
			g_Bouncer->Fatal();
		}

		Timer[2] = Temp2;

		Timer[3] = g_Timers[i]->param ? g_Timers[i]->param : "";

		List[Count++] = Tcl_Merge(4, const_cast<char **>(Timer));

		gfree(Temp1);
		gfree(Temp2);
	}

	static char *Out = NULL;

	if (Out != NULL) {
		Tcl_Free(Out);
	}

	Out = Tcl_Merge(Count, const_cast<char **>(List));

	for (int a = 0; a < Count; a++) {
		Tcl_Free(List[a]);
	}

	return Out;
}

const char* getcurrentnick(void) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		throw "Invalid user.";

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

bool bncisipblocked(const char* Ip) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		throw "Invalid user.";

	sockaddr_in Peer;

	Peer.sin_family = AF_INET;

	unsigned long addr = inet_addr(Ip);

#ifdef _WIN32
	Peer.sin_addr.S_un.S_addr = addr;
#else /* _WIN32 */
	Peer.sin_addr.s_addr = addr;
#endif /* _WIN32 */

	return Context->IsIpBlocked((sockaddr *)&Peer);
}

void bnclogbadlogin(const char* Ip) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		throw "Invalid user.";

	sockaddr_in Peer;

	Peer.sin_family = AF_INET;

	unsigned long addr = inet_addr(Ip);

#ifdef _WIN32
	Peer.sin_addr.S_un.S_addr = addr;
#else /* _WIN32 */
	Peer.sin_addr.s_addr = addr;
#endif /* _WIN32 */

	return Context->LogBadLogin((sockaddr *)&Peer);
}

bool bncvalidusername(const char* Name) {
	return g_Bouncer->IsValidUsername(Name);
}

bool bncvaliduser(const char* Name) {
	return (g_Bouncer->GetUser(Name) != NULL);
}

int bncgetsendq(void) {
	return g_Bouncer->GetSendqSize();
}

void bncsetsendq(int NewSize) {
	g_Bouncer->SetSendqSize(NewSize);
}

bool synthwho(const char *Channel, bool Simulate) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		throw "Invalid user.";

	if (g_CurrentClient == NULL || Context != g_CurrentClient->GetOwner())
		g_CurrentClient = Context->GetClientConnectionMultiplexer();

	if (!g_CurrentClient)
		return false;

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (IRC == NULL)
		return false;

	CChannel* ChannelObj = IRC->GetChannel(Channel);

	if (ChannelObj == NULL)
		return false;

	return ChannelObj->SendWhoReply(g_CurrentClient, Simulate);
}

const char *impulse(int imp) {
	return g_Bouncer->DebugImpulse(imp);
}

void bncaddcommand(const char *Name, const char *Category, const char *Description, const char *HelpText) {
	CUser *Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		throw "Invalid user.";

	if (g_CurrentClient == NULL || Context != g_CurrentClient->GetOwner())
		g_CurrentClient = Context->GetClientConnectionMultiplexer();

	if (!g_CurrentClient)
		return;

	commandlist_t *List = g_CurrentClient->GetCommandList();

	AddCommand(List, Name, Category, Description, HelpText);
}

void bncdeletecommand(const char *Name) {
	CUser *Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		throw "Invalid user.";

	if (g_CurrentClient == NULL || Context != g_CurrentClient->GetOwner())
		g_CurrentClient = Context->GetClientConnectionMultiplexer();

	if (!g_CurrentClient)
		return;

	commandlist_t *List = g_CurrentClient->GetCommandList();

	DeleteCommand(List, Name);
}

void bncsetglobaltag(const char *Tag, const char *Value) {
	g_Bouncer->SetTagString(Tag, Value);
}

const char *bncgetglobaltag(const char *Tag) {
	return g_Bouncer->GetTagString(Tag);
}

const char *bncgetglobaltags(void) {
	CConfig *Config = g_Bouncer->GetConfig();
	int Count = Config->GetLength();
	const char *Item;

	int argc = 0;
	const char** argv = (const char**)malloc(Count * sizeof(const char*));

	while ((Item = g_Bouncer->GetTagName(argc)) != NULL) {
		argv[argc] = Item;
		argc++;
	}

	static char* List = NULL;

	if (List != NULL) {
		Tcl_Free(List);
	}

	List = Tcl_Merge(argc, const_cast<char **>(argv));

	free(argv);

	return List;
}

int hijacksocket(void) {
	clientdata_t ClientData;
	CTclClientSocket *TclSocket;

	if (g_CurrentClient == NULL) {
		throw "No client object available.";
	}

	ClientData = g_CurrentClient->Hijack();
	g_CurrentClient = NULL;

	if (ClientData.Socket == INVALID_SOCKET) {
		throw "Invalid client object.";
	}

	TclSocket = new CTclClientSocket(ClientData.Socket);

	if (TclSocket == NULL) {
		throw "TclSocket could not be instantiated.";
	}

	TclSocket->SetSendQ(ClientData.SendQ);
	TclSocket->SetRecvQ(ClientData.RecvQ);
	TclSocket->SetSSLObject(ClientData.SSLObject);

	return TclSocket->GetIdx();
}

const char *getusermodes(void) {
	CUser* Context = g_Bouncer->GetUser(g_Context);

	if (Context == NULL)
		throw "Invalid user.";

	CIRCConnection* IRC = Context->GetIRCConnection();

	if (IRC == NULL)
		return NULL;

	return IRC->GetUsermodes();
}

int bncgetreslimit(const char *Resource, const char *User) {
	CUser *UserObj;

	if (User != NULL) {
		UserObj = g_Bouncer->GetUser(User);

		if (UserObj == NULL) {
			throw "There is no such user.";
		}
	} else {
		UserObj = NULL;
	}

	return g_Bouncer->GetResourceLimit(Resource, UserObj);
}

void bncsetreslimit(const char *Resource, int NewLimit, const char *User) {
	CUser *UserObj;

	if (User != NULL) {
		UserObj = g_Bouncer->GetUser(User);

		if (UserObj == NULL) {
			throw "There is no such user.";
		}
	} else {
		UserObj = NULL;
	}

	g_Bouncer->SetResourceLimit(Resource, NewLimit, UserObj);
}

void setchannelsortvalue(int Value) {
	g_ChannelSortValue = Value;
}

void TclDnsLookupCallback(void *CookiePtr, hostent *Response) {
	tcldnsquery_t Cookie = *(tcldnsquery_t *)CookiePtr;
	const char *ip, *host, *code;
	int objc = 5;
	Tcl_Obj* objv[5];
 
	if (Response == NULL) {
		code = "0";
 
		if (Cookie.reverse) {
			ip = Cookie.host;
			host = Cookie.host;
		} else {
			if (Cookie.ipv6) {
				ip = "::0";
			} else {
				ip = "0.0.0.0";
			}
 
			host = Cookie.host;
		}
	} else {
		code = "1";
 
		if (Cookie.reverse) {
			ip = Cookie.host;
			host = Response->h_name;
		} else {
			const sockaddr *sin;
 
			sin = HostEntToSockAddr(Response);
 
			host = Cookie.host;
 
			if (sin != NULL) {
				ip = IpToString(const_cast<sockaddr *>(sin));
			} else {
				ip = host;
			}
		}
	}
 
	objv[0] = Tcl_NewStringObj(Cookie.proc, -1);
	Tcl_IncrRefCount(objv[0]);
 
	objv[1] = Tcl_NewStringObj(ip, -1);
	Tcl_IncrRefCount(objv[1]);
 
	objv[2] = Tcl_NewStringObj(host, -1);
	Tcl_IncrRefCount(objv[2]);
 
	objv[3] = Tcl_NewStringObj(code, -1);
	Tcl_IncrRefCount(objv[3]);
 
	if (Cookie.param) {
		objv[4] = Tcl_NewStringObj(Cookie.param, -1);
		Tcl_IncrRefCount(objv[4]);
	} else {
		objc--;
	}
 
	Tcl_EvalObjv(g_Interp, objc, objv, TCL_EVAL_GLOBAL);
 
	if (Cookie.param) {
		Tcl_DecrRefCount(objv[4]);
	}
 
	Tcl_DecrRefCount(objv[3]);
	Tcl_DecrRefCount(objv[2]);
	Tcl_DecrRefCount(objv[1]);
	Tcl_DecrRefCount(objv[0]);
 
	free(Cookie.proc);
	free(Cookie.param);
	free(Cookie.host);
	free(CookiePtr);
}
 
int internaldnslookup(const char *host, const char *tclproc, int reverse, int ipv6, const char *param) {
	tcldnsquery_t *Cookie;
 
	if (tclproc == NULL) {
		return 1;
	}
 
	Cookie = (tcldnsquery_t *)malloc(sizeof(tcldnsquery_t));
 
	if (Cookie == NULL) {
		return 1;
	}
 
	Cookie->reverse = (reverse != 0);
	Cookie->proc = strdup(tclproc);
	Cookie->param = param ? strdup(param) : NULL;
	Cookie->host = strdup(host);
	Cookie->ipv6 = (ipv6 != 0);
 
	int af;
 
	if (ipv6) {
		af = AF_INET6;
	} else {
		af = AF_INET;
	}
 
	CDnsQuery *Query = new CDnsQuery(Cookie, TclDnsLookupCallback, af);
 
	if (!reverse) {
		Query->GetHostByName(host, af);
	} else {
		sockaddr_storage sin;
 
		if (!StringToIp(host, af, (sockaddr *)&sin, sizeof(sin))) {
			throw "Failed to parse IP address.";
		}

		Query->GetHostByAddr((sockaddr *)&sin);
	}
 
	return 0;
}

const char *bncconfigdir(void) {
	return g_Bouncer->BuildPathConfig("");
}

const char *bncmoduledir(void) {
	return g_Bouncer->BuildPathModule("");
}

const char *bncshareddir(void) {
    return g_Bouncer->BuildPathShared("");
}

const char *bncexedir(void) {
    return g_Bouncer->BuildPathExe("");
}
