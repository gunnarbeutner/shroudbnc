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
} g_Allocations[500000];

int g_AllocationCount = 0;
int g_IgnoreCount = 0;

#define ALLOCIGNORE 100000000
#define ALLOCDIV 150

void Debug_Final(void) {
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
	}

	HeapFree(GetProcessHeap(), 0, sym);

	return ptr;
}

void* DebugMalloc(size_t Size) {
	unsigned long programcounter;

	if (++g_IgnoreCount > ALLOCIGNORE && rand() < RAND_MAX / ALLOCDIV)
		return NULL;

	g_Mem += Size;

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

	void * ptr = HeapAlloc(GetProcessHeap(), 0, Size);

	if (g_Debug && Ret) {
		g_AllocationCount++;
		g_Allocations[g_AllocationCount - 1].valid = true;
		g_Allocations[g_AllocationCount - 1].reported = false;
		g_Allocations[g_AllocationCount - 1].ptr = ptr;
		g_Allocations[g_AllocationCount - 1].size = Size;
		g_Allocations[g_AllocationCount - 1].func = strdup(sym->Name);
	}

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

void DebugFree(void* p) {
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

	HeapFree(GetProcessHeap(), 0, p);
}

void* DebugReAlloc(void* p, size_t newsize) {
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

		HeapFree(GetProcessHeap(), 0, sym);

		return ptr;
	} else
		return DebugMalloc(newsize);
}

char* DebugStrDup(const char* p) {
	char* s = (char*)DebugMalloc(strlen(p) + 1);

	if (s == NULL)
		return NULL;

	strcpy(s, p);

	return s;
}

bool ReportMemory(time_t Now, void* Cookie) {
	for (int i = 0; i < g_AllocationCount; i++) {
		if (g_Allocations[i].valid && g_Allocations[i].size > 50 && !g_Allocations[i].reported) {
			if (strcmp(g_Allocations[i].func, "DebugStrDup") == 0)
				printf("%s -> %p (%s)\n", g_Allocations[i].func, g_Allocations[i].ptr, g_Allocations[i].ptr);
			else
				printf("%s -> %p (%d)\n", g_Allocations[i].func, g_Allocations[i].ptr, g_Allocations[i].size);

			g_Allocations[i].reported = true;
		}
	}

	printf("%d bytes\n", g_Mem);

	return true;
}

#undef strcmpi
int profilestrcmpi(const char* a, const char* b) { return strcmpi(a, b); }

#endif
