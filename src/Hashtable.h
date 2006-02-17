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
 * hash_t<Type>
 *
 * Represents a (key, value) pair in a hashtable.
 */
template <typename Type> struct hash_t {
	char *Name;
	Type Value;
};

typedef unsigned long hashvalue_t;

/**
 * DestroyObject<Type>
 *
 * A generic value destructor which can be used in a hashtable.
 */
template<typename Type>
void DestroyObject(Type *Object) {
	delete Object;
}

int CmpString(const void *pA, const void *pB);

template<typename Type, bool CaseSensitive, int Size> class CHashtable {
	typedef void (DestroyValue)(Type Object);

	template <typename HashListType> struct hashlist_t {
		unsigned int Count;
		char **Keys;
		HashListType *Values;
	};

	hashlist_t<Type> m_Items[Size]; /**< used for storing the items of the hashtable */
	DestroyValue *m_DestructorFunc; /**< the function which should be used for destroying items */
public:
	CHashtable(void) {
		memset(m_Items, 0, sizeof(m_Items));

		m_DestructorFunc = NULL;
	}

	~CHashtable(void) {
		Clear();
	}

	void Clear(void) {
		for (unsigned int i = 0; i < sizeof(m_Items) / sizeof(hashlist_t<Type>); i++) {
			hashlist_t<Type> *List = &m_Items[i];

			for (unsigned int a = 0; a < List->Count; a++) {
				free(List->Keys[a]);

				if (m_DestructorFunc) {
					m_DestructorFunc(List->Values[a]);
				}
			}

			free(List->Keys);
			free(List->Values);
		}

		memset(m_Items, 0, sizeof(m_Items));
	}

	static hashvalue_t Hash(const char *String) {
		unsigned long HashValue = 5381;
		int c;

		while (c = *String++) {
			HashValue = ((HashValue << 5) + HashValue) + c; /* hash * 33 + c */
		}

		return HashValue;
	}

	bool Add(const char *Key, Type Value) {
		char *dupKey;
		char **newKeys;
		Type *newValues;
		hashlist_t<Type> *List;

		if (Key == NULL) {
			return false;
		}

		// Remove any existing item which has the same key
		Remove(Key);

		List = &m_Items[Hash(Key) % Size];

		dupKey = strdup(Key);

		if (dupKey == NULL) {
/*			LOGERROR("strdup() failed.");*/

			return false;
		}

		newKeys = (char **)realloc(List->Keys, (List->Count + 1) * sizeof(char*));

		if (newKeys == NULL) {
/*			LOGERROR("realloc() failed.");*/

			free(dupKey);

			return false;
		}

		List->Keys = newKeys;

		newValues = (Type *)realloc(List->Values, (List->Count + 1) * sizeof(Type));

		if (newValues == NULL) {
/*			LOGERROR("realloc() failed.");*/

			free(dupKey);

			return false;
		}

		List->Count++;

		List->Values = newValues;

		List->Keys[List->Count - 1] = dupKey;
		List->Values[List->Count -1] = Value;

		return true;
	}

	Type Get(const char *Key) const {
		if (Key == NULL) {
			return NULL;
		}

		const hashlist_t<Type> *List = &m_Items[Hash(Key) % Size];

		if (List->Count == 0) {
			return NULL;
		} else {
			for (unsigned int i = 0; i < List->Count; i++) {
				if (List->Keys[i] && (CaseSensitive ? strcmp(List->Keys[i], Key) : strcasecmp(List->Keys[i], Key)) == 0) {
					return List->Values[i];
				}
			}

			return NULL;
		}
	}

	unsigned int GetLength(void) const {
		unsigned int Count = 0;

		for (unsigned int i = 0; i < sizeof(m_Items) / sizeof(hashlist_t<Type>); i++) {
			Count += m_Items[i].Count;
		}

		return Count;
	}

	bool Remove(const char *Key, bool DontDestroy = false) {
		hashlist_t<Type> *List;

		if (Key == NULL) {
			return false;
		}

		List = &m_Items[Hash(Key) % Size];

		if (List->Count == 0) {
			return true;
		} else if (List->Count == 1 && (CaseSensitive ? strcmp(List->Keys[0], Key) : strcasecmp(List->Keys[0], Key)) == 0) {
			if (m_DestructorFunc != NULL && DontDestroy == false) {
				m_DestructorFunc(List->Values[0]);
			}

			free(List->Keys[0]);

			free(List->Keys);
			free(List->Values);
			List->Count = 0;
			List->Keys = NULL;
			List->Values = NULL;
		} else {
			for (unsigned int i = 0; i < List->Count; i++) {
				if (List->Keys[i] && (CaseSensitive ? strcmp(List->Keys[i], Key) : strcasecmp(List->Keys[i], Key)) == 0) {
					free(List->Keys[i]);

					List->Keys[i] = List->Keys[List->Count - 1];

					if (m_DestructorFunc != NULL && DontDestroy == false) {
						m_DestructorFunc(List->Values[i]);
					}

					List->Values[i] = List->Values[List->Count - 1];
					List->Count--;

					break;
				}
			}
		}

		return true;
	}

	void RegisterValueDestructor(DestroyValue *Func) {
		m_DestructorFunc = Func;
	}

	hash_t<Type> *Iterate(int Index) const {
		int Skip = 0;

		for (unsigned int i = 0; i < sizeof(m_Items) / sizeof(hashlist_t<Type>); i++) {
			for (unsigned int a = 0; a < m_Items[i].Count; a++) {
				if (Skip == Index) {
					static hash_t<Type> Item;

					Item.Name = m_Items[i].Keys[a];
					Item.Value = m_Items[i].Values[a];

					return &Item;
				}

				Skip++;
			}
		}

		return NULL;
	}

	char **GetSortedKeys(void) const {
		char **Keys = NULL;
		unsigned int Count = 0;

		for (unsigned int i = 0; i < sizeof(m_Items) / sizeof(hashlist_t<Type>); i++) {
			Keys = (char **)realloc(Keys, (Count + m_Items[i].Count) * sizeof(char*));

			for (unsigned int a = 0; a < m_Items[i].Count; a++) {
				Keys[Count + a] = m_Items[i].Keys[a];
			}

			Count += m_Items[i].Count;
		}

		qsort(Keys, Count, sizeof(char *), CmpString);

		Keys = (char **)realloc(Keys, ++Count * sizeof(char*));

		Keys[Count - 1] = NULL;

		return Keys;
	}

};

class CHashCompare {
	const char *m_String;
	hashvalue_t m_Hash;
public:
	CHashCompare(const char *String) {
		m_String = String;

		if (String) {
			m_Hash = CHashtable<void *, false, 2^16>::Hash(String);
		} else {
			m_Hash = 0;
		}
	}

	inline bool operator==(CHashCompare Other) const {
		if (m_Hash != Other.m_Hash) {
			return false;
		} else {
			return (strcasecmp(m_String, Other.m_String) == 0);
		}
	}
};
