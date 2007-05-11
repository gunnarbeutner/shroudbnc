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

/**
 * assoctype_t
 *
 * The type of a list item.
 */
typedef enum assoctype_e {
	Assoc_String = 1,
	Assoc_Integer = 2,
	Assoc_Box = 3
} assoctype_t;

class CAssocArray;

/**
 * assoc_t
 *
 * An item in an associative array.
 */
typedef struct assoc_s {
	char *Name; /**< the name of the item */
	assoctype_t Type; /**< the type of the item, see ASSOC_* constants for details */

	union {
		char *ValueString; /**< the value for (type == Assoc_String) */
		int ValueInt; /**< the value for (type == Assoc_Integer) */
		CAssocArray *ValueBox; /**< the value for (type == Assoc_Box) */
	};
} assoc_t;

/**
 * CAssocArray
 *
 * An associative list of items.
 */
class CAssocArray {
	assoc_t *m_Values; /**< the values for this associative array */
	unsigned int m_Count; /**< the count of items */
public:
	CAssocArray(void);
	virtual ~CAssocArray(void);

	virtual void AddString(const char *Name, const char *Value);
	virtual void AddInteger(const char *Name, int Value);
	virtual void AddBox(const char *Name, CAssocArray *Value);

	virtual const char *ReadString(const char *Name);
	virtual int ReadInteger(const char *Name);
	virtual CAssocArray *ReadBox(const char *Name);

	virtual assoc_t *Iterate(unsigned int Index);

	virtual CAssocArray *Create(void);
	virtual void Destroy(void);
};
