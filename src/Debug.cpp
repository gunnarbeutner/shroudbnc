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

/**
 * DebugMalloc
 *
 * A debugging version of malloc. Allocates memory and keeps track
 * of allocated objects (in g_Allocations).
 *
 * @param Size the size of the new object
 * @param File the file where the DebugMalloc() call comes from
 */
void *DebugMalloc(size_t Size, const char *File) {
	allocation_t Allocation;

	for (size_t i = strlen(File) - 1; i >= 0; i--) {
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

/**
 * DebugFree
 *
 * Deallocates memory and removes the reference to the memory block
 * from the g_Allocations vector.
 *
 * @param Pointer pointer to the memory block
 * @param File the file where the call to this function comes from
 */
void DebugFree(void *Pointer, const char *File) {
	for (unsigned int i = 0; i < g_Allocations.GetLength(); i++) {
		if (g_Allocations[i].Pointer == Pointer) {
			g_Allocations.Remove(i);

			break;
		}
	}

	free(Pointer);
}

/**
 * DebugReAlloc
 *
 * Resizes a block of memory while preserving the previous contents and
 * updates the g_Allocations vector.
 *
 * @param Pointer the pointer to the old memory block
 * @param NewSize the desired new size
 * @param File the name of the file where the call to this function comes from
 */
void *DebugReAlloc(void *Pointer, size_t NewSize, const char *File) {
	void *NewPointer;

	for (size_t i = strlen(File) - 1; i >= 0; i--) {
		if (File[i] == '\\' || File[i] == '/') {
			File = &File[i + 1];

			break;
		}
	}

	if (strcasecmp(File, "vector.h") == 0) {
		return realloc(Pointer, NewSize);
	}

	if (Pointer == NULL) {
		return DebugMalloc(NewSize, File);
	} else {
		NewPointer = realloc(Pointer, NewSize);

		for (unsigned int i = 0; i < g_Allocations.GetLength(); i++) {
			if (g_Allocations[i].Pointer == Pointer) {
				if (NewPointer != NULL) {
					g_Allocations[i].Size = NewSize;
					g_Allocations[i].Pointer = NewPointer;
				} else {
					g_Allocations.Remove(i);
				}

				break;
			}
		}

		return NewPointer;
	}
}

/**
 * DebugStrDup
 *
 * A debugging version of strdup(). Uses DebugMalloc
 * and strcpy internally.
 *
 * @param String the string which is to be copied
 * @param File the file where the call to this function comes from
 */
char *DebugStrDup(const char *String, const char *File) {
	size_t Size;
	char *Copy;

	Size = strlen(String) + 1;
	Copy = (char*)DebugMalloc(Size, File);

	if (Copy == NULL) {
		return NULL;
	}

	strmcpy(Copy, String, Size);

	return Copy;
}
#endif
