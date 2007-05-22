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
#include <string.h>
#include "Box.h"

static box_t g_RootBox;

static box_t Box_alloc(void);
static void Box_free(box_t Box);
static void Box_free_element(element_t *Element);
static int Box_put(box_t Parent, element_t Element);
static element_t *Box_get(box_t Parent, const char *Name);

static box_t Box_alloc(void) {
	box_t Box;

	Box = (box_t)malloc(sizeof(*Box));

	if (Box == NULL) {
		return NULL;
	}

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

	free(Box);
}

static void Box_free_element(element_t *Element) {
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

	NewElement = Box_get(Parent, Element.Name);

	if (NewElement != NULL) {
		Box_free_element(NewElement);
	} else {
		NewElement = (element_t *)malloc(sizeof(element_t));

		if (NewElement == NULL) {
			return -1;
		}
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

	Element.Type = TYPE_STRING;

	strncpy(Element.Name, Name, sizeof(Element.Name));
	Element.Name[sizeof(Element.Name) - 1] = '\0';

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

	Element.Type = TYPE_INTEGER;

	strncpy(Element.Name, Name, sizeof(Element.Name));
	Element.Name[sizeof(Element.Name) - 1] = '\0';

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

	Element.Type = TYPE_STRING;

	strncpy(Element.Name, Name, sizeof(Element.Name));
	Element.Name[sizeof(Element.Name) - 1] = '\0';

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
		return Element.ValueBox;
	}
}

int Box_remove(box_t Parent, const char *Name) {
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
		Element->Previous->Next = NULL;
	} else {
		Parent->First = Element->Next;
	}

	if (Element->Next != NULL) {
		Element->Next->Previous = NULL;
	} else {
		Parent->Last = Element->Previous;
	}

	Box_free_element(Element);

	return 0;
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

	if (Previous == NULL) {
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
