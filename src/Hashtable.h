/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007,2010 Gunnar Beutner                                 *
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

#ifndef HASHTABLE_H
#define HASHTABLE_H

/**
 * hash_t<Type>
 *
 * Represents a (key, value) pair in a hashtable.
 */
template <typename Type>
struct hash_t {
	char *Name; /**< the name of the item */
	Type Value; /**< the item in the hashtable */
};

/**
 * hashlist_t
 *
 * A bucket in a hashtable.
 */
template <typename HashListType>
struct SBNCAPI hashlist_t {
	int Count;
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
 * CmpStringCase
 *
 * Compares two strings. This function is intended to be used with qsort().
 *
 * @param pA the first string
 * @param pB the second string
 */
inline int CmpStringCase(const void *pA, const void *pB) {
	return strcasecmp(*(const char **)pA, *(const char **)pB);
}

/**
 * Hash
 *
 * Calculates a hash value for a string (using the djb2 algorithm).
 *
 * @param String the string
 */
inline unsigned long Hash(const char *String, bool CaseSensitive) {
	unsigned long HashValue = 5381;
	int Character;

	while ((Character = *(String++)) != '\0') {
		if (!CaseSensitive) {
			Character = tolower(Character);
		}

		HashValue = ((HashValue << 5) + HashValue) + Character; /* HashValue * 33 + Character */
	}

	return HashValue;
}

template<typename Type, bool CaseSensitive>
class CHashtable {
private:
	hashlist_t<Type> *m_Buckets; /**< used for storing the items of the hashtable */
	int m_BucketCount; /** bucket count */
	void (*m_DestructorFunc)(Type Object); /**< the function which should be used for destroying items */
	int m_LengthCache; /**< (cached) number of items in the hashtable */

	/**
	 * Rehash
	 *
	 * Increases the bucket count and rehashes the hashtable.
	 */
	void Rehash(void) {
		hashlist_t<Type> *OldBuckets;
		int OldBucketCount;

		OldBuckets = m_Buckets;
		OldBucketCount = m_BucketCount;

		m_BucketCount *= 2;
		m_Buckets = (hashlist_t<Type> *)malloc(sizeof(hashlist_t<Type>) * m_BucketCount);

		if (m_Buckets == NULL) {
			m_Buckets = OldBuckets;
			m_BucketCount /= 2;

			return;
		}

		m_LengthCache = 0;

		memset(m_Buckets, 0, sizeof(hashlist_t<Type>) * m_BucketCount);

		for (int i = 0; i < OldBucketCount; i++) {
			hashlist_t<Type> *List = &OldBuckets[i];

			for (int a = 0; a < List->Count; a++) {
				if (!Add(List->Keys[a], List->Values[a])) {
					/* TODO: this would be a fatal error, add logging */
					exit(EXIT_FAILURE);
				}

				free(List->Keys[a]);
			}

			free(List->Keys);
			free(List->Values);
		}

		free(OldBuckets);
	}

public:
#ifndef SWIG
	/**
	 * CHashtable
	 *
	 * Constructs an empty hashtable.
	 */
	CHashtable(void) {
		m_BucketCount = 32;
		m_Buckets = (hashlist_t<Type> *)malloc(sizeof(hashlist_t<Type>) * m_BucketCount);

		/* TODO: safely check alloc result */

		memset(m_Buckets, 0, sizeof(hashlist_t<Type>) * m_BucketCount);

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


		free(m_Buckets);
	}
#endif /*SWIG */
	/**
	 * Clear
	 *
	 * Removes all items from the hashtable.
	 */
	void Clear(void) {
		for (int i = 0; i < m_BucketCount; i++) {
			hashlist_t<Type> *List = &m_Buckets[i];

			for (int a = 0; a < List->Count; a++) {
				free(List->Keys[a]);

				if (m_DestructorFunc != NULL) {
					m_DestructorFunc(List->Values[a]);
				}
			}

			free(List->Keys);
			free(List->Values);
		}

		memset(m_Buckets, 0, sizeof(m_Buckets));

		m_LengthCache = 0;
	}

	/**
	 * Add
	 *
	 * Inserts a new item into a hashtable.
	 *
	 * @param Key the name of the item
	 * @param Value the item
	 */
	RESULT<bool> Add(const char *Key, Type Value) {
		char *dupKey;
		char **newKeys;
		Type *newValues;
		hashlist_t<Type> *List;

		if (Key == NULL) {
			THROW(bool, Generic_InvalidArgument, "Key cannot be NULL.");
		}

		// Remove any existing item which has the same key
		Remove(Key);

		List = &m_Buckets[Hash(Key, CaseSensitive) % m_BucketCount];

		dupKey = strdup(Key);

		if (dupKey == NULL) {
			THROW(bool, Generic_OutOfMemory, "strdup() failed.");
		}

		newKeys = (char **)realloc(List->Keys, (List->Count + 1) * sizeof(char *));

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

		if (List->Count > 3) {
			Rehash();
		}

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
	Type Get(const char *Key) const {
		const hashlist_t<Type> *List;

		if (Key == NULL) {
			return NULL;
		}

		List = &m_Buckets[Hash(Key, CaseSensitive) % m_BucketCount];

		if (List->Count == 0) {
			return NULL;
		} else {
			for (int i = 0; i < List->Count; i++) {
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
	RESULT<bool> Remove(const char *Key, bool DontDestroy = false) {
		hashlist_t<Type> *List;

		if (Key == NULL) {
			THROW(bool, Generic_InvalidArgument, "Key cannot be NULL.");
		}

		List = &m_Buckets[Hash(Key, CaseSensitive) % m_BucketCount];

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
			for (int i = 0; i < List->Count; i++) {
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
	int GetLength(void) const {
		return m_LengthCache;
	}

	/**
	 * RegisterValueDestructor
	 *
	 * Registers a value destructor for a hashtable.
	 *
	 * @param Func the value destructor
	 */
	void RegisterValueDestructor(void (*Func)(Type Object)) {
		m_DestructorFunc = Func;
	}

	/**
	 * Iterate
	 *
	 * Returns the Index-th item of the hashtable.
	 *
	 * @param Index the index
	 */
	hash_t<Type> *Iterate(int Index) const {
		static void *thisPointer = NULL;
		static int cache_Index = 0, cache_i = 0, cache_a = 0;
		int Skip = 0;
		int i, a;
		bool first = true;

		if (thisPointer == this && cache_Index == Index - 1) {
			i = cache_i;
			a = cache_a;
			Skip = cache_Index;
		} else {
			i = 0;
			a = 0;
		}

		for (; i < m_BucketCount; i++) {
			if (!first) {
				a = 0;
			} else {
				first = false;
			}

			for (;a < m_Buckets[i].Count; a++) {
				if (Skip == Index) {
					static hash_t<Type> Item;

					Item.Name = m_Buckets[i].Keys[a];
					Item.Value = m_Buckets[i].Values[a];

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
	char **GetSortedKeys(void) const {
		char **Keys = NULL;
		int Count = 0;

		for (int i = 0; i < m_BucketCount; i++) {
			Keys = (char **)realloc(Keys, (Count + m_Buckets[i].Count) * sizeof(char *));

			if (Count + m_Buckets[i].Count > 0 && Keys == NULL) {
				return NULL;
			}

			for (int a = 0; a < m_Buckets[i].Count; a++) {
				Keys[Count + a] = m_Buckets[i].Keys[a];
			}

			Count += m_Buckets[i].Count;
		}

		assert(Count == m_LengthCache);

		qsort(Keys, Count, sizeof(Keys[0]), CmpStringCase);

		Keys = (char **)realloc(Keys, ++Count * sizeof(char *));

		if (Keys == NULL) {
			return NULL;
		}

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
	explicit CHashCompare(const char *String) {
		m_String = String;

		if (String) {
			m_Hash = Hash(String, false);
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
#endif /* SBNC */

#endif /* HASHTABLE_H */
