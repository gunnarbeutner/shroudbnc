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
	} catch (const char* p) {
		SWIG_exception(SWIG_RuntimeError, const_cast<char*>(p));
	}
}

#else
int Tcl_ProcInit(Tcl_Interp* interp);
#endif

// exported procs, which are accessible via tcl

//const char* getuser(const char* Option);
//int setuser(const char* Option, const char* Value);

const char* bncuserlist(void);

const char* channel(const char* Function, const char* Channel = 0, const char* Parameter = 0);
const char* user(const char* Function, const char* User = 0, const char* Parameter = 0, const char* Parameter2 = 0);

int putclient(const char* text);
int simul(const char* User, const char* Command);

int internalbind(const char* type, const char* proc);
int internalunbind(const char* type, const char* proc);

void setctx(const char* ctx);
const char* getctx(void);

const char* getbncuser(const char* User, const char* Type, const char* Parameter2 = 0);
int setbncuser(const char* User, const char* Type, const char* Value = 0, const char* Parameter2 = 0);
void addbncuser(const char* User, const char* Password);
void delbncuser(const char* User);

const char* internalchanlist(const char* Channel);

const char* bncversion(void);
const char* bncnumversion(void);
int bncuptime(void);

int floodcontrol(const char* Function);

const char* getisupport(const char* Feature);
int requiresparam(char Mode);
bool isprefixmode(char Mode);

const char* internalchannels(void);

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
void jump(void);
void rehash(void);
void die(void);
int putserv(const char* text);
int getchanjoin(const char* Nick, const char* Channel);
int getchanidle(const char* Nick, const char* Channel);
const char* duration(int Interval);
int ticklerand(int limit);
int clearqueue(const char* Queue);
int queuesize(const char* Queue);
int puthelp(const char* text);
int putquick(const char* text);
