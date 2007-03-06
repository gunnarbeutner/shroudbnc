/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007 Gunnar Beutner                                           *
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
sbncSetStatus g_SetStatusFunc = NULL;

/**
 * sigint_handler
 *
 * The "signal" handler for SIGINT (i.e. Ctrl+C).
 */
#ifndef _WIN32
void sigint_handler(int code) {
	if (g_SetStatusFunc != NULL) {
		g_SetStatusFunc(STATUS_SHUTDOWN);

		signal(SIGINT, SIG_IGN);
	}
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

	if (g_SetStatusFunc != NULL) {
		printf("Control code received (%s).\n", HRCode);

		g_SetStatusFunc(STATUS_SHUTDOWN);
	}

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

void sbncSigEnable(void) {
#ifndef _WIN32
	signal(SIGUSR1, sig_usr1);
#endif
}

bool sbncGetBox(CAssocArray **Box) {
	delete g_Box;
	g_Box = NULL;

	g_Box = new CAssocArray();
	g_Freeze = true;
	g_Signal = true;

	*Box = g_Box;

	return true;
}

void sbncSetModulePath(const char *Module) {
	free(g_Mod);
	g_Mod = strdup(Module);
}

const char *sbncGetModulePath(void) {
	return g_Mod;
}

#ifndef _WIN32
void PathCanonicalize(char *NewPath, const char *Path) {
	int i = 0, o = 0;

	while (true) {
		if ((Path[i] == '\\' || Path[i] == '/') && Path[i + 1] == '.') {
			i += 2;
		}

		if (o >= MAX_PATH - 1) {
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
#endif

const char *sbncGetBaseName(const char *Arg0) {
	static char *BasePath = NULL;

	if (BasePath != NULL) {
		return BasePath;
	}

#ifndef _WIN32
	if (Arg0[0] == '.' || Arg0[0] == '/') {
		BasePath = (char *)malloc(strlen(Arg0) + 1);
		strcpy(BasePath, Arg0);
	}

	// TODO: look through PATH env

	for (int i = strlen(BasePath) - 1; i >= 0; i--) {
		if (BasePath[i] == '/') {
			BasePath[i] = '\0';

			break;
		}
	}
#else
	BasePath = (char *)malloc(MAX_PATH);

	GetModuleFileName(NULL, BasePath, MAX_PATH);

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

const char *sbncBuildPath(const char *Filename, const char *BasePath = NULL) {
	char NewPath[MAX_PATH];
	static char *Path = NULL;

	if (sbncIsAbsolutePath(Filename)) {
		return Filename;
	}

	free(Path);

	if (BasePath == NULL) {
		BasePath = sbncGetBaseName(g_Arg0);

		// couldn't find the basepath, fall back to relative paths (i.e. Filename)
		if (BasePath == NULL) {
			return Filename;
		}
	}

	Path = (char *)malloc(strlen(BasePath) + 1 + strlen(Filename) + 1);
	strcpy(Path, BasePath);
#ifdef _WIN32
	strcat(Path, "\\");
#else
	strcat(Path, "/");
#endif
	strcat(Path, Filename);

#ifdef _WIN32
	for (unsigned int i = 0; i < strlen(Path); i++) {
		if (Path[i] == '/') {
			Path[i] = '\\';
		}
	}
#endif

	PathCanonicalize(NewPath, Path);
	
	strcpy(Path, NewPath);

	return Path;
}

void sbncReadIpcFile(void) {
	FILE *IpcFile;

	IpcFile = fopen(sbncBuildPath("sbnc.ipc"), "r");

	if (IpcFile != NULL) {
		char *n;
		char FileBuf[512];

		if ((n = fgets(FileBuf, sizeof(FileBuf) - 1, IpcFile)) != 0) {
			if (FileBuf[strlen(FileBuf) - 1] == '\n')
				FileBuf[strlen(FileBuf) - 1] = '\0';

			if (FileBuf[strlen(FileBuf) - 1] == '\r')
				FileBuf[strlen(FileBuf) - 1] = '\0';

			free(g_Mod);
			g_Mod = strdup(sbncBuildPath(FileBuf));
		}

		fclose(IpcFile);
	}
}

HMODULE sbncLoadModule(void) {
	HMODULE hMod;

	hMod = LoadLibrary(g_Mod);

	printf("Loading shroudBNC from %s\n", g_Mod);

	if (hMod == NULL) {
		printf("Module could not be loaded.");

#if !defined(_WIN32) || defined(__MINGW32__)
		printf(" %s", lt_dlerror());
#endif

		printf("\n");

		return NULL;
	}

	g_LoadFunc = (sbncLoad)GetProcAddress(hMod, "sbncLoad");
	g_SetStatusFunc = (sbncSetStatus)GetProcAddress(hMod, "sbncSetStatus");

	if (g_LoadFunc == NULL) {
		printf("Function \"sbncLoad\" does not exist in the module.\n");

		FreeLibrary(hMod);

		return NULL;
	}

	if (g_SetStatusFunc == NULL) {
		printf("Function \"sbncSetStatus\" does not exist in the module.\n");

		FreeLibrary(hMod);

		return NULL;
	}
	
	return hMod;
}

int main(int argc, char **argv) {
	int ExitCode = EXIT_SUCCESS;
	HMODULE hMod;
	char *ThisMod;
	char ExeName[MAX_PATH];
	char TclLibrary[512];

	g_Arg0 = argv[0];

#ifdef _WIN32
	if (!GetEnvironmentVariable("TCL_LIBRARY", TclLibrary, sizeof(TclLibrary)) || strlen(TclLibrary) == 0) {
		SetEnvironmentVariable("TCL_LIBRARY", "./tcl8.4");
	}

	SetCurrentDirectory(sbncBuildPath("."));
	SetConsoleCtrlHandler(sigint_handler, TRUE);
#else
	chdir(sbncBuildPath("."));
	signal(SIGINT, sigint_handler);
#endif

	printf("shroudBNC loader\n");

#if defined (_WIN32) && !defined(NOSERVICE)
	if (argc > 1) {
		if (strcasecmp(argv[1], "--install") == 0) {
			GetModuleFileName(NULL, ExeName, sizeof(ExeName));

			if (InstallService(ExeName)) {
				printf("Service was successfully installed.\n");
			} else {
				printf("Service could not be installed.\n");
			}

			return 0;
		} else if (strcasecmp(argv[1], "--uninstall") == 0) {
			if (UninstallService()) {
				printf("Service was successfully removed.\n");
			} else {
				printf("Service could not be removed.\n");
			}

			return 0;
		} else if (strcasecmp(argv[1], "--service") == 0) {
			ServiceMain();

			return 0;
		}
	}
#endif

	g_Mod = strdup(sbncBuildPath(SBNC_MODULE));
	ThisMod = strdup(g_Mod);

	Socket_Init();

#if !defined(_WIN32) || defined(__MINGW32__)
	lt_dlinit();
#endif

	sbncReadIpcFile();

	hMod = sbncLoadModule();

	if (hMod == NULL && strcasecmp(g_Mod, SBNC_MODULE) != 0) {
		free(g_Mod);
		free(ThisMod);
		g_Mod = strdup(sbncBuildPath(SBNC_MODULE));
		ThisMod = strdup(g_Mod);

		printf("Trying failsafe module...\n");

		hMod = sbncLoadModule();

		if (hMod == NULL) {
			printf("Giving up...\n");

			free(g_Mod);
			free(ThisMod);

			return EXIT_FAILURE;
		}
	}

	if (hMod == NULL) {
		return 1;
	}

	loaderparams_s Parameters;

	Parameters.Version = 202;

	Parameters.argc = argc;
	Parameters.argv = argv;
	Parameters.basepath = sbncGetBaseName(argv[0]);

	Parameters.GetBox = sbncGetBox;
	Parameters.SigEnable = sbncSigEnable;

	Parameters.SetModulePath = sbncSetModulePath;
	Parameters.GetModulePath = sbncGetModulePath;

	Parameters.BuildPath = sbncBuildPath;

	g_Signal = false;

	while (true) {
		g_Freeze = false;

		Parameters.Box = g_Box;

		g_LoadFunc(&Parameters);

		if (!g_Freeze) {
			break;
		}

		printf("Unloading shroudBNC...\n");

		FreeLibrary(hMod);

		if (g_Signal == true)
			sbncReadIpcFile();

		g_Signal = false;

		hMod = sbncLoadModule();

		if (hMod == NULL) {
			free(g_Mod);
			free(ThisMod);
			g_Mod = strdup(sbncBuildPath(ThisMod));
			ThisMod = strdup(g_Mod);

			printf("Trying previous module...\n");

			hMod = sbncLoadModule();

			if (hMod == NULL) {
				free(g_Mod);
				g_Mod = strdup(sbncBuildPath(SBNC_MODULE));

				printf("Trying failsafe module...\n");

				hMod = sbncLoadModule();

				if (hMod == NULL) {
					printf("Giving up...\n");

					ExitCode = EXIT_FAILURE;

					break;
				}
			}
		} else {
			free(ThisMod);
			ThisMod = strdup(g_Mod);
		}
	}

	Socket_Final();

#if !defined(_WIN32) || defined(__MINGW32__)
	lt_dlexit();
#endif

	free(ThisMod);
	free(g_Mod);

	return ExitCode;
}
