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
 * CVector
 *
 * A generic list.
 */
template <typename Type>
class CVector {
private:
	bool m_ReadOnly; /**< indicates whether the list is read-only */
	Type *m_List; /**< the actual list */
	unsigned int m_Count; /**< the number of items in the list */

public:
	/**
	 * CVector
	 *
	 * Constructs an empty list.
	 */
	CVector(void) {
		m_List = NULL;
		m_Count = 0;
		m_ReadOnly = false;
	}

	/**
	 * ~CVector
	 *
	 * Destroys a list
	 */
	virtual ~CVector(void) {
		Clear();
	}

	/**
	 * Insert
	 *
	 * Inserts a new item into the list.
	 *
	 * @param Item the item which is to be inserted
	 */
	virtual bool Insert(Type Item) {
		Type *NewList;

		if (m_ReadOnly)
			return false;

		NewList = (Type *)realloc(m_List, sizeof(Type) * ++m_Count);

		if (NewList == NULL) {
			m_Count--;

			return false;
		}

		m_List = NewList;
		m_List[m_Count - 1] = Item;

		return true;
	}

	/**
	 * Remove
	 *
	 * Removes an item from the list.
	 *
	 * @param Index the index of the item which is to be removed
	 */
	virtual bool Remove(int Index) {
		if (m_ReadOnly)
			return false;

		m_List[Index] = m_List[m_Count - 1];

		m_List = (Type *)realloc(m_List, sizeof(Type) * --m_Count);

		return true;
	}

	/**
	 * Remove
	 *
	 * Removes an item from the list.
	 *
	 * @param Item the item which is to be removed
	 */
	virtual bool Remove(Type Item) {
		bool ReturnValue = false;

		for (int i = m_Count - 1; i >= 0; i--) {
			if (memcmp(&m_List[i], &Item, sizeof(Item)) == 0) {
				if (Remove(i))
					ReturnValue = true;
			}
		}

		return ReturnValue;
	}

	/**
	 * operator []
	 *
	 * Returns an item.
	 *
	 * @param Index the index of the item which is to be returned
	 */
	virtual Type& operator[] (int Index) {
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
	virtual Type& Get(int Index) {
		return m_List[Index];
	}

	/**
	 * GetLength
	 *
	 * Returns the number of items.
	 */
	virtual unsigned int GetLength(void) {
		return m_Count;
	}

	/**
	 * GetList
	 *
	 * Returns the actual list which is used for storing the items.
	 */
	virtual Type *GetList(void) {
		return m_List;
	}

	/**
	 * SetList
	 *
	 * Sets a new internal list by copying the items from another list.
	 */
	virtual void SetList(Type *List, int Count) {
		free(m_List);

		m_List = (Type *)malloc(sizeof(Type) * Count);
		memcpy(m_List, List, sizeof(Type) * Count);
		m_Count = Count;
		m_ReadOnly = false;
	}

	/**
	 * GetAddressOf
	 *
	 * Returns the address of an item.
	 *
	 * @param Index the index of the item
	 */
	virtual Type *GetAddressOf(int Index) {
		return &(m_List[Index]);
	}

	/**
	 * Clear
	 *
	 * Removes all items from the list.
	 */
	virtual void Clear(void) {
		free(m_List);
		m_List = NULL;
		m_Count = 0;
	}
};
