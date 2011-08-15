/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2011 Gunnar Beutner                                      *
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

#ifndef LIST_H
#define LIST_H

typedef enum list_error_e {
	List_ReadOnly,
	List_ItemNotFound
} list_error_t;

template <typename Type>
struct link_t {
	Type Value;
	bool Valid;
	link_t<Type> *Next;
	link_t<Type> *Previous;
};

template <typename Type>
class CList;

template <typename Type>
class CListCursor;

template <typename Type>
CListCursor<Type> GetCursorHelper(CList<Type> *List);

/**
 * CList
 *
 * A linked list.
 */
template <typename Type>
class CList {
private:
	link_t<Type> *m_Head; /**< first element */
	link_t<Type> *m_Tail; /**< last element */
	mutable unsigned int m_Locks; /**< number of locks held */

public:
	typedef class CListCursor<Type> Cursor;

#ifndef SWIG
	/**
	 * CList
	 *
	 * Constructs an empty list.
	 */
	CList(void) {
		m_Head = NULL;
		m_Tail = NULL;
		m_Locks = 0;
	}

	/**
	 * ~CList
	 *
	 * Destroys a list.
	 */
	~CList(void) {
		Clear();
	}
#endif /* SWIG */

	/**
	 * Insert
	 *
	 * Inserts a new item into the list.
	 *
	 * @param Item the item which is to be inserted
	 */
	RESULT<link_t<Type> *> Insert(Type Item) {
		link_t<Type> *Element;

		Element = (link_t<Type> *)malloc(sizeof(link_t<Type>));

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
		Element->Valid = true;

		RETURN(link_t<Type> *, Element);
	}

	/**
	 * Remove
	 *
	 * Removes an item from the list.
	 *
	 * @param Item the item which is to be removed
	 */
	RESULT<bool> Remove(Type Item) {
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
	void Remove(link_t<Type> *Item) {
		if (Item == NULL) {
			return;
		}

		if (m_Locks > 0) {
			Item->Valid = false;
#ifdef _DEBUG
			memset(&(Item->Value), 0, sizeof(Item->Value));
#endif /* _DEBUG */
		} else {
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
	}

	/**
	 * GetHead
	 *
	 * Returns the head of the linked list.
	 */
	link_t<Type> *GetHead(void) const {
		return m_Head;
	}

	/**
	 * Clear
	 *
	 * Removes all items from the list.
	 */
	void Clear(void) {
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

	/**
	 * Lock
	 *
	 * Locks the list so items are delay-deleted.
	 */
	void Lock(void) {
		m_Locks++;
	}

	/**
	 * Unlock
	 *
	 * Unlocks the list.
	 */
	void Unlock(void) {
		assert(m_Locks > 0);

		m_Locks--;

		if (m_Locks == 0) {
			link_t<Type> *Current, *Next;

			Current = m_Head;

			while (Current != NULL) {
				Next = Current->Next;

				if (!Current->Valid) {
					Remove(Current);
				}

				Current = Next;
			}
		}
	}
};

/**
 * CListCursor
 *
 * Used for safely iterating over CList objects.
 */
template <typename Type>
class CListCursor {
private:
	link_t<Type> *m_Current; /**< the current item */
	CList<Type> *m_List; /**< the list */

public:
	/**
	 * CListCursor
	 *
	 * Initializes a new cursor.
	 *
	 * @param List the list object
	 */
	explicit CListCursor(CList<Type> *List) {
		m_List = List;

		List->Lock();

		m_Current = List->GetHead();

		while (m_Current != NULL && !m_Current->Valid) {
			m_Current = m_Current->Next;
		}
	}

	/**
	 * ~CListCursor
	 *
	 * Destroys a cursor.
	 */
	~CListCursor(void) {
		m_List->Unlock();
	}

	/**
	 * operator *
	 *
	 * Retrieves the current object.
	 */
	Type& operator *(void) {
		return m_Current->Value;
	}

	/**
	 * operator ->
	 *
	 * Retrieves the current object.
	 */
	Type* operator ->(void) {
		return &(m_Current->Value);
	}

	/**
	 * Remove
	 *
	 * Removes the current item.
	 */
	void Remove(void) {
		m_List->Remove(m_Current);
	}

	/**
	 * Proceed
	 *
	 * Proceeds in the linked list.
	 */
	void Proceed(void) {
		if (m_Current == NULL) {
			return;
		}

		m_Current = m_Current->Next;

		while (m_Current != NULL && !m_Current->Valid) {
			m_Current = m_Current->Next;
		}
	}

	/**
	 * IsValid
	 *
	 * Checks whether the end of the list has been reached.
	 */
	bool IsValid(void) {
		return (m_Current != NULL);
	}

	/**
	 * IsRemoved
	 *
	 * Checks whether the current item has been removed.
	 */
	bool IsRemoved(void) {
		if (m_Current == NULL) {
			return true;
		} else {
			return !m_Current->Valid;
		}
	}
};

#endif /* LIST_H */
