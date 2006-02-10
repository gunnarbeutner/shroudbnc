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

#define NOADNSLIB
#include "../StdAfx.h"
#include "../sbnc.h"

#ifdef _MSC_VER
#ifndef _DEBUG
#define SBNC_MODULE "sbnc.dll"
#else
#define SBNC_MODULE "..\\..\\..\\Debug\\sbnc.dll"
#endif
#else
#define SBNC_MODULE "./libsbnc.la"
#endif

CAssocArray *g_Box;
char *g_Mod;
bool g_Freeze, g_Signal;
const char *g_Arg0;

sbncLoad g_LoadFunc;
sbncPrepareFreeze g_PrepareFreezeFunc;

#ifndef _WIN32

void Socket_Init(void) {}
void Socket_Final(void) {}

#define Sleep(x) usleep(x * 1000)

void sig_usr1(int code) {
	signal(SIGUSR1, SIG_IGN);

	if (g_PrepareFreezeFunc())
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
#endif
}

const char *sbncBuildPath(const char *Filename, const char *BasePath = NULL) {
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
	char NewPath[MAX_PATH];

	PathCanonicalize(NewPath, Path);
	
	strcpy(Path, NewPath);
#endif

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
	g_PrepareFreezeFunc = (sbncPrepareFreeze)GetProcAddress(hMod, "sbncPrepareFreeze");

	if (g_LoadFunc == NULL) {
		printf("Function \"sbncLoad\" does not exist in the module.\n");

		FreeLibrary(hMod);

		return NULL;
	}

	if (g_PrepareFreezeFunc == NULL) {
		printf("Function \"sbncPrepareFreeze\" does not exist in the module.\n");

		FreeLibrary(hMod);

		return NULL;
	}
	
	return hMod;
}

int main(int argc, char **argv) {
	HMODULE hMod;
	char *ThisMod;

	g_Arg0 = argv[0];

#ifdef _WIN32
	SetCurrentDirectory(sbncBuildPath("."));
#else
	chdir(sbncBuildPath("."));
#endif

	printf("shroudBNC loader\n");

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
		g_Mod = strdup(sbncBuildPath(SBNC_MODULE));

		printf("Trying failsafe module...\n");

		hMod = sbncLoadModule();

		if (hMod == NULL) {
			free(g_Mod);

			printf("Giving up...\n");

			return 1;
		}
	}

	if (hMod == NULL) {
		return 1;
	}

	loaderparams_s Parameters;

	Parameters.Version = 201;

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

		if (!g_Freeze)
			break;

		printf("Unloading shroudBNC...\n");

		FreeLibrary(hMod);

		if (g_Signal == true)
			sbncReadIpcFile();

		g_Signal = false;

		hMod = sbncLoadModule();

		if (hMod == NULL) {
			free(g_Mod);
			g_Mod = strdup(sbncBuildPath(ThisMod));

			printf("Trying previous module...\n");

			hMod = sbncLoadModule();

			if (hMod == NULL) {
				free(g_Mod);
				g_Mod = strdup(sbncBuildPath(SBNC_MODULE));

				printf("Trying failsafe module...\n");

				hMod = sbncLoadModule();

				if (hMod == NULL) {
					free(g_Mod);

					printf("Giving up...\n");

					Socket_Final();
#if !defined(_WIN32) || defined(__MINGW32__)
					lt_dlexit();
#endif

					return 1;
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

	free(g_Mod);

	return 0;
}
