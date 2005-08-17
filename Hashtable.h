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

template <typename Type> struct xhash_t {
	char* Name;
	Type Value;
};

template<typename Type, bool CaseSensitive, int Size, bool VolatileKeys = false> class CHashtable {
	typedef void (DestroyValue)(Type P);

	template <typename HType> struct hash_t {
		int subcount;
		char** keys;
		HType* values;
	};

	hash_t<Type> m_Items[Size];
	DestroyValue* m_DestructorFunc;
public:
	CHashtable(void) {
		for (unsigned int i = 0; i < sizeof(m_Items) / sizeof(hash_t<Type>); i++) {
			m_Items[i].subcount = 0;
			m_Items[i].keys = NULL;
			m_Items[i].values = NULL;
		}

		m_DestructorFunc = NULL;
	}

	~CHashtable(void) {
		for (unsigned int i = 0; i < sizeof(m_Items) / sizeof(hash_t<Type>); i++) {
			hash_t<Type>* P = &m_Items[i];

			for (int a = 0; a < P->subcount; a++) {
				if (!VolatileKeys)
					free(P->keys[a]);

				if (m_DestructorFunc)
					m_DestructorFunc(P->values[a]);
			}

			free(P->keys);
			free(P->values);
		}
	}

	static unsigned char Hash(const char* String) {
		unsigned char Out = 0;
		unsigned int Len = strlen(String);

		for (size_t i = 0; i < Len; i++)
			Out ^= CaseSensitive ? String[i] : toupper(String[i]);

		return Out & (Size - 1);
	}

	void Add(const char* Key, Type Value) {
		if (!Key)
			return;

		Remove(Key);

		hash_t<Type>* P = &m_Items[Hash(Key)];

		P->subcount++;

		P->keys = (char**)realloc(P->keys, P->subcount * sizeof(char*));
		P->values = (Type*)realloc(P->values, P->subcount * sizeof(Type));

		if (VolatileKeys)
			P->keys[P->subcount - 1] = const_cast<char*>(Key);
		else
			P->keys[P->subcount - 1] = strdup(Key);

		P->values[P->subcount -1] = Value;
	}

	Type Get(const char* Key) {
		if (!Key)
			return NULL;

		hash_t<Type>* P = &m_Items[Hash(Key)];

		if (P->subcount == 0)
			return NULL;
		else {
			for (int i = 0; i < P->subcount; i++)
				if (P->keys[i] && (CaseSensitive ? strcmp(P->keys[i], Key) : strcmpi(P->keys[i], Key)) == 0)
					return P->values[i];

			return NULL;
		}
	}

	int Count(void) {
		int Count = 0;

		for (int i = 0; i < sizeof(m_Items) / sizeof(hash_t<Type>); i++)
			Count += m_Items[i].subcount;

		return Count;
	}

	void Remove(const char* Key, bool NoRelease = false) {
		if (!Key)
			return;

		hash_t<Type>* P = &m_Items[Hash(Key)];

		if (P->subcount == 0)
			return;
		else if (P->subcount == 1 && (CaseSensitive ? strcmp(P->keys[0], Key) : strcmpi(P->keys[0], Key)) == 0) {
			if (m_DestructorFunc && !NoRelease)
				m_DestructorFunc(P->values[0]);

			if (!VolatileKeys)
				free(P->keys[0]);

			free(P->keys);
			free(P->values);
			P->subcount = 0;
			P->keys = NULL;
			P->values = NULL;
		} else {
			for (int i = 0; i < P->subcount; i++) {
				if (P->keys[i] && (CaseSensitive ? strcmp(P->keys[i], Key) : strcmpi(P->keys[i], Key)) == 0) {
					if (!VolatileKeys)
						free(P->keys[i]);

					P->keys[i] = P->keys[P->subcount - 1];

					if (m_DestructorFunc && !NoRelease)
						m_DestructorFunc(P->values[i]);

					P->values[i] = P->values[P->subcount - 1];
					P->subcount--;

					break;
				}
			}
		}
	}

	void RegisterValueDestructor(DestroyValue* Func) {
		m_DestructorFunc = Func;
	}

	xhash_t<Type>* Iterate(int Index) {
		int Skip = 0;

		for (unsigned int i = 0; i < sizeof(m_Items) / sizeof(hash_t<Type>); i++) {
			for (int a = 0; a < m_Items[i].subcount; a++) {
				if (Skip == Index) {
					static xhash_t<Type> H;

					H.Name = m_Items[i].keys[a];
					H.Value = m_Items[i].values[a];

					return &H;
				}

				Skip++;
			}
		}

		return NULL;
	}

	char** GetSortedKeys(void) {
		char** Keys = NULL;
		unsigned int Count = 0;

		for (unsigned int i = 0; i < sizeof(m_Items) / sizeof(hash_t<Type>); i++) {
			Keys = (char**)realloc(Keys, (Count + m_Items[i].subcount) * sizeof(char*));

			for (int a = 0; a < m_Items[i].subcount; a++) {
				Keys[Count + a] = m_Items[i].keys[a];
			}

			Count += m_Items[i].subcount;
		}

		qsort(Keys, Count, sizeof(char*), keyStrCmp);

		Keys = (char**)realloc(Keys, ++Count * sizeof(char*));

		Keys[Count - 1] = NULL;

		return Keys;
	}

};

class CHashCompare {
	const char* m_String;
	unsigned char m_Hash;
public:
	CHashCompare(const char* String) {
		m_String = String;

		if (String)
			m_Hash = CHashtable<void*, false, 256>::Hash(String);
		else
			m_Hash = 0;
	}

	inline bool operator==(CHashCompare Other) {
		if (m_Hash != Other.m_Hash)
			return false;
		else
			return (strcmpi(m_String, Other.m_String) == 0);
	}
};
