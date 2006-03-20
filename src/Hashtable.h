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

/**
 * hashlist_t
 *
 * A bucket in a hashtable.
 */
template <typename HashListType> struct hashlist_t {
	unsigned int Count;
	char **Keys;
	HashListType *Values;
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

/**
 * CmpString
 *
 * Compares two strings. This function is intended to be used with qsort().
 *
 * @param pA the first string
 * @param pB the second string
 */
inline int CmpString(const void *pA, const void *pB) {
	return strcmp(*(const char **)pA, *(const char **)pB);
}

template<typename Type, bool CaseSensitive, int Size> class CHashtable {
	hashlist_t<Type> m_Items[Size]; /**< used for storing the items of the hashtable */
	void (*m_DestructorFunc)(Type Object); /**< the function which should be used for destroying items */
	unsigned int m_LengthCache; /**< (cached) number of items in the hashtable */
public:
#ifndef SWIG
	/**
	 * CHashtable
	 *
	 * Constructs an empty hashtable.
	 */
	CHashtable(void) {
		memset(m_Items, 0, sizeof(m_Items));

		m_DestructorFunc = NULL;

		m_LengthCache = 0;
	}

	/**
	 * ~CHashtable
	 *
	 * Destructs a hashtable
	 */
	~CHashtable(void) {
		Clear();
	}
#endif

	/**
	 * Clear
	 *
	 * Removes all items from the hashtable.
	 */
	virtual void Clear(void) {
		for (unsigned int i = 0; i < sizeof(m_Items) / sizeof(hashlist_t<Type>); i++) {
			hashlist_t<Type> *List = &m_Items[i];

			for (unsigned int a = 0; a < List->Count; a++) {
				free(List->Keys[a]);

				if (m_DestructorFunc != NULL) {
					m_DestructorFunc(List->Values[a]);
				}
			}

			free(List->Keys);
			free(List->Values);
		}

		memset(m_Items, 0, sizeof(m_Items));
	}

	/**
	 * Hash
	 *
	 * Calculates a hash value for a string.
	 *
	 * @param String the string
	 */
	static hashvalue_t Hash(const char *String) {
		unsigned long HashValue = 5381;
		int c;

		while (c = *String++) {
			HashValue = ((HashValue << 5) + HashValue) + c; /* hash * 33 + c */
		}

		return HashValue;
	}

	/**
	 * Add
	 *
	 * Inserts a new item into a hashtable.
	 *
	 * @param Key the name of the item
	 * @param Value the item
	 */
	virtual RESULT<bool> Add(const char *Key, Type Value) {
		char *dupKey;
		char **newKeys;
		Type *newValues;
		hashlist_t<Type> *List;

		if (Key == NULL) {
			THROW(bool, Generic_InvalidArgument, "Key cannot be NULL.");
		}

		// Remove any existing item which has the same key
		Remove(Key);

		List = &m_Items[Hash(Key) % Size];

		dupKey = strdup(Key);

		if (dupKey == NULL) {
			THROW(bool, Generic_OutOfMemory, "strdup() failed.");
		}

		newKeys = (char **)realloc(List->Keys, (List->Count + 1) * sizeof(char*));

		if (newKeys == NULL) {
			free(dupKey);

			THROW(bool, Generic_OutOfMemory, "realloc() failed.");
		}

		List->Keys = newKeys;

		newValues = (Type *)realloc(List->Values, (List->Count + 1) * sizeof(Type));

		if (newValues == NULL) {
			free(dupKey);

			THROW(bool, Generic_OutOfMemory, "realloc() failed.");
		}

		List->Count++;

		List->Values = newValues;

		List->Keys[List->Count - 1] = dupKey;
		List->Values[List->Count -1] = Value;

		m_LengthCache++;

		RETURN(bool, true);
	}

	/**
	 * Get
	 *
	 * Returns the item which is associated to a key or NULL if
	 * there is no such item.
	 *
	 * @param Key the key
	 */
	virtual Type Get(const char *Key) const {
		const hashlist_t<Type> *List;

		if (Key == NULL) {
			return NULL;
		}

		List = &m_Items[Hash(Key) % Size];

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

	/**
	 * Remove
	 *
	 * Removes an item from the hashlist.
	 *
	 * @param Key the name of the item
	 * @param DontDestroy determines whether the value destructor function
	 *					  is going to be called for the item
	 */
	virtual RESULT<bool> Remove(const char *Key, bool DontDestroy = false) {
		hashlist_t<Type> *List;

		if (Key == NULL) {
			THROW(bool, Generic_InvalidArgument, "Key cannot be NULL.");
		}

		List = &m_Items[Hash(Key) % Size];

		if (List->Count == 0) {
			RETURN(bool, true);
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

			m_LengthCache--;
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

					m_LengthCache--;

					break;
				}
			}
		}

		RETURN(bool, true);
	}

	/**
	 * GetLength
	 *
	 * Returns the number of items in the hashtable.
	 */
	virtual unsigned int GetLength(void) const {
/*		unsigned int Count = 0;

		for (unsigned int i = 0; i < sizeof(m_Items) / sizeof(hashlist_t<Type>); i++) {
			Count += m_Items[i].Count;
		}

		return Count;*/

		return m_LengthCache;
	}

	/**
	 * RegisterValueDestructor
	 *
	 * Registers a value destructor for a hashtable.
	 *
	 * @param Func the value destructor
	 */
	virtual void RegisterValueDestructor(void (*Func)(Type Object)) {
		m_DestructorFunc = Func;
	}

	/**
	 * Iterate
	 *
	 * Returns the Index-th item of the hashtable.
	 *
	 * @param Index the index
	 */
	virtual hash_t<Type> *Iterate(unsigned int Index) const {
		static void *thisPointer = NULL;
		static unsigned int cache_Index = 0, cache_i = 0, cache_a = 0;
		int Skip = 0;
		unsigned int i, a;
		bool first = true;

		if (thisPointer == this && cache_Index == Index - 1) {
			i = cache_i;
			a = cache_a;
			Skip = cache_Index;
		} else {
			i = 0;
			a = 0;
		}

		for (; i < sizeof(m_Items) / sizeof(hashlist_t<Type>); i++) {
			if (!first) {
				a = 0;
			} else {
				first = false;
			}

			for (;a < m_Items[i].Count; a++) {
				if (Skip == Index) {
					static hash_t<Type> Item;

					Item.Name = m_Items[i].Keys[a];
					Item.Value = m_Items[i].Values[a];

					cache_Index = Index;
					cache_i = i;
					cache_a = a;
					thisPointer = (void *)this;

					return &Item;
				}

				Skip++;
			}
		}

		return NULL;
	}

	/**
	 * GetSortedKeys
	 *
	 * Returns a NULL-terminated list of keys for the hashtable. The returned list
	 * will eventually have to be passed to free().
	 */
	virtual char **GetSortedKeys(void) const {
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

#ifdef SBNC
/**
 * CHashCompare
 *
 * Used for comparing strings efficiently.
 */
class CHashCompare {
	const char *m_String; /**< the actual string */
	hashvalue_t m_Hash; /**< the string's hash value */
public:
	/**
	 * CHashCompare
	 *
	 * Constructs a CHashCompare object.
	 */
	CHashCompare(const char *String) {
		m_String = String;

		if (String) {
			m_Hash = CHashtable<void *, false, 2^16>::Hash(String);
		} else {
			m_Hash = 0;
		}
	}

	/**
	 * operator==
	 *
	 * Compares two CHashCompare objects.
	 */
	bool operator==(CHashCompare Other) const {
		if (m_Hash != Other.m_Hash) {
			return false;
		} else {
			return (strcasecmp(m_String, Other.m_String) == 0);
		}
	}
};
#endif
