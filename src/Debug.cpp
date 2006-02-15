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

#if defined(_WIN32) && defined(_DEBUG)
#include "StdAfx.h"
#include <imagehlp.h>

#pragma comment(lib, "imagehlp.lib")

bool g_DebugInit = false;
unsigned long g_Mem = 0;
bool g_Debug = false;

struct alloc_t {
	bool valid;
	bool reported;
	size_t size;
	void* ptr;
	char* func;
	char *file;
	int line;
} g_Allocations[500000];

int g_AllocationCount = 0;
int g_IgnoreCount = 0;
FILE *g_DebugLog = NULL;

#define ALLOCIGNORE 100000000
#define ALLOCDIV 150

#undef ALLOC_DEBUG

void Debug_Final(void) {
	fclose(g_DebugLog);
	SymCleanup(GetCurrentProcess());
}

#undef free
#undef strdup

void* operator new(size_t Size) {
	unsigned long programcounter;

	if (++g_IgnoreCount > ALLOCIGNORE && rand() < RAND_MAX / ALLOCDIV)
		return NULL;

	g_Mem += Size;

	__asm mov eax, [ebp + 4]
	__asm mov [programcounter], eax

	if (!g_DebugInit) {
		SymInitialize(GetCurrentProcess(), ".;.\\Debug", TRUE);
		g_DebugInit = true;
		g_DebugLog = fopen("sbnc-debug.log", "w");
	}

	IMAGEHLP_SYMBOL* sym;

	sym = (IMAGEHLP_SYMBOL*)HeapAlloc(GetProcessHeap(), 0, sizeof(IMAGEHLP_SYMBOL) + 500);
	sym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
	sym->MaxNameLength = 500;

	BOOL Ret = SymGetSymFromAddr(GetCurrentProcess(), programcounter, NULL, sym);

	void * ptr = HeapAlloc(GetProcessHeap(), 0, Size);

	if (g_Debug && Ret) {
		g_AllocationCount++;
		g_Allocations[g_AllocationCount - 1].valid = true;
		g_Allocations[g_AllocationCount - 1].reported = false;
		g_Allocations[g_AllocationCount - 1].ptr = ptr;
		g_Allocations[g_AllocationCount - 1].size = Size;
		g_Allocations[g_AllocationCount - 1].func = strdup(sym->Name);
		g_Allocations[g_AllocationCount - 1].file = strdup("unknown");
		g_Allocations[g_AllocationCount - 1].line = 0;
	}

	if (g_Bouncer != NULL) {
		g_Bouncer->Log("new: %s -> %p", sym->Name, ptr);
	} else {
		fprintf(g_DebugLog, "new: %s -> %p\n", sym->Name, ptr);
		fflush(g_DebugLog);
	}

#ifdef ALLOC_DEBUG
	printf("operator new(%d) from %s\n", Size, sym->Name);
#endif

	HeapFree(GetProcessHeap(), 0, sym);

	return ptr;
}

void* DebugMalloc(size_t Size, const char *file, int line) {
	unsigned long programcounter;

	if (++g_IgnoreCount > ALLOCIGNORE && rand() < RAND_MAX / ALLOCDIV)
		return NULL;

	g_Mem += Size;

	__asm mov eax, [ebp + 4]
	__asm mov [programcounter], eax

	if (!g_DebugInit) {
		SymInitialize(GetCurrentProcess(), ".;.\\Debug", TRUE);
		g_DebugInit = true;
		g_DebugLog = fopen("sbnc-debug.log", "w");
	}

	for (int i = strlen(file) - 1; i >= 0; i--) {
		if (file[i] == '\\') {
			file = &file[i + 1];
			break;
		}
	}

	IMAGEHLP_SYMBOL* sym;

	sym = (IMAGEHLP_SYMBOL*)HeapAlloc(GetProcessHeap(), 0, sizeof(IMAGEHLP_SYMBOL) + 500);
	sym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
	sym->MaxNameLength = 500;

	BOOL Ret = SymGetSymFromAddr(GetCurrentProcess(), programcounter, NULL, sym);

	void * ptr = HeapAlloc(GetProcessHeap(), 0, Size);

	if (g_Debug && Ret) {
		g_AllocationCount++;
		g_Allocations[g_AllocationCount - 1].valid = true;
		g_Allocations[g_AllocationCount - 1].reported = false;
		g_Allocations[g_AllocationCount - 1].ptr = ptr;
		g_Allocations[g_AllocationCount - 1].size = Size;
		g_Allocations[g_AllocationCount - 1].func = strdup(sym->Name);
		g_Allocations[g_AllocationCount - 1].file = strdup(file);
		g_Allocations[g_AllocationCount - 1].line = line;
	}

	if (g_Bouncer != NULL) {
		g_Bouncer->Log("malloc: %s (%s:%d) -> %p", sym->Name, file, line, ptr);
	} else {
		fprintf(g_DebugLog, "malloc: %s (%s:%d) -> %p\n", sym->Name, file, line, ptr);
		fflush(g_DebugLog);
	}

#ifdef ALLOC_DEBUG
	printf("malloc(%d) from %s\n", Size, sym->Name);
#endif

	HeapFree(GetProcessHeap(), 0, sym);

	return ptr;
}

void operator delete(void* p) {
	g_Mem -= HeapSize(GetProcessHeap(), 0, p);

	for (int i = 0; i < g_AllocationCount; i++) {
		if (g_Allocations[i].ptr == p && g_Allocations[i].valid) {
			g_Allocations[i].valid = false;
			break;
		}
	}

	HeapFree(GetProcessHeap(), 0, p);
}

void DebugFree(void* p, const char *file, int line) {
	PROCESS_HEAP_ENTRY he;
	bool Found = false;

	if (!p)
		return;

	he.lpData = NULL;

	while (HeapWalk(GetProcessHeap(), &he)) {
		if (he.lpData == p) {
			Found = true;

			break;
		}
	}

	if (Found == false) {
		free(p);

		return;
	}

	g_Mem -= HeapSize(GetProcessHeap(), 0, p);

	for (int i = 0; i < g_AllocationCount; i++) {
		if (g_Allocations[i].ptr == p && g_Allocations[i].valid) {
			g_Allocations[i].valid = false;
			free(g_Allocations[i].func);
			break;
		}
	}

	for (int i = strlen(file) - 1; i >= 0; i--) {
		if (file[i] == '\\') {
			file = &file[i + 1];
			break;
		}
	}

	if (g_Bouncer != NULL) {
		g_Bouncer->Log("free: %p (%s:%d)", p, file, line);
	} else {
		fprintf(g_DebugLog, "free: %p (%s:%d)\n", p, file, line);
		fflush(g_DebugLog);
	}

	HeapFree(GetProcessHeap(), 0, p);
}

void* DebugReAlloc(void* p, size_t newsize, const char *file, int line) {
	if (p) {
		if (++g_IgnoreCount > ALLOCIGNORE && rand() < RAND_MAX / ALLOCDIV)
			return NULL;

		unsigned long programcounter;
		g_Mem -= HeapSize(GetProcessHeap(), 0, p);
		g_Mem += newsize;

		__asm mov eax, [ebp + 4]
		__asm mov [programcounter], eax

		if (!g_DebugInit) {
			SymInitialize(GetCurrentProcess(), ".;.\\Debug", TRUE);
			g_DebugInit = true;
		}

		IMAGEHLP_SYMBOL* sym;

		sym = (IMAGEHLP_SYMBOL*)HeapAlloc(GetProcessHeap(), 0, sizeof(IMAGEHLP_SYMBOL) + 500);
		sym->SizeOfStruct = sizeof(IMAGEHLP_SYMBOL);
		sym->MaxNameLength = 500;

		BOOL Ret = SymGetSymFromAddr(GetCurrentProcess(), programcounter, NULL, sym);

		void* ptr = HeapReAlloc(GetProcessHeap(), 0, p, newsize);

		if (Ret && g_Debug) {
			for (int i = 0; i < g_AllocationCount; i++) {
				if (g_Allocations[i].ptr == p) {
					g_Allocations[i].ptr = ptr;
					g_Allocations[i].reported = false;
					g_Allocations[i].size = newsize;

					break;
				}
			}
		}

		for (int i = strlen(file) - 1; i >= 0; i--) {
			if (file[i] == '\\') {
				file = &file[i + 1];
				break;
			}
		}
		if (g_Bouncer != NULL) {
			g_Bouncer->Log("realloc: %s (%s:%d) -> %p", sym->Name, file, line, ptr);
		} else {
			fprintf(g_DebugLog, "realloc: %s (%s:%d) -> %p\n", sym->Name, file, line, ptr);
			fflush(g_DebugLog);
		}

		HeapFree(GetProcessHeap(), 0, sym);

		return ptr;
	} else
		return DebugMalloc(newsize, file, line);
}

char* DebugStrDup(const char* p, const char *file, int line) {
	char* s = (char*)DebugMalloc(strlen(p) + 1, file, line);

	if (s == NULL)
		return NULL;

	strcpy(s, p);

	return s;
}

#endif
