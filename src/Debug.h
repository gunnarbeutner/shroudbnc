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

/**
 * allocation_t
 *
 * Used for logging allocations.
 */
typedef struct allocation_s {
	const char *File;
	size_t Size;
	void *Pointer;
} allocation_t;

/**
 * file_t
 *
 * Used for summarizing all allocations for a single source file
 */
typedef struct file_s {
	const char *File;
	unsigned int AllocationCount;
	size_t Bytes;
} file_t;

#ifdef SBNC
extern CVector<allocation_t> g_Allocations; /**< a list of all active memory blocks */
#endif
