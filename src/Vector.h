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
	unsigned m_Count; /**< the number of items in the list */

public:
	/**
	 * CVector
	 *
	 * Constructs an empty list
	 */
	CVector(void) {
		m_List = NULL;
		m_Count = 0;
		m_ReadOnly = false;
	}

	/**
	 * CVector
	 *
	 * Constructs a list by copying an existing array
	 */
//	CVector(Type *List, int Count, bool ReadOnly = true) {
//		SetList(List, Count);
//		m_ReadOnly = ReadOnly;
//	}

	/**
	 * ~CVector
	 *
	 * Destroys a list
	 */
	virtual ~CVector(void) {
		free(m_List);
	}

	/**
	 * Insert
	 *
	 * Inserts a new item into the list
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
	 * Removes an item from the list
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
	 * Removes an item from the list
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
	 * Returns an item
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
	 * Returns an item
	 *
	 * @param Index the index of the item which is to be returned
	 */
	virtual Type& Get(int Index) {
		return m_List[Index];
	}

	/**
	 * Count
	 *
	 * Returns the number of items
	 */
	virtual int Count(void) {
		return m_Count;
	}

	/**
	 * GetList
	 *
	 * Returns the actual list which is used for storing the items
	 */
	virtual Type *GetList(void) {
		return m_List;
	}

	/**
	 * SetList
	 *
	 * Sets a new internal list by copying the items from another list
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
	 * Returns the address of an item
	 *
	 * @param Index the index of the item
	 */
	virtual Type *GetAddressOf(int Index) {
		return &(m_List[Index]);
	}
};
