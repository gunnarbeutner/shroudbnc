/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007 Gunnar Beutner                                      *
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

int g_ArgC;
char **g_ArgV;
const char *g_ModulePath;

#if defined(IPV6) && defined(__MINGW32__)
const struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
#endif

const char *sbncGetBaseName(void) {
	static char *BasePath = NULL;

	if (BasePath != NULL) {
		return BasePath;
	}

#ifndef _WIN32
	size_t Len;

	if (g_ArgV[0][0] == '.' || g_ArgV[0][0] == '/') {
		Len = strlen(g_ArgV[0]) + 1;
		BasePath = (char *)malloc(Len);
		strncpy(BasePath, g_ArgV[0], Len);
	}

	// TODO: look through PATH env

	for (int i = strlen(BasePath) - 1; i >= 0; i--) {
		if (BasePath[i] == '/') {
			BasePath[i] = '\0';

			break;
		}
	}
#else
	BasePath = (char *)malloc(MAXPATHLEN);

	GetModuleFileName(NULL, BasePath, MAXPATHLEN);

	PathRemoveFileSpec(BasePath);
#endif

	return BasePath;
}

bool sbncIsAbsolutePath(const char *Path) {
#ifdef _WIN32
	return !PathIsRelative(Path);
#else
	if (Path[0] == '/') {
		return true;
	}

	return false;
#endif
}

void sbncPathCanonicalize(char *NewPath, const char *Path) {
	int i = 0, o = 0;

	while (true) {
		if ((Path[i] == '\\' || Path[i] == '/') && Path[i + 1] == '.' && Path[i + 1] != '\0' && Path[i + 2] != '.') {
			i += 2;
		}

		if (o >= MAXPATHLEN - 1) {
			NewPath[o] = '\0';

			return;
		} else {
			NewPath[o] = Path[i];
		}

		if (Path[i] == '\0') {
			return;
		}

		i++;
		o++;
	}
}

const char *sbncBuildPath(const char *Filename, const char *BasePath) {
	char NewPath[MAXPATHLEN];
	static char *Path = NULL;
	size_t Len;

	if (sbncIsAbsolutePath(Filename)) {
		return Filename;
	}

	free(Path);

	if (BasePath == NULL) {
		BasePath = sbncGetBaseName();

		// couldn't find the basepath, fall back to relative paths (i.e. Filename)
		if (BasePath == NULL) {
			return Filename;
		}
	}

	Len = strlen(BasePath) + 1 + strlen(Filename) + 1;
	Path = (char *)malloc(Len);
	strncpy(Path, BasePath, Len);
#ifdef _WIN32
	strncat(Path, "\\", Len);
#else
	strncat(Path, "/", Len);
#endif
	strncat(Path, Filename, Len);

#ifdef _WIN32
	for (unsigned int i = 0; i < strlen(Path); i++) {
		if (Path[i] == '/') {
			Path[i] = '\\';
		}
	}
#endif

	sbncPathCanonicalize(NewPath, Path);
	
	strncpy(Path, NewPath, sizeof(Path));

	return Path;
}

/**
 * sbncLoad
 *
 * Used by "sbncloader" to start shroudBNC
 */
extern "C" EXPORT int sbncLoad(const char *ModulePath, bool LPC, bool Daemonize, int argc, char **argv) {
	char TclLibrary[512];
	CConfigFile *Config;

#ifdef _WIN32
	_setmode(fileno(stdin), O_BINARY);
	_setmode(fileno(stdout), O_BINARY);
#endif

	Sleep(10000);

	RpcSetLPC(LPC);

	safe_reinit();

	time_t LastResurrect = safe_get_integer(NULL, "ResurrectTimestamp");

	if (LastResurrect > time(NULL) - 30) {
		// We're dying way too often
		safe_exit(6);
	}

	safe_put_integer(NULL, "ResurrectTimestamp", time(NULL));

	int ResurrectCount = safe_get_integer(NULL, "Resurrect");
	ResurrectCount++;
	safe_put_integer(NULL, "Resurrect", ResurrectCount);

	g_ArgC = argc;
	g_ArgV = argv;
	g_ModulePath = ModulePath;

	chdir(sbncBuildPath("."));

#ifdef _WIN32
	if (!GetEnvironmentVariable("TCL_LIBRARY", TclLibrary, sizeof(TclLibrary)) || strlen(TclLibrary) == 0) {
		SetEnvironmentVariable("TCL_LIBRARY", "./tcl8.4");
	}
#endif

	safe_box_t box = safe_put_box(NULL, "hello");

	safe_put_string(box, "hi", "world");

	safe_remove(box, "hi");
	safe_remove(NULL, "hello");

	srand((unsigned int)time(NULL));

#if defined(_DEBUG) && defined(_WIN32)
	SetUnhandledExceptionFilter(GuardPageHandler);

	SymInitialize(GetCurrentProcess(), NULL, TRUE);
#endif

#ifndef _WIN32
	if (getuid() == 0 || geteuid() == 0 || getgid() == 0 || getegid() == 0) {
		safe_printf("You cannot run shroudBNC as 'root' or using an account which has 'wheel' privileges. Use an ordinary user account and remove the suid bit if it is set.\n");
		return EXIT_FAILURE;
	}

	rlimit core_limit = { INT_MAX, INT_MAX };
	setrlimit(RLIMIT_CORE, &core_limit);
#endif

#if !defined(_WIN32 ) || defined(__MINGW32__)
	lt_dlinit();
#endif

	time(&g_CurrentTime);

	Config = new CConfigFile(sbncBuildPath("sbnc.conf", NULL), NULL);

	if (Config == NULL) {
		safe_printf("Fatal: could not create config object.");

#if !defined(_WIN32 ) || defined(__MINGW32__)
		lt_dlexit();
#endif

		return EXIT_FAILURE;
	}

	// constructor sets g_Bouncer
	new CCore(Config, argc, argv, Daemonize);

#if !defined(_WIN32)
	signal(SIGPIPE, SIG_IGN);
#endif

	g_Bouncer->StartMainLoop();

	if (g_Bouncer != NULL) {
		delete g_Bouncer;
	}

	delete Config;

#if !defined(_WIN32 ) || defined(__MINGW32__)
	lt_dlexit();
#endif

	safe_exit(EXIT_SUCCESS);
	exit(EXIT_SUCCESS);
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

const char *sbncGetModulePath(void) {
	return g_ModulePath;
}
