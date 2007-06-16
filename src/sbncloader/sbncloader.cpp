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

char *g_Mod;
bool g_Signal;
bool g_Service = false;

sbncLoad g_LoadFunc = NULL;

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

void sigchld_handler(int Code) {
	int Status;

	waitpid(-1, &Status, 0);
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

	return TRUE;
}
#endif

#ifndef _WIN32

void Socket_Init(void) {}
void Socket_Final(void) {}

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

	if (g_LoadFunc == NULL) {
		safe_printf("Function \"sbncLoad\" does not exist in the module.\n");

		FreeLibrary(hMod);

		return NULL;
	}

	return hMod;
}

bool sbncDaemonize(void) {
#ifndef _WIN32
	pid_t pid;
	pid_t sid;
	int fd;

	pid = fork();
	if (pid == -1) {
		return false;
	}

	if (pid) {
		fprintf(stdout, "DONE\n");
		exit(0);
	}

	fd = open("/dev/null", O_RDWR);
	if (fd) {
		if (fd != 0) {
			dup2(fd, 0);
		}

		if (fd != 1) {
			dup2(fd, 1);
		}

		if (fd != 2) {
			dup2(fd, 2);
		}

		if (fd > 2) {
			close(fd);
		}
	}

	sid = setsid();
	if (sid == -1) {
		return false;
	}
#endif

	return true;
}

void sbncReadIpcFile(void) {
	FILE *IpcFile;

	IpcFile = fopen("sbnc.ipc", "r");

	if (IpcFile != NULL) {
		char *n;
		char FileBuf[512];

		if ((n = fgets(FileBuf, sizeof(FileBuf) - 1, IpcFile)) != 0) {
			if (FileBuf[strlen(FileBuf) - 1] == '\n')
				FileBuf[strlen(FileBuf) - 1] = '\0';

			if (FileBuf[strlen(FileBuf) - 1] == '\r')
				FileBuf[strlen(FileBuf) - 1] = '\0';

			free(g_Mod);
			g_Mod = strdup(FileBuf);
		}

		fclose(IpcFile);
	}
}

int main(int argc, char **argv) {
	int Result;
	bool LPC = false, RpcChild = false;
	bool Install = false, Uninstall = false, Service = false;
	bool Daemonize = true, Usage = false;
	int ExitCode = EXIT_SUCCESS;
	HMODULE hMod;
#ifdef _WIN32
	char ExeName[MAXPATHLEN];
#endif
	char PathName[MAXPATHLEN];

#ifdef _WIN32
	GetModuleFileName(NULL, PathName, sizeof(PathName));
#else
	strncpy(PathName, argv[0], sizeof(PathName));
	PathName[sizeof(PathName) - 1] = '\0';
#endif

	for (char *p = PathName + strlen(PathName); p >= PathName; p--) {
		if (*p == '/' || *p == '\\') {
			*p = '\0';

			break;
		}
	}

	chdir(PathName);

	for (unsigned int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--rpc-child") == 0) {
			RpcChild = true;
		}

		if (strcmp(argv[i], "--lpc") == 0) {
			LPC = true;
		}

		if (strcmp(argv[i], "--install") == 0) {
			Install = true;
		}

		if (strcmp(argv[i], "--uninstall") == 0) {
			Uninstall = true;
		}

		if (strcmp(argv[i], "--service") == 0) {
			Service = true;
		}

		if (strcmp(argv[i], "--foreground") == 0) {
			Daemonize = false;
		}

		if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "/?") == 0) {
			Usage = true;
		}
	}

	struct stat ConfigStats;
	if (stat("sbnc.conf", &ConfigStats) != 0) {
		Daemonize = false;
	}

	Socket_Init();

	fprintf(stderr, "shroudBNC (loader: " BNCVERSION ") - an object-oriented IRC bouncer\n");

	if (Usage) {
		fprintf(stderr, "\n");
		fprintf(stderr, "Syntax: %s [OPTION]", argv[0]);
		fprintf(stderr, "\n");
		fprintf(stderr, "Options:\n");
		fprintf(stderr, "\t--help\t\tdisplay this help and exit\n");
		fprintf(stderr, "\t--foreground\trun in the foreground\n");
		fprintf(stderr, "\t--lpc\t\tdon't start a child process\n");
#ifdef _WIN32
		fprintf(stderr, "\t--install\tinstalls the win32 service\n");
		fprintf(stderr, "\t--uninstall\tuninstalls the win32 service\n");
#endif

		return 3;
	}

	if (RpcChild || Install || Uninstall || Service) {
		Daemonize = false;
	}

	if (Daemonize) {
		fprintf(stderr, "Daemonizing... ");

		if (sbncDaemonize()) {
			fprintf(stderr, "DONE\n");
		}
	} else if (!RpcChild) {
		LPC = true;
	}

	if (!RpcChild && !LPC && !Install && !Uninstall && !Service) {
		PipePair_t PipesLocal;

#ifndef _WIN32
		signal(SIGPIPE, SIG_IGN);
		signal(SIGCHLD, sigchld_handler);
#endif

		do {
			if (!RpcInvokeClient(argv[0], &PipesLocal, argc, argv)) {
				break;
			}

			Result = RpcRunServer(PipesLocal);

			if (PipesLocal.In != NULL) {
				fclose(PipesLocal.In);
			}

			if (PipesLocal.Out != NULL) {
				fclose(PipesLocal.Out);
			}
		} while (Result);

		return 0;
	}

#ifdef _WIN32
	SetConsoleCtrlHandler(sigint_handler, TRUE);
#else
	signal(SIGINT, sigint_handler);
#endif

	safe_printf("shroudBNC loader\n");

#if defined (_WIN32) && !defined(NOSERVICE)
	if (Install) {
		GetModuleFileName(NULL, ExeName, sizeof(ExeName));

		if (InstallService(ExeName)) {
			safe_printf("Service was successfully installed.\n");
		} else {
			safe_printf("Service could not be installed.\n");
		}

		return 0;
	} else if (Uninstall) {
		if (UninstallService()) {
			safe_printf("Service was successfully removed.\n");
		} else {
			safe_printf("Service could not be removed.\n");
		}

		return 0;
	} else if (Service) {
		ServiceMain();

		return 0;
	}
#endif

	g_Mod = strdup(SBNC_MODULE);

	sbncReadIpcFile();

#if !defined(_WIN32) || defined(__MINGW32__)
	lt_dlinit();
#endif

	hMod = sbncLoadModule();

	if (hMod == NULL && strcasecmp(g_Mod, SBNC_MODULE) != 0) {
		free(g_Mod);
		g_Mod = strdup(SBNC_MODULE);

		safe_printf("Trying failsafe module...\n");

		hMod = sbncLoadModule();

		if (hMod == NULL) {
			safe_printf("Giving up...\n");

			free(g_Mod);

			return EXIT_FAILURE;
		}
	}

	if (hMod == NULL) {
		return 1;
	}

	g_Signal = false;

	g_LoadFunc(g_Mod, LPC, Daemonize, argc, argv);

	Socket_Final();

#if !defined(_WIN32) || defined(__MINGW32__)
	lt_dlexit();
#endif

	free(g_Mod);

	return ExitCode;
}
