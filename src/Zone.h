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

template<typename Type, int HunkSize>
void ZoneLeakCheck(void *Zone);

/**
 * CZoneInformation
 *
 * Provides information about a zone.
 */
struct CZoneInformation {
public:
	virtual unsigned int GetCount(void) const = 0;
	virtual unsigned int GetCapacity(void) const = 0;
	virtual const char *GetTypeName(void) const = 0;
	virtual size_t GetTypeSize(void) const = 0;
	virtual void PerformLeakCheck(void) const = 0;
};

/**
 * CZone<Type>
 *
 * A memory zone used for efficiently storing similar objects.
 */
template<typename Type, int HunkSize>
class CZone : public CZoneInformation {
	template<typename HunkType>
	struct hunkobject_s {
		bool Valid;
		char Data[sizeof(HunkType)];
	};

	template<typename HunkType, int HunkObjectSize>
	struct hunk_s {
		hunkobject_s<HunkType> HunkObjects[HunkObjectSize];
	};

	hunk_s<Type, HunkSize> **m_Hunks;
	unsigned int m_Count;
	bool m_Registered;

	hunk_s<Type, HunkSize> *AddHunk(void) {
		hunk_s<Type, HunkSize> **NewHunks;
		hunk_s<Type, HunkSize> *NewHunk;

		NewHunk = (hunk_s<Type, HunkSize> *)malloc(sizeof(hunk_s<Type, HunkSize>));

		if (NewHunk == NULL) {
			return NULL;
		}

		NewHunks = (hunk_s<Type, HunkSize> **)realloc(m_Hunks, (m_Count + 1) * sizeof(hunk_s<Type, HunkSize> *));

		if (NewHunks == NULL) {
			free(NewHunk);

			return NULL;
		}

		for (unsigned int i = 0; i < HunkSize; i++) {
			NewHunk->HunkObjects[i].Valid = false;
		}

		m_Hunks = NewHunks;
		m_Count++;

		m_Hunks[m_Count - 1] = NewHunk;

		return NewHunk;
	}

	void Optimize(void) {
		bool UnusedHunk;

		for (unsigned int i = 0; i < m_Count; i++) {
			hunk_s<Type, HunkSize> *Hunk = m_Hunks[i];

			UnusedHunk = true;

			for (unsigned int h = 0; h < HunkSize; h++) {
				hunkobject_s<Type> *HunkObject = &(Hunk->HunkObjects[h]);

				if (HunkObject->Valid) {
					UnusedHunk = false;

					break;
				}
			}

			if (UnusedHunk) {
				m_Hunks[i] = m_Hunks[m_Count - 1];
				m_Count--;

				free(Hunk);
			}
		}
	}

public:
	CZone(void) {
		m_Hunks = NULL;
		m_Count = 0;
	}

	~CZone(void) {
		for (unsigned int i = 0; i < m_Count; i++) {
			free(m_Hunks[i]);
		}

		free(m_Hunks);
	}

	bool Register(void) {
		return RegisterZone(this);
	}

	Type *Allocate(void) {
		hunk_s<Type, HunkSize> *Hunk;

		if (!m_Registered) {
			m_Registered = Register();
		}

		for (unsigned int i = 0; i < m_Count; i++) {
			for (unsigned int h = 0; h < HunkSize; h++) {
				hunkobject_s<Type> *HunkObject = &(m_Hunks[i]->HunkObjects[h]);

				if (!HunkObject->Valid) {
					HunkObject->Valid = true;

					return (Type *)HunkObject->Data;
				}
			}
		}

		Hunk = AddHunk();

		if (Hunk == NULL) {
			return NULL;
		}

		Hunk->HunkObjects[0].Valid = true;

		return (Type *)Hunk->HunkObjects[0].Data;
	}

	void Delete(Type *Object) {
		for (unsigned int i = 0; i < m_Count; i++) {
			for (unsigned int h = 0; h < HunkSize; h++) {
				hunkobject_s<Type> *HunkObject = &(m_Hunks[i]->HunkObjects[h]);

				if ((Type *)HunkObject->Data == Object) {
#ifdef _DEBUG
					if (!HunkObject->Valid) {
						printf("Double free for zone object %p", Object);
					}
#endif

					HunkObject->Valid = false;

					break;
				}
			}
		}

		Optimize();
	}


	virtual void PerformLeakCheck(void) const {
		int Count = GetCount();

		if (Count > 0) {
			printf("Leaked %d zone objects of type \"%s\" (size %d).\n", Count, GetTypeName(), GetTypeSize());
		}
	}

	virtual unsigned int GetCount(void) const {
		unsigned int Count = 0;

		for (unsigned int i = 0; i < m_Count; i++) {
			for (unsigned int h = 0; h < HunkSize; h++) {
				hunkobject_s<Type> *HunkObject = &(m_Hunks[i]->HunkObjects[h]);

				if ((Type *)HunkObject->Valid) {
					Count++;
				}
			}
		}

		return Count;
	}

	virtual unsigned int GetCapacity(void) const {
		return HunkSize * m_Count;
	}

	virtual const char *GetTypeName(void) const {
		return typeid(Type).name();
	}

	virtual size_t GetTypeSize(void) const {
		return sizeof(Type);
	}
};

template<typename Type, int HunkSize>
void ZoneLeakCheck(void *Zone) {
	CZone<Type, HunkSize> *ZoneObject = (CZone<Type, HunkSize> *)Zone;

	ZoneObject->LeakCheck();
}

/**
 * CZoneObject<Type>
 *
 * A class which uses zones for allocation of its objects.
 */
template<typename InheritedClass, int HunkSize = 512>
class CZoneObject {
	static CZone<InheritedClass, HunkSize> m_Zone; /**< the zone for objects of this class */
public:
	void *operator new(size_t Size) {
		assert(Size <= sizeof(InheritedClass));

#ifdef LEAKLEAK
		return malloc(Size);
#else
		return m_Zone.Allocate();
#endif
	}

	void operator delete(void *Object) {
#ifdef LEAKLEAK
		free(Object);
#else
		m_Zone.Delete((InheritedClass *)Object);
#endif
	}

	const CZone<InheritedClass, HunkSize> *GetZone(void) {
		return &m_Zone;
	}
};

template<typename InheritedClass, int HunkSize>
CZone<InheritedClass, HunkSize> CZoneObject<InheritedClass, HunkSize>::m_Zone;
