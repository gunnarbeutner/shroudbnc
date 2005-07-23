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

#if !defined(_WIN32) && !defined(__FreeBSD__)
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
	if (geteuid() == 0) {
		printf("You cannot run sBNC as 'root'. Use an ordinary user account and remove the suid bit if it is set.\n");
		return 1;
	}

	rlimit core_limit = { INT_MAX, INT_MAX };
	setrlimit(RLIMIT_CORE, &core_limit);
#endif

	Socket_Init();

	adns_init(&g_adns_State, adns_if_noerrprint, NULL);

	CBouncerConfig* Config = new CBouncerConfig("sbnc.conf");

	// constructor sets g_Bouncer
	new CBouncerCore(Config, argc, argv);

#if defined(_WIN32) && defined(_DEBUG)
	CTimer* DebugTimer = g_Bouncer->CreateTimer(15, 1, ReportMemory, NULL);
#endif

#if !defined(_WIN32)
	sighandler_t oldhandler = signal(SIGINT, sigint_handler);
#endif

	g_Bouncer->StartMainLoop();

#if !defined(_WIN32)
	signal(SIGINT, oldhandler);
#endif

#if defined(_WIN32) && defined(_DEBUG)
	DebugTimer->Destroy();
#endif

	delete g_Bouncer;
	delete Config;

	adns_finish(g_adns_State);

	Socket_Final();

	return 0;
}
