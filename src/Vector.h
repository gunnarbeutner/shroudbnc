template <typename Type>
class CVector {
private:
	bool m_ReadOnly;
	Type* m_List;
	unsigned m_Count;

public:
	CVector(void) {
		m_List = NULL;
		m_Count = 0;
		m_ReadOnly = false;
	}

	CVector(Type *List, int Count, bool ReadOnly = true) {
		SetList(List, Count);
		m_ReadOnly = ReadOnly;
	}

	~CVector(void) {
		free(m_List);
	}

	bool Insert(Type T) {
		Type* NewList;

		if (m_ReadOnly)
			return false;

		NewList = (Type*)realloc(m_List, sizeof(Type) * ++m_Count);

		if (NewList == NULL) {
			m_Count--;

			return false;
		}

		m_List = NewList;
		m_List[m_Count - 1] = T;

		return true;
	}

	bool Remove(int Index) {
		if (m_ReadOnly)
			return false;

		m_List[Index] = m_List[m_Count - 1];

		m_List = (Type *)realloc(m_List, sizeof(Type) * --m_Count);

		return true;
	}

	bool Remove(Type T) {
		bool ReturnValue = false;

		for (int i = m_Count - 1; i >= 0; i--) {
			if (m_List[i] == T) {
				if (Remove(i))
					ReturnValue = true;
			}
		}

		return ReturnValue;
	}

	Type& operator[] (int Index) {
		// check m_Count

		return m_List[Index];
	}

	int Count(void) {
		return m_Count;
	}

	Type *GetList(void) {
		return m_List;
	}

	void SetList(Type *List, int Count) {
		free(m_List);

		m_List = (Type *)malloc(sizeof(Type) * Count);
		memcpy(m_List, List, sizeof(Type) * Count);
		m_Count = Count;
		m_ReadOnly = false;

	}

	Type *GetAddressOf(int Index) {
		return &(m_List[Index]);
	}
};
