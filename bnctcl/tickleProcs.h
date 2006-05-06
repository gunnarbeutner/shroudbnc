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

#ifdef SWIG
%module bnc
%{
#include "tickleProcs.h"
%}

%rename(rand) ticklerand;

%include "exception.i"
%exception {
	try {
		$function
	} catch (const char *Description) {
		SWIG_exception(SWIG_RuntimeError, const_cast<char *>(Description));
	}
}

%typemap(in) char * (Tcl_DString ds_, bool ds_use_ = false) {
	ds_use_ = true;
	$1 = Tcl_UtfToExternalDString(g_Encoding, Tcl_GetString($input), -1, &ds_);
}

%typemap(freearg) char * {
	if (ds_use_$argnum)
		Tcl_DStringFree(&ds_$argnum);
}

%typemap(out) char * {
	Tcl_DString ds_result;

	Tcl_SetObjResult(interp, Tcl_NewStringObj(Tcl_ExternalToUtfDString(g_Encoding, $1, -1, &ds_result),-1));
	Tcl_DStringFree(&ds_result);
}

%header %{
extern Tcl_Encoding g_Encoding;
%}

struct CTclSocket;
struct CTclClientSocket;

#else
int Tcl_ProcInit(Tcl_Interp* interp);

class CTclSocket;
class CTclClientSocket;
#endif

// exported procs, which are accessible via tcl

int putclient(const char* text);
const char *simul(const char* User, const char* Command);

int internalbind(const char* type, const char* proc, const char* pattern = 0, const char* user = 0);
int internalunbind(const char* type, const char* proc, const char* pattern = 0, const char* user = 0);

void setctx(const char* ctx);
const char* getctx(void);

const char* bncuserlist(void);
const char* getbncuser(const char* User, const char* Type, const char* Parameter2 = 0);
int setbncuser(const char* User, const char* Type, const char* Value = 0, const char* Parameter2 = 0);
void addbncuser(const char* User, const char* Password);
void delbncuser(const char* User);
bool bnccheckpassword(const char* User, const char* Password);

const char* internalchanlist(const char* Channel);

const char* bncversion(void);
const char* bncnumversion(void);
int bncuptime(void);

int floodcontrol(const char* Function);

const char* getisupport(const char* Feature);
void setisupport(const char *Feature, const char *Value);
int requiresparam(char Mode);
bool isprefixmode(char Mode);
const char* getchanprefix(const char* Channel, const char* Nick);

const char* internalchannels(void);
const char* bncmodules(void);

int bncsettag(const char* channel, const char* nick, const char* tag, const char* value);
const char* bncgettag(const char* channel, const char* nick, const char* tag);
void haltoutput(void);

const char* bnccommand(const char* Cmd, const char* Parameters);

const char* md5(const char* String);

void debugout(const char* String);

int internalgetchanidle(const char* Nick, const char* Channel);

void bncreply(const char* Text);

int trafficstats(const char* User, const char* ConnectionType = NULL, const char* Type = NULL);
void bncjoinchans(const char* User);

int internalvalidsocket(int Socket);
int internallisten(unsigned short Port, const char* Type, const char* Options = 0, const char* Flag = 0, bool SSL = false, const char *BindIp = NULL);
void internalsocketwriteln(int Socket, const char* Line);
int internalconnect(const char* Host, unsigned short Port, bool SSL = false);
void internalclosesocket(int Socket);

int internaltimer(int Interval, bool Repeat, const char* Proc, const char* Parameter = 0);
int internalkilltimer(const char* Proc, const char* Parameter = 0);
char *internaltimers(void);

int timerstats(void);

void bncdisconnect(const char* Reason);
void bnckill(const char* Reason);

const char* getcurrentnick(void);

const char* internalbinds(void);

const char* bncgetmotd(void);
void bncsetmotd(const char* Motd);
const char* bncgetgvhost(void);
void bncsetgvhost(const char* GVHost);

const char* getbnchosts(void);
void delbnchost(const char* Host);
int addbnchost(const char* Host);
bool bncisipblocked(const char* Ip);
bool bnccanhostconnect(const char* Host);

bool bncvalidusername(const char* Name);

int bncgetsendq(void);
void bncsetsendq(int NewSize);

void bncaddcommand(const char *Name, const char *Category, const char *Description, const char *HelpText = 0);
void bncdeletecommand(const char *Name);

bool synthwho(const char *Channel, bool Simulate);
const char* getchanrealname(const char* Nick, const char* Channel = 0);

const char *impulse(int imp);

void bncsetglobaltag(const char *Tag, const char *Value = 0);
const char *bncgetglobaltag(const char *Tag);
const char *bncgetglobaltags(void);

const char *getusermodes(void);

const char *getzoneinfo(const char *Zone = 0);
const char *getallocinfo(void);

int hijacksocket(void);

void putmainlog(const char *Text);

// eggdrop compat
bool onchan(const char* Nick, const char* Channel = 0);
const char* topic(const char* Channel);
const char* topicnick(const char* Channel);
int topicstamp(const char* Channel);
const char* getchanmode(const char* Channel);
bool isop(const char* Nick, const char* Channel = 0);
bool isvoice(const char* Nick, const char* Channel = 0);
bool ishalfop(const char* Nick, const char* Channel = 0);
const char* getchanhost(const char* Nick, const char* Channel = 0);
void jump(const char *Server = 0, unsigned int Port = 0, const char *Password = 0);
void rehash(void);
void die(void);
int putserv(const char* text);
int getchanjoin(const char* Nick, const char* Channel);
int ticklerand(int limit);
int clearqueue(const char* Queue);
int queuesize(const char* Queue);
int puthelp(const char* text);
int putquick(const char* text);
void putlog(const char* Text);
char* chanbans(const char* Channel);

void control(int Socket, const char* Proc);
