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
 * CZone<Type>
 *
 * A memory zone used for efficiently storing similar objects.
 */
template<typename Type, int HunkSize>
class CZone {
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
		// TODO: FIX FIX FIX
#ifdef _DEBUG
		if (g_Bouncer) {
			return g_Bouncer->RegisterExitHandler(&ZoneLeakCheck<Type, HunkSize>, this);
		} else {
			return false;
		}
#endif
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
	}

	int GetCount(void) const {
		int Count = 0;

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

	float GetUsage(void) const {
		return GetCount() / (HunkSize * m_Count);
	}

	void LeakCheck(void) const {
		int Count = GetCount();

		if (Count > 0) {
			printf("Leaked %d zone objects of size %d.\n", Count, sizeof(Type));
		}
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

		return m_Zone.Allocate();
	}

	void operator delete(void *Object) {
		m_Zone.Delete((InheritedClass *)Object);
	}

	const CZone<InheritedClass, HunkSize> *GetZone(void) {
		return &m_Zone;
	}
};

template<typename InheritedClass, int HunkSize>
CZone<InheritedClass, HunkSize> CZoneObject<InheritedClass, HunkSize>::m_Zone;

/**
 * DestroyZoneObject<Type>
 *
 * A generic value destructor which can be used in a hashtable.
 */
template<typename Type>
void DestroyZoneObject(Type *Object) {
//	Object->~Type;
}
