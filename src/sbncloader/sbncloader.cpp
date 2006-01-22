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
#include "../sbnc.h"

#ifdef _WIN32
#ifndef _DEBUG
#define SBNC_MODULE "sbnc.dll"
#else
#define SBNC_MODULE "..\\Debug\\sbnc.dll"
#endif
#else
#define SBNC_MODULE "./sbnc.so"
#endif

CAssocArray *g_Box;
char *g_Mod;
bool g_Freeze, g_Signal;

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

void sbncSetModule(const char *Module) {
	free(g_Mod);
	g_Mod = strdup(Module);
}

void sbncSetAutoReload(bool Reload) {
}

void sbncReadIpcFile(void) {
	FILE *ipcFile = fopen("sbnc.ipc", "r");

	if (ipcFile != NULL) {
		char *n;
		char FileBuf[512];

		if ((n = fgets(FileBuf, sizeof(FileBuf) - 1, ipcFile)) != 0) {
			if (FileBuf[strlen(FileBuf) - 1] == '\n')
				FileBuf[strlen(FileBuf) - 1] = '\0';

			if (FileBuf[strlen(FileBuf) - 1] == '\r')
				FileBuf[strlen(FileBuf) - 1] = '\0';

			free(g_Mod);
			g_Mod = strdup(FileBuf);
		}

		fclose(ipcFile);
	}
}

HMODULE sbncLoadModule(void) {
	HMODULE hMod;

	hMod = LoadLibrary(g_Mod);

	printf("Loading shroudBNC from %s\n", g_Mod);

	if (hMod == NULL) {
		printf("Module could not be loaded.");

#ifndef _WIN32
		printf(" %s", dlerror());
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

	printf("shroudBNC loader\n");

	g_Mod = strdup(SBNC_MODULE);
	ThisMod = strdup(g_Mod);

	Socket_Init();

	sbncReadIpcFile();

	hMod = sbncLoadModule();

	if (hMod == NULL) {
		free(g_Mod);
		g_Mod = strdup(SBNC_MODULE);

		printf("Trying failsafe module...\n");

		hMod = sbncLoadModule();

		if (hMod == NULL) {
			free(g_Mod);

			printf("Giving up...\n");

			return 1;
		}
	}

	if (hMod == NULL)
		return 1;

	loaderparams_s Parameters;

	Parameters.Version = 200;
	Parameters.argc = argc;
	Parameters.argv = argv;
	Parameters.GetBox = sbncGetBox;
	Parameters.SigEnable = sbncSigEnable;
	Parameters.SetModule = sbncSetModule;
	Parameters.unused = sbncSetAutoReload;

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
			g_Mod = strdup(ThisMod);

			printf("Trying previous module...\n");

			hMod = sbncLoadModule();

			if (hMod == NULL) {
				free(g_Mod);
				g_Mod = strdup(SBNC_MODULE);

				printf("Trying failsafe module...\n");

				hMod = sbncLoadModule();

				if (hMod == NULL) {
					free(g_Mod);

					printf("Giving up...\n");

					return 1;
				}
			}
		} else {
			free(ThisMod);
			ThisMod = strdup(g_Mod);
		}
	}

	Socket_Final();

	free(g_Mod);

	return 0;
}
