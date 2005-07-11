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
#include "BouncerConfig.h"
#include "BouncerUser.h"
#include "BouncerCore.h"
#include "ModuleFar.h"
#include "Module.h"
#include "Hashtable.h"
#include "utility.h"

#ifndef _WIN32
typedef __sighandler_t sighandler_t;
#endif

CBouncerCore* g_Bouncer;

//#ifdef ASYNC_DNS
adns_state g_adns_State;
//#endif

#ifndef _WIN32
void sigint_handler(int code) {
	g_Bouncer->Log("SIGINT received.");
	g_Bouncer->GlobalNotice("SIGINT received.", true);

	g_Bouncer->Shutdown();
}
#endif

int main(int argc, char* argv[]) {
#ifndef _WIN32
	rlimit core_limit = { INT_MAX, INT_MAX };
	setrlimit(RLIMIT_CORE, &core_limit);
#endif

	Socket_Init();

//#ifdef ASYNC_DNS
	adns_init(&g_adns_State, adns_if_noerrprint, NULL);
//#endif

	CBouncerConfig* Config = new CBouncerConfig("sbnc.conf");

	// constructor sets g_Bouncer
	new CBouncerCore(Config, argc, argv);

#if defined(_WIN32) && defined(_DEBUG)
	g_Bouncer->CreateTimer(5, 1, ReportMemory, NULL);
#endif

#if !defined(_WIN32) && !defined(_FREEBSD)
	sighandler_t oldhandler = signal(SIGINT, sigint_handler);
#endif

	g_Bouncer->StartMainLoop();

#if !defined(_WIN32) && !defined(_FREEBSD)
	signal(SIGINT, oldhandler);
#endif

	delete g_Bouncer;
	delete Config;

//#ifdef ASYNC_DNS
	adns_finish(g_adns_State);
//#endif

	Socket_Final();

	return 0;
}
