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

#ifndef VECTOR_H
#define VECTOR_H

typedef enum vector_error_e {
	Vector_PreAllocated,
	Vector_ItemNotFound
} vector_error_t;

/**
 * CVector
 *
 * A generic list.
 */
template <typename Type>
class CVector {
private:
	mutable Type *m_List; /**< the actual list */
	int m_Count; /**< the number of items in the list */
	int m_AllocCount; /**< the number of allocated items */

public:
#ifndef SWIG
	/**
	 * CVector
	 *
	 * Constructs an empty list.
	 */
	CVector(void) {
		m_List = NULL;
		m_Count = 0;
		m_AllocCount = 0;
	}

	/**
	 * CVector
	 *
	 * Constructs an empty pre-allocated list.
	 */
	explicit CVector(int AllocCount) {
		m_List = NULL;
		m_Count = 0;
		m_AllocCount = 0;

		Preallocate(AllocCount);
	}

	/**
	 * ~CVector
	 *
	 * Destroys a list.
	 */
	~CVector(void) {
		Clear();
	}
#endif /* SWIG */

	/**
	 * Preallocate
	 *
	 * Preallocates memory for the list.
	 *
	 * @param AllocCount number of items
	 */
	void Preallocate(int AllocCount) {
		Clear();

		m_AllocCount = AllocCount;
		m_List = (Type *)malloc(sizeof(Type) * AllocCount);
	}

	/**
	 * Insert
	 *
	 * Inserts a new item into the list.
	 *
	 * @param Item the item which is to be inserted
	 */
	RESULT<bool> Insert(Type Item) {
		Type *NewList;

		if (m_AllocCount == 0) {
			NewList = (Type *)realloc(m_List, sizeof(Type) * ++m_Count);

			if (NewList == NULL) {
				m_Count--;

				THROW(bool, Generic_OutOfMemory, "Out of memory.");
			}

			m_List = NewList;
		} else {
			if (m_AllocCount > m_Count) {
				m_Count++;
			} else {
				THROW(bool, Generic_OutOfMemory, "Out of memory.");
			}
		}

		m_List[m_Count - 1] = Item;

		RETURN(bool, true);
	}

	/**
	 * Remove
	 *
	 * Removes an item from the list.
	 *
	 * @param Index the index of the item which is to be removed
	 */
	RESULT<bool> Remove(int Index) {
		Type *NewList;

		if (m_AllocCount != 0) {
			THROW(bool, Vector_PreAllocated, "Vector is pre-allocated.");
		}

		m_List[Index] = m_List[m_Count - 1];

		NewList = (Type *)realloc(m_List, sizeof(Type) * --m_Count);

		if (NewList != NULL || m_Count == 0) {
			m_List = NewList;
		}

		RETURN(bool, true);
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

		for (int i = m_Count - 1; i >= 0; i--) {
			if (memcmp(&m_List[i], &Item, sizeof(Item)) == 0) {
				if (Remove(i)) {
					ReturnValue = true;
				}
			}
		}

		if (ReturnValue) {
			RETURN(bool, true);
		} else {
			THROW(bool, Vector_ItemNotFound, "Item could not be found.");
		}
	}

	/**
	 * operator []
	 *
	 * Returns an item.
	 *
	 * @param Index the index of the item which is to be returned
	 */
	Type& operator[] (int Index) const {
		// check m_Count

		return m_List[Index];
	}

	/**
	 * Get
	 *
	 * Returns an item.
	 *
	 * @param Index the index of the item which is to be returned
	 */
	Type& Get(int Index) const {
		return m_List[Index];
	}

	/**
	 * GetLength
	 *
	 * Returns the number of items.
	 */
	int GetLength(void) const {
		return m_Count;
	}

	/**
	 * GetList
	 *
	 * Returns the actual list which is used for storing the items.
	 */
	Type *GetList(void) const {
		return m_List;
	}

	/**
	 * SetList
	 *
	 * Sets a new internal list by copying the items from another list.
	 */
	RESULT<bool> SetList(Type *List, int Count) {
		Clear();

		m_List = (Type *)malloc(sizeof(Type) * Count);

		if (m_List == NULL) {
			THROW(bool, Generic_OutOfMemory, "malloc() failed.");
		}

		memcpy(m_List, List, sizeof(Type) * Count);
		m_Count = Count;

		RETURN(bool, true);
	}

	/**
	 * GetAddressOf
	 *
	 * Returns the address of an item.
	 *
	 * @param Index the index of the item
	 */
	Type *GetAddressOf(int Index) const {
		return &(m_List[Index]);
	}

	/**
	 * Clear
	 *
	 * Removes all items from the list.
	 */
	void Clear(void) {
		free(m_List);
		m_List = NULL;
		m_Count = 0;
		m_AllocCount = 0;
	}

	/**
	 * GetNew
	 *
	 * Inserts a new item into the list (memory is set to NULs).
	 */
	RESULT<Type *> GetNew(void) {
		Type Item;

		memset(&Item, 0, sizeof(Item));

		RESULT<bool> Result = Insert(Item);

		THROWIFERROR(Type *, Result);
		RETURN(Type *, GetAddressOf(GetLength() - 1));
	}
};

#endif /* VECTOR_H */
