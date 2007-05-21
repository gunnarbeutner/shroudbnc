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

#define NOADNSLIB
#include "../StdAfx.h"
#include "Service.h"

#ifdef _MSC_VER
#ifndef _DEBUG
#define SBNC_MODULE "sbnc.dll"
#else
#define SBNC_MODULE "..\\..\\..\\Debug\\sbnc.dll"
#endif
#else
#define SBNC_MODULE "./lib-0/libsbnc.la"
#endif

#ifndef _WIN32
#define MAX_PATH 260
#endif

CAssocArray *g_Box;
char *g_Mod;
bool g_Freeze, g_Signal;
const char *g_Arg0;
bool g_Service = false;

sbncLoad g_LoadFunc = NULL;
//sbncSetStatus g_SetStatusFunc = NULL;

#define safe_printf(...)

/**
 * sigint_handler
 *
 * The "signal" handler for SIGINT (i.e. Ctrl+C).
 */
#ifndef _WIN32
void sigint_handler(int code) {
	// TODO: fixfixfix
	/*if (g_SetStatusFunc != NULL) {
		g_SetStatusFunc(STATUS_SHUTDOWN);

		signal(SIGINT, SIG_IGN);
	}*/
}
#else
BOOL WINAPI sigint_handler(DWORD Code) {
	const char *HRCode;

	switch (Code) {
		case CTRL_C_EVENT:
			HRCode = "Control-C";
			break;
		case CTRL_BREAK_EVENT:
			HRCode = "Control-Break";
			break;
		case CTRL_CLOSE_EVENT:
			HRCode = "Close/End Task";
			break;
		case CTRL_LOGOFF_EVENT:
			HRCode = "User logging off";

			if (g_Service) {
				return TRUE;
			}

			break;
		case CTRL_SHUTDOWN_EVENT:
			HRCode = "System shutting down";
			break;
		default:
			HRCode = "Unknown code";
	}

	/*if (g_SetStatusFunc != NULL) {
		safe_printf("Control code received (%s).\n", HRCode);

		g_SetStatusFunc(STATUS_SHUTDOWN);
	}*/

	Sleep(10000);

	return TRUE;
}
#endif

#ifndef _WIN32

void Socket_Init(void) {}
void Socket_Final(void) {}

#define Sleep(x) usleep(x * 1000)

void sig_usr1(int code) {
	signal(SIGUSR1, SIG_IGN);

	if (g_SetStatusFunc(STATUS_FREEZE))
		g_Freeze = true;
	else
		signal(SIGUSR1, sig_usr1);
}

#else

void Socket_Init(void) {
	WSADATA wsaData;

	WSAStartup(MAKEWORD(1,1), &wsaData);
}

void Socket_Final(void) {
	WSACleanup();
}

#endif

HMODULE sbncLoadModule(void) {
	HMODULE hMod;

	hMod = LoadLibrary(g_Mod);

	safe_printf("Loading shroudBNC from %s\n", g_Mod);

	if (hMod == NULL) {
		safe_printf("Module could not be loaded.");

#if !defined(_WIN32) || defined(__MINGW32__)
		safe_printf(" %s", lt_dlerror());
#endif

		safe_printf("\n");

		return NULL;
	}

	g_LoadFunc = (sbncLoad)GetProcAddress(hMod, "sbncLoad");
	//g_SetStatusFunc = (sbncSetStatus)GetProcAddress(hMod, "sbncSetStatus");

	if (g_LoadFunc == NULL) {
		safe_printf("Function \"sbncLoad\" does not exist in the module.\n");

		FreeLibrary(hMod);

		return NULL;
	}

	/*if (g_SetStatusFunc == NULL) {
		safe_printf("Function \"sbncSetStatus\" does not exist in the module.\n");

		FreeLibrary(hMod);

		return NULL;
	}*/
	
	return hMod;
}

int main(int argc, char **argv) {
	int ExitCode = EXIT_SUCCESS;
	HMODULE hMod;
	char *ThisMod;
	char ExeName[MAX_PATH];
	char TclLibrary[512];

	Socket_Init();

	if (argc <= 1 || strcasecmp(argv[1], "--rpc-child") != 0) {
		PipePair_t Pipes;

		RpcInvokeClient(argv[0], &Pipes);
		RpcRunServer(Pipes);

		return 0;
	}

	g_Arg0 = argv[0];

#ifdef _WIN32
	if (!GetEnvironmentVariable("TCL_LIBRARY", TclLibrary, sizeof(TclLibrary)) || strlen(TclLibrary) == 0) {
		SetEnvironmentVariable("TCL_LIBRARY", "./tcl8.4");
	}

	SetConsoleCtrlHandler(sigint_handler, TRUE);
#else
	signal(SIGINT, sigint_handler);
#endif

	safe_printf("shroudBNC loader\n");

#if defined (_WIN32) && !defined(NOSERVICE)
	if (argc > 1) {
		if (strcasecmp(argv[1], "--install") == 0) {
			GetModuleFileName(NULL, ExeName, sizeof(ExeName));

			if (InstallService(ExeName)) {
				safe_printf("Service was successfully installed.\n");
			} else {
				safe_printf("Service could not be installed.\n");
			}

			return 0;
		} else if (strcasecmp(argv[1], "--uninstall") == 0) {
			if (UninstallService()) {
				safe_printf("Service was successfully removed.\n");
			} else {
				safe_printf("Service could not be removed.\n");
			}

			return 0;
		} else if (strcasecmp(argv[1], "--service") == 0) {
			ServiceMain();

			return 0;
		}
	}
#endif

	g_Mod = strdup(SBNC_MODULE);
	ThisMod = strdup(g_Mod);

#if !defined(_WIN32) || defined(__MINGW32__)
	lt_dlinit();
#endif

	hMod = sbncLoadModule();

	if (hMod == NULL && strcasecmp(g_Mod, SBNC_MODULE) != 0) {
		free(g_Mod);
		free(ThisMod);
		g_Mod = strdup(SBNC_MODULE);
		ThisMod = strdup(g_Mod);

		safe_printf("Trying failsafe module...\n");

		hMod = sbncLoadModule();

		if (hMod == NULL) {
			safe_printf("Giving up...\n");

			free(g_Mod);
			free(ThisMod);

			return EXIT_FAILURE;
		}
	}

	if (hMod == NULL) {
		return 1;
	}

	g_Signal = false;

	Sleep(10000);
	DebugBreak();

	g_LoadFunc(ThisMod, argc, argv);

	Socket_Final();

#if !defined(_WIN32) || defined(__MINGW32__)
	lt_dlexit();
#endif

	free(ThisMod);
	free(g_Mod);

	return ExitCode;
}
