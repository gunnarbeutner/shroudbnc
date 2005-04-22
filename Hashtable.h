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

#if !defined(AFX_HASHTABLE_H__FA383811_CE36_4970_9236_9F1EAECC6998__INCLUDED_)
#define AFX_HASHTABLE_H__FA383811_CE36_4970_9236_9F1EAECC6998__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

template <typename value_type> struct hash_t {
	bool Valid;
	char* Name;
	value_type Value;
};

template <typename value_type, bool casesensitive> class CHashtable {
	typedef void (DestroyValue)(value_type P);

	hash_t<value_type>* m_Pairs;
	int m_PairCount;
	DestroyValue* m_DestructorFunc;
public:
	CHashtable(void) {
		m_Pairs = NULL;
		m_PairCount = 0;

		m_DestructorFunc = NULL;
	}

	virtual ~CHashtable() {
		for (int i = 0; i < m_PairCount; i++) {
			if (m_DestructorFunc && m_Pairs[i].Valid) {
				m_DestructorFunc(m_Pairs[i].Value);
				free(m_Pairs[i].Name);
			}
		}

		free(m_Pairs);
	}

	virtual void Add(const char* Name, value_type Value) {
		for (int i = 0; i < m_PairCount; i++) {
			if (!m_Pairs[i].Valid) {
				m_Pairs[i].Name = strdup(Name);
				m_Pairs[i].Value = Value;
				m_Pairs[i].Valid = true;

				return;
			} else if ((casesensitive && strcmp(m_Pairs[i].Name, Name) == 0) || (!casesensitive && strcmpi(m_Pairs[i].Name, Name) == 0)) {
				if (m_DestructorFunc)
					m_DestructorFunc(m_Pairs[i].Value);

				m_Pairs[i].Value = Value;

				return;
			}
		}

		m_Pairs = (hash_t<value_type>*)realloc(m_Pairs, ++m_PairCount * sizeof(hash_t<value_type>));

		m_Pairs[m_PairCount - 1].Valid = true;
		m_Pairs[m_PairCount - 1].Name = strdup(Name);
		m_Pairs[m_PairCount - 1].Value = Value;
	}

	virtual void InternalRemove(const char* Name, bool Release = true) {
		for (int i = 0; i < m_PairCount; i++) {
			if (m_Pairs[i].Valid && ((casesensitive && strcmp(m_Pairs[i].Name, Name) == 0) || (!casesensitive && strcmpi(m_Pairs[i].Name, Name) == 0))) {
				free(m_Pairs[i].Name);
				m_Pairs[i].Name = NULL;
				m_Pairs[i].Valid = false;

				if (Release && m_DestructorFunc)
					m_DestructorFunc(m_Pairs[i].Value);

				return;
			}
		}
	}

	virtual void RemoveNoRelease(const char* Name) {
		InternalRemove(Name, false);
	}

	virtual void Remove(const char* Name) {
		InternalRemove(Name, true);
	}

	virtual value_type Get(const char* Name) {
		for (int i = 0; i < m_PairCount; i++) {
			if (m_Pairs[i].Valid && ((casesensitive && strcmp(m_Pairs[i].Name, Name) == 0) || (!casesensitive && strcmpi(m_Pairs[i].Name, Name) == 0))) {
				return m_Pairs[i].Value;
			}
		}

		return NULL;
	}

	virtual hash_t<value_type>* Iterate(int Index) {
		int a = 0;

		for (int i = 0; i < m_PairCount; i++) {
			if (a == Index && m_Pairs[i].Valid)
				return &m_Pairs[i];

			if (m_Pairs[i].Valid)
				a++;
		}

		return NULL;
	}

	virtual hash_t<value_type>* GetInnerData(void) {
		return m_Pairs;
	}

	virtual int GetInnerDataCount(void) {
		return m_PairCount;
	}

	virtual void RegisterValueDestructor(DestroyValue* Func) {
		m_DestructorFunc = Func;
	}
};

#endif // !defined(AFX_HASHTABLE_H__FA383811_CE36_4970_9236_9F1EAECC6998__INCLUDED_)
