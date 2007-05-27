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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../snprintf.h"
#include "Box.h"

static box_t g_RootBox;

static box_t Box_alloc(void);
static void Box_free(box_t Box);
static void Box_free_element(element_t *Element, int OnlyName = 0);
static int Box_put(box_t Parent, element_t Element);
static int Box_remove_internal(box_t Parent, const char *Name, int Free = 1);
static element_t *Box_get(box_t Parent, const char *Name);
static const char *Box_unique_name(void);
static int Box_reinit_internal(box_t Box);

static box_t Box_alloc(void) {
	box_t Box;

	Box = (box_t)malloc(sizeof(*Box));

	if (Box == NULL) {
		return NULL;
	}

	Box->Name = NULL;
	Box->Parent = NULL;
	Box->First = NULL;
	Box->Last = NULL;

	return Box;
}

static void Box_free(box_t Box) {
	element_t *Element = Box->First;

	while (Element != NULL) {
		switch (Element->Type) {
			case TYPE_STRING:
				free(Element->ValueString);

				break;
			case TYPE_BOX:
				Box_free(Element->ValueBox);

				break;
			default:
				break;
		}

		Element = Element->Next;
	}

	free(Box->Name);
	free(Box);
}

static void Box_free_element(element_t *Element, int OnlyName) {
	free(Element->Name);

	if (OnlyName) {
		return;
	}

	if (Element->Type == TYPE_STRING) {
		free(Element->ValueString);
	} else if (Element->Type == TYPE_BOX) {
		Box_free(Element->ValueBox);
	}
}

static int Box_put(box_t Parent, element_t Element) {
	element_t *NewElement;

	if (Parent == NULL) {
		if (g_RootBox == NULL) {
			g_RootBox = Box_alloc();
		}

		Parent = g_RootBox;
	}

	if (Parent->ReadOnly) {
		return -1;
	}

	NewElement = Box_get(Parent, Element.Name);

	if (NewElement != NULL) {
		Box_remove(Parent, Element.Name);
	}

	NewElement = (element_t *)malloc(sizeof(element_t));

	if (NewElement == NULL) {
		return -1;
	}

	*NewElement = Element;

	if (Parent->Last != NULL) {
		Parent->Last->Next = NewElement;
	} else {
		Parent->First = NewElement;
	}

	NewElement->Previous = Parent->Last;
	NewElement->Next = NULL;

	Parent->Last = NewElement;

	if (Element.Type == TYPE_BOX) {
		Element.ValueBox->Parent = Parent;
	}

	return 0;
}

static int Box_remove_internal(box_t Parent, const char *Name, int Free) {
	element_t *Element;

	if (Parent == NULL) {
		if (g_RootBox == NULL) {
			return 0;
		}

		Parent = g_RootBox;
	}

	Element = Box_get(Parent, Name);

	if (Element == NULL) {
		return 0;
	}

	if (Element->Previous != NULL) {
		Element->Previous->Next = Element->Next;
	} else {
		Parent->First = Element->Next;
	}

	if (Element->Next != NULL) {
		Element->Next->Previous = Element->Previous;
	} else {
		Parent->Last = Element->Previous;
	}

	Box_free_element(Element, !Free);

	return 0;
}

static const char *Box_unique_name(void) {
	static unsigned int UniqueId = 0;
	static char UniqueIdStr[64];

	snprintf(UniqueIdStr, sizeof(UniqueIdStr), "%d", UniqueId);

	UniqueId++;

	return UniqueIdStr;
}

static int Box_reinit_internal(box_t Box) {
	element_t *Current;

	if (Box == NULL) {
		Box = g_RootBox;

		if (Box == NULL) {
			return 0;
		}
	}

	Current = Box->First;

	while (Current != NULL) {
		if (Current->Type == TYPE_BOX) {
			Current->ValueBox->ReadOnly = false;

			Box_reinit_internal(Current->ValueBox);
		}

		Current = Current->Next;
	}

	return 0;
}

element_t *Box_get(box_t Parent, const char *Name) {
	element_t *Element;

	if (Parent == NULL) {
		if (g_RootBox == NULL) {
			return NULL;
		}

		Parent = g_RootBox;
	}

	Element = Parent->First;

	while (Element != NULL) {
		if (strcmp(Element->Name, Name) == 0) {
			return Element;
		}

		Element = Element->Next;
	}

	return NULL;
}

int Box_put_string(box_t Parent, const char *Name, const char *Value) {
	element_t Element;

	if (Name == NULL) {
		Name = Box_unique_name();
	}

	Element.Type = TYPE_STRING;

	Element.Name = strdup(Name);

	if (Element.Name == NULL) {
		return -1;
	}

	Element.ValueString = strdup(Value);

	if (Element.ValueString == NULL) {
		Box_free_element(&Element);

		return -1;
	}

	if (Box_put(Parent, Element) == -1) {
		Box_free_element(&Element);

		return -1;
	} else {
		return 0;
	}
}

int Box_put_integer(box_t Parent, const char *Name, int Value){
	element_t Element;

	if (Name == NULL) {
		Name = Box_unique_name();
	}

	Element.Type = TYPE_INTEGER;

	Element.Name = strdup(Name);

	if (Element.Name == NULL) {
		return -1;
	}

	Element.ValueInteger = Value;

	if (Box_put(Parent, Element) == -1) {
		Box_free_element(&Element);

		return -1;
	} else {
		return 0;
	}
}

box_t Box_put_box(box_t Parent, const char *Name){
	element_t Element;
	box_t OldBox;

	if (Name == NULL) {
		Name = Box_unique_name();
	} else {
		OldBox = Box_get_box(Parent, Name);

		if (OldBox != NULL) {
			return OldBox;
		}
	}

	Element.Type = TYPE_BOX;

	Element.Name = strdup(Name);

	if (Element.Name == NULL) {
		return NULL;
	}

	Element.ValueBox = Box_alloc();

	if (Element.ValueBox == NULL) {
		free(Element.Name);

		return NULL;
	}

	if (Box_put(Parent, Element) == -1) {
		Box_free_element(&Element);

		return NULL;
	} else {
		Element.ValueBox->Name = strdup(Name);
		return Element.ValueBox;
	}
}

int Box_remove(box_t Parent, const char *Name) {
	return Box_remove_internal(Parent, Name, 1);
}

const char *Box_get_string(box_t Parent, const char *Name){
	element_t *Element;

	Element = Box_get(Parent, Name);

	if (Element == NULL || Element->Type != TYPE_STRING) {
		return NULL;
	} else {
		return Element->ValueString;
	}
}

int Box_get_integer(box_t Parent, const char *Name){
	element_t *Element;

	Element = Box_get(Parent, Name);

	if (Element == NULL || Element->Type != TYPE_INTEGER) {
		return 0;
	} else {
		return Element->ValueInteger;
	}
}

box_t Box_get_box(box_t Parent, const char *Name){
	element_t *Element;

	Element = Box_get(Parent, Name);

	if (Element == NULL || Element->Type != TYPE_BOX) {
		return NULL;
	} else {
		return Element->ValueBox;
	}
}

int Box_enumerate(box_t Parent, element_t **Previous, char *Name, int Len) {
	element_t *Element;

	if (*Previous == NULL) {
		if (Parent == NULL) {
			if (g_RootBox == NULL) {
				return -1;
			}

			Parent = g_RootBox;
		}

		Element = Parent->First;
	} else {
		Element = (*Previous)->Next;
	}

	if (Element == NULL) {
		return -1;
	}

	*Previous = Element;

	strncpy(Name, Element->Name, Len);
	Name[Len - 1] = '\0';


	return 0;
}

int Box_rename(box_t Parent, const char *OldName, const char *NewName) {
	element_t *Element;

	Box_remove(Parent, NewName);
	Element = Box_get(Parent, OldName);

	if (Element != NULL) {
		char *Temp = Element->Name;
		Element->Name = strdup(NewName);

		if (Element->Name == NULL) {
			return -1;
		}

		free(Temp);
	}

	return 0;
}

box_t Box_get_parent(box_t Box) {
	if (Box == NULL) {
		return NULL;
	} else {
		return Box->Parent;
	}
}

const char *Box_get_name(box_t Box) {
	if (Box == NULL) {
		return NULL;
	} else {
		return Box->Name;
	}
}

int Box_move(box_t NewParent, box_t Box, const char *NewName) {
	element_t Element;
	box_t Parent;
	const char *OldName;

	if (Box == NULL) {
		return -1;
	}

	if (NewName != NULL) {
		Box_remove(NewParent, NewName);
	}

	Parent = Box->Parent;

	if (Parent == NULL) {
		return -1;
	}

	OldName = Box->Name;

	if (OldName == NULL) {
		return -1;
	}

	Box_remove_internal(Parent, OldName, 0);

	if (NewName == NULL) {
		NewName = Box_unique_name();
	}

	char *Temp = Box->Name;
	Box->Name = strdup(NewName);

	if (Box->Name == NULL) {
		return -1;
	}

	free(Temp);

	Element.Name = strdup(NewName);

	if (Element.Name == NULL) {
		return -1;
	}

	Element.Type = TYPE_BOX;
	Element.Next = NULL;
	Element.Previous = NULL;
	Element.ValueBox = Box;

	return Box_put(NewParent, Element);
}

int Box_set_ro(box_t Box, int ReadOnly) {
	if (Box == NULL) {
		Box = g_RootBox;

		if (Box == NULL) {
			return -1;
		}
	}

	Box->ReadOnly = (ReadOnly != 0);

	return 0;
}

int Box_reinit(void) {
	return Box_reinit_internal(NULL);
}
