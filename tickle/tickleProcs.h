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
%rename(bind) ticklebind;
#else
int Tcl_ProcInit(Tcl_Interp* interp);
#endif

// exported procs, which are accessible via tcl

const char* getuser(const char* Option);
int setuser(const char* Option, const char* Value);

const char* channels(void);
const char* users(void);

const char* channel(const char* Function, const char* Channel = 0, const char* Parameter = 0);
const char* user(const char* Function, const char* User = 0, const char* Parameter = 0, const char* Parameter2 = 0);

int putserv(const char* text);
int putclient(const char* text);
int simul(const char* User, const char* Command);

int ticklebind(const char* type, const char* proc);
int unbind(const char* type, const char* proc);

void jump(void);

void rehash(void);
void die(void);

void setctx(const char* ctx);
const char* getctx(void);
