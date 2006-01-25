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

#define SBNC
#include "sbnc.h"

CBouncerCore *g_Bouncer = NULL;
bool g_Freeze;
loaderparams_s *g_LoaderParameters;
adns_state g_adns_State;

#ifndef _WIN32
/**
 * sigint_handler
 *
 * The "signal" handler for SIGINT (i.e. Ctrl+C).
 */
void sigint_handler(int code) {
	g_Bouncer->Log("SIGINT received.");
	g_Bouncer->GlobalNotice("SIGINT received.", true);

	g_Bouncer->Shutdown();

	signal(SIGINT, SIG_IGN);
}
#endif

/**
 * sbncLoad
 *
 * Used by "sbncloader" to start shroudBNC
 */
extern "C" EXPORT int sbncLoad(loaderparams_s *Parameters) {
	if (Parameters->Version < 200) {
		printf("Incompatible loader version. Expected version 200, got %d.\n", Parameters->Version);

		return 1;
	}

	srand((unsigned int)time(NULL));

#ifndef _WIN32
	if (geteuid() == 0) {
		printf("You cannot run shroudBNC as 'root'. Use an ordinary user account and remove the suid bit if it is set.\n");
		return 1;
	}

	rlimit core_limit = { INT_MAX, INT_MAX };
	setrlimit(RLIMIT_CORE, &core_limit);

	lt_dlinit();
#endif

	adns_init(&g_adns_State, adns_if_noerrprint, NULL);

	CBouncerConfig* Config = new CBouncerConfig("sbnc.conf");

	if (Config == NULL) {
		printf("Fatal: could not create config object.");

#ifndef _WIN32
		lt_dlexit();
#endif

		return 1;
	}

	g_LoaderParameters = Parameters;

	// constructor sets g_Bouncer
	new CBouncerCore(Config, Parameters->argc, Parameters->argv);

	if (Parameters->Box)
		g_Bouncer->Unfreeze(Parameters->Box);

#if defined(_WIN32) && defined(_DEBUG)
	CTimer* DebugTimer = g_Bouncer->CreateTimer(15, 1, ReportMemory, NULL);
#endif

#if !defined(_WIN32)
	signal(SIGINT, sigint_handler);
	signal(SIGPIPE, SIG_IGN);
#endif

	g_Freeze = false;

	g_Bouncer->StartMainLoop();

#if !defined(_WIN32)
	signal(SIGINT, SIG_IGN);
#endif

#if defined(_WIN32) && defined(_DEBUG)
	DebugTimer->Destroy();

	ReportMemory(time(NULL), NULL);
#endif

	if (g_Bouncer) {
		if (g_Freeze) {
			CAssocArray *Box;

			Parameters->GetBox(&Box);

			g_Bouncer->Freeze(Box);
		} else {
			delete g_Bouncer;
		}
	}

	delete Config;

	adns_finish(g_adns_State);

#ifndef _WIN32
	lt_dlexit();
#endif

	return 0;
}

/**
 * sbncPrepareFreeze
 *
 * Used by "sbncloader" to notify shroudBNC that it is going
 * to be "freezed" (i.e. reloaded/restarted).
 */
extern "C" EXPORT bool sbncPrepareFreeze(void) {
	g_Freeze = true;

	return true;
}

/* for debugging */
/*int main(int argc, char **argv) {
	loaderparams_s p;

	p.Version = 200;
	p.argc = argc;
	p.argv = argv;
	p.Box = NULL;

	sbncLoad(&p);

	return 0;
}
*/
