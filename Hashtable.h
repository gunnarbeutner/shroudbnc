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

#if !defined(AFX_HASHTABLE_H__E4C049B1_D51E_4EBE_AD3A_E96BC93BFA4A__INCLUDED_)
#define AFX_HASHTABLE_H__E4C049B1_D51E_4EBE_AD3A_E96BC93BFA4A__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

template <typename Type> struct xhash_t {
	char* Name;
	Type Value;
};

template<typename Type, bool CaseSensitive> class CHashtable {
	typedef void (DestroyValue)(Type P);

	template <typename HType> struct hash_t {
		int subcount;
		char** keys;
		HType* values;
	};

	hash_t<Type> m_Items[0xFF];
	DestroyValue* m_DestructorFunc;

	int Hash(const char* String) {
		int Out = 0;

		for (size_t i = 0; i < strlen(String); i++)
			Out ^= String[i];

		return Out & 0xFF;
	}
public:
	CHashtable(void) {
		for (int i = 0; i < sizeof(m_Items) / sizeof(hash_t<Type>); i++) {
			m_Items[i].subcount = 0;
			m_Items[i].keys = NULL;
			m_Items[i].values = NULL;
		}

		m_DestructorFunc = NULL;
	}

	~CHashtable(void) {
		for (int i = 0; i < sizeof(m_Items) / sizeof(hash_t<Type>); i++) {
			hash_t<Type>* P = &m_Items[i];

			for (int a = 0; a < P->subcount; a++) {
				free(P->keys[a]);

				if (m_DestructorFunc)
					m_DestructorFunc(P->values[a]);
			}

			free(P->keys);
			free(P->values);
		}
	}

	void Add(const char* Key, Type Value) {
		Remove(Key);

		hash_t<Type>* P = &m_Items[Hash(Key)];

		P->subcount++;

		P->keys = (char**)realloc(P->keys, P->subcount * sizeof(char*));
		P->values = (Type*)realloc(P->values, P->subcount * sizeof(Type));

		P->keys[P->subcount - 1] = strdup(Key);
		P->values[P->subcount -1] = Value;
	}

	Type Get(const char* Key) {
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
		hash_t<Type>* P = &m_Items[Hash(Key)];

		if (P->subcount == 0)
			return;
		else if (P->subcount == 1 && (CaseSensitive ? strcmp(P->keys[0], Key) : strcmpi(P->keys[0], Key)) == 0) {
			if (m_DestructorFunc && !NoRelease)
				m_DestructorFunc(P->values[0]);

			free(P->keys[0]);
			free(P->keys);
			free(P->values);
			P->subcount = 0;
			P->keys = NULL;
			P->values = NULL;
		} else {
			for (int i = 0; i < P->subcount; i++) {
				if (P->keys[i] && (CaseSensitive ? strcmp(P->keys[i], Key) : strcmpi(P->keys[i], Key)) == 0) {
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

		for (int i = 0; i < sizeof(m_Items) / sizeof(hash_t<Type>); i++) {
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

};

#endif // !defined(AFX_HASHTABLE_H__E4C049B1_D51E_4EBE_AD3A_E96BC93BFA4A__INCLUDED_)
