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

#ifdef _DEBUG
CVector<allocation_t> g_Allocations;

#undef malloc
#undef free
#undef realloc
#undef strdup

void* DebugMalloc(size_t Size, const char *File) {
	allocation_t Allocation;

	for (int i = strlen(File) - 1; i >= 0; i--) {
		if (File[i] == '\\' || File[i] == '/') {
			File = &File[i + 1];

			break;
		}
	}

	if (strcasecmp(File, "vector.h") == 0) {
		return malloc(Size);
	}

	Allocation.File = File;
	Allocation.Size = Size;
	Allocation.Pointer = malloc(Size);

	g_Allocations.Insert(Allocation);

	return Allocation.Pointer;
}

void DebugFree(void *Pointer, const char *File) {
	for (unsigned int i = 0; i < g_Allocations.GetLength(); i++) {
		if (g_Allocations[i].Pointer == Pointer) {
			g_Allocations.Remove(i);

			break;
		}
	}

	free(Pointer);
}

void *DebugReAlloc(void *Pointer, size_t NewSize, const char *File) {
	if (Pointer == NULL) {
		return DebugMalloc(NewSize, File);
	} else {
		return realloc(Pointer, NewSize);
	}
}

char *DebugStrDup(const char *String, const char *File) {
	char *Copy;

	Copy = (char *)DebugMalloc(strlen(String) + 1, File);

	if (Copy == NULL) {
		return NULL;
	}

	strcpy(Copy, String);

	return Copy;
}
#endif
