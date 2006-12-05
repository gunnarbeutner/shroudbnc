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
 * sbncLoad
 *
 * Used by "sbncloader" to start shroudBNC
 */
extern "C" EXPORT int sbncLoad(loaderparams_t *Parameters) {
	CConfigFile *Config;

	if (Parameters->Version != 202) {
		printf("Incompatible loader version. Expected version 202, got %d.\n"
				"You might want to read the README.issues file for more information about this problem.\n", Parameters->Version);

		return 1;
	}

	srand((unsigned int)time(NULL));

#if defined(_DEBUG) && defined(_WIN32)
	SetUnhandledExceptionFilter(GuardPageHandler);

	SymInitialize(GetCurrentProcess(), NULL, TRUE);
#endif

#ifndef _WIN32
	if (getuid() == 0 || geteuid() == 0 || getgid() == 0 || getegid() == 0) {
		printf("You cannot run shroudBNC as 'root' or using an account which has 'wheel' privileges. Use an ordinary user account and remove the suid bit if it is set.\n");
		return EXIT_FAILURE;
	}

	rlimit core_limit = { INT_MAX, INT_MAX };
	setrlimit(RLIMIT_CORE, &core_limit);
#endif

#if !defined(_WIN32 ) || defined(__MINGW32__)
	lt_dlinit();
#endif

	time(&g_CurrentTime);

	Config = new CConfigFile(Parameters->BuildPath("sbnc.conf", NULL), NULL);

	if (Config == NULL) {
		printf("Fatal: could not create config object.");

#if !defined(_WIN32 ) || defined(__MINGW32__)
		lt_dlexit();
#endif

		return EXIT_FAILURE;
	}

	g_LoaderParameters = Parameters;

	// constructor sets g_Bouncer
	new CCore(Config, Parameters->argc, Parameters->argv);

	if (Parameters->Box) {
		g_Bouncer->Thaw(Parameters->Box);
	}

#if !defined(_WIN32)
	signal(SIGPIPE, SIG_IGN);
#endif

	g_Bouncer->StartMainLoop();

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

	return EXIT_SUCCESS;
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
const char *DebugGetModulePath(void) {
	return "./lib-0/";
}

const char *DebugBuildPath(const char *Filename, const char *Base) {
	size_t Size;
	static char *Path = NULL;

	if (Filename[0] == '/') {
		return Filename;
	}

	if (Base == NULL) {
		return Filename;
	}

	free(Path);

	Size = strlen(Filename) + strlen(Base) + 2;
	Path = (char *)malloc(Size);
	strmcpy(Path, Base, Size);
	strmcat(Path, "/", Size);
	strmcat(Path, Filename, Size);

	return Path;
}

void DebugSigEnable(void) {}

int main(int argc, char **argv) {
	loaderparams_s p;

	p.Version = 202;
	p.argc = argc;
	p.argv = argv;
	p.basepath = ".";
	p.BuildPath = DebugBuildPath;
	p.GetModulePath = DebugGetModulePath;
	p.SigEnable = DebugSigEnable;
	p.Box = NULL;

	sbncLoad(&p);

	return 0;
}
