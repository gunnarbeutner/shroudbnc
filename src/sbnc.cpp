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

#ifndef SBNC
#define SBNC
#endif

#include "StdAfx.h"

CCore *g_Bouncer = NULL;
loaderparams_t *g_LoaderParameters;

#if defined(IPV6) && defined(__MINGW32__)
const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
#endif

/**
 * sigint_handler
 *
 * The "signal" handler for SIGINT (i.e. Ctrl+C).
 */
#ifndef _WIN32
void sigint_handler(int code) {
	g_Bouncer->Log("SIGINT received.");

	g_Bouncer->Shutdown();

	signal(SIGINT, SIG_IGN);
}
#else
BOOL WINAPI sigint_handler(DWORD Code) {
	g_Bouncer->Log("Control code received.");

	g_Bouncer->Shutdown();

	return FALSE;
}
#endif

/**
 * sbncLoad
 *
 * Used by "sbncloader" to start shroudBNC
 */
extern "C" EXPORT int sbncLoad(loaderparams_t *Parameters) {
	CConfig *Config;

	if (Parameters->Version < 201) {
		printf("Incompatible loader version. Expected version 201, got %d.\n", Parameters->Version);

		return 1;
	}

	srand((unsigned int)time(NULL));

#ifndef _WIN32
	if (getuid() == 0 || geteuid() == 0 || getgid() == 0 || getegid() == 0) {
		printf("You cannot run shroudBNC as 'root'. Use an ordinary user account and remove the suid bit if it is set.\n");
		return 1;
	}

	rlimit core_limit = { INT_MAX, INT_MAX };
	setrlimit(RLIMIT_CORE, &core_limit);

#endif

#if !defined(_WIN32 ) || defined(__MINGW32__)
	lt_dlinit();
#endif

	Config = new CConfig(Parameters->BuildPath("sbnc.conf", NULL));

	if (Config == NULL) {
		printf("Fatal: could not create config object.");

#if !defined(_WIN32 ) || defined(__MINGW32__)
		lt_dlexit();
#endif

		return 1;
	}

	g_LoaderParameters = Parameters;

	// constructor sets g_Bouncer
	new CCore(Config, Parameters->argc, Parameters->argv);

	if (Parameters->Box) {
		g_Bouncer->Unfreeze(Parameters->Box);
	}

#if !defined(_WIN32)
	signal(SIGINT, sigint_handler);
	signal(SIGPIPE, SIG_IGN);
#else
	SetConsoleCtrlHandler(sigint_handler, TRUE);
#endif

	g_Bouncer->StartMainLoop();

#if !defined(_WIN32)
	signal(SIGINT, SIG_IGN);
#endif

	if (g_Bouncer != NULL) {
		if (g_Bouncer->GetStatus() == STATUS_FREEZE) {
			CAssocArray *Box;

			Parameters->GetBox(&Box);

			g_Bouncer->Freeze(Box);
		} else {
			delete g_Bouncer;
		}
	}

	delete Config;

#if !defined(_WIN32 ) || defined(__MINGW32__)
	lt_dlexit();
#endif

	return 0;
}

/**
 * sbncSetStatus
 *
 * Used by "sbncloader" to notify shroudBNC of status changes.
 */
extern "C" EXPORT bool sbncSetStatus(int Status) {
	if (g_Bouncer != NULL) {
		g_Bouncer->SetStatus(Status);
	}

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
