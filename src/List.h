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

typedef enum list_error_e {
	List_ReadOnly,
	List_ItemNotFound
} list_error_t;

template <typename Type>
struct link_t {
	Type Value;
	link_t<Type> *Next;
	link_t<Type> *Previous;
};

/**
 * CList
 *
 * A linked list.
 */
template <typename Type>
class CList {
private:
	mutable link_t<Type> *m_Head; /**< first element */
	mutable link_t<Type> *m_Tail; /**< last element */

public:
#ifndef SWIG
	/**
	 * CList
	 *
	 * Constructs an empty list.
	 */
	CList(void) {
		m_Head = NULL;
		m_Tail = NULL;
	}

	/**
	 * ~CList
	 *
	 * Destroys a list.
	 */
	virtual ~CList(void) {
		Clear();
	}
#endif

	/**
	 * Insert
	 *
	 * Inserts a new item into the list.
	 *
	 * @param Item the item which is to be inserted
	 */
	virtual RESULT<link_t<Type> *> Insert(Type Item) {
		link_t<Type> *Element;

		Element = (link_t<Type> *)malloc(sizeof(link_t<Type>));

		mmark(Element);

		if (Element == NULL) {
			THROW(link_t<Type> *, Generic_OutOfMemory, "Out of memory.");
		}

		Element->Next = NULL;

		if (m_Tail != NULL) {
			Element->Previous = m_Tail;
			m_Tail->Next = Element;
			m_Tail = Element;
		} else {
			m_Head = Element;
			m_Tail = Element;
			Element->Previous = NULL;
		}

		Element->Value = Item;

		RETURN(link_t<Type> *, Element);
	}

	/**
	 * Remove
	 *
	 * Removes an item from the list.
	 *
	 * @param Item the item which is to be removed
	 */
	virtual RESULT<bool> Remove(Type Item) {
		bool ReturnValue = false;
		link_t<Type> *Current = m_Head;

		while (Current != NULL) {
			if (memcmp(&(Current->Value), &Item, sizeof(Item))) {
				Remove(Current);

				RETURN(bool, true);
			}
		}

		THROW(bool, Vector_ItemNotFound, "Item could not be found.");
	}

	/**
	 * Remove
	 *
	 * Removes an item from the list
	 *
	 * @param Item the item's link_t which is to be removed
	 */
	virtual void Remove(link_t<Type> *Item) {
		if (Item == NULL) {
			return;
		}

		if (Item->Next != NULL) {
			Item->Next->Previous = Item->Previous;
		}

		if (Item->Previous != NULL) {
			Item->Previous->Next = Item->Next;
		}

		if (Item == m_Head) {
			m_Head = Item->Next;
		}

		if (Item == m_Tail) {
			m_Tail = Item->Previous;
		}

		free(Item);
	}

	/**
	 * GetHead
	 *
	 * Returns the head of the linked list.
	 */
	virtual link_t<Type> *GetHead(void) const {
		return m_Head;
	}

	/**
	 * Clear
	 *
	 * Removes all items from the list.
	 */
	virtual void Clear(void) {
		link_t<Type> *Current = m_Head;
		link_t<Type> *Next = NULL;

		while (Current != NULL) {
			Next = Current->Next;
			free(Current);
			Current = Next;
		}

		m_Head = NULL;
		m_Tail = NULL;
	}
};
