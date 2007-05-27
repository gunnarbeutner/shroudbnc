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

#define TYPE_INTEGER 0
#define TYPE_STRING 1
#define TYPE_BOX 2

struct box_s;

typedef struct element_s {
	int Type;
	unsigned int References;

	char *Name;

	union {
		int ValueInteger;
		char *ValueString;
		box_s *ValueBox;
	};

	struct element_s *Previous;
	struct element_s *Next;
} element_t;

typedef struct box_s {
	struct box_s *Parent;
	char *Name;
	bool ReadOnly;
	element_t *First;
	element_t *Last;
} *box_t;

int Box_put_string(box_t Parent, const char *Name, const char *Value);
int Box_put_integer(box_t Parent, const char *Name, int Value);
box_t Box_put_box(box_t Parent, const char *Name);
int Box_remove(box_t Parent, const char *Name);

const char *Box_get_string(box_t Parent, const char *Name);
int Box_get_integer(box_t Parent, const char *Name);
box_t Box_get_box(box_t Parent, const char *Name);
int Box_enumerate(box_t Parent, element_t **Previous, char *Name, int Len);
int Box_rename(box_t Parent, const char *OldName, const char *NewName);
box_t Box_get_parent(box_t Box);
const char *Box_get_name(box_t Box);
int Box_move(box_t NewParent, box_t Box, const char *NewName);
int Box_set_ro(box_t Box, int ReadOnly);
int Box_reinit(void);
