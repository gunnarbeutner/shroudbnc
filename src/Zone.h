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
 * hunkobject_t
 *
 * A single hunk object.
 */
template<typename Type>
struct hunkobject_s {
	bool Valid; /**< specifies whether this object is in use */
	char Data[sizeof(Type)]; /**< the object's data */
};

/**
 * CZone<Type>
 *
 * A memory zone used for efficiently storing similar objects.
 */
template<typename Type, int HunkSize>
class CZone : public CZoneInformation {

	template<typename HunkType, int HunkObjectSize>
	struct hunk_s {
		hunkobject_s<HunkType> HunkObjects[HunkObjectSize]; /**< the objects in this hunk */
	};

	hunk_s<Type, HunkSize> **m_Hunks;
	unsigned int m_Count;
	bool m_Registered;

	/**
	 * AddHunk
	 *
	 * Creates a new hunk of memory in this zone.
	 */
	hunk_s<Type, HunkSize> *AddHunk(void) {
		hunk_s<Type, HunkSize> **NewHunks;
		hunk_s<Type, HunkSize> *NewHunk;

		NewHunk = (hunk_s<Type, HunkSize> *)malloc(sizeof(hunk_s<Type, HunkSize>));

		if (NewHunk == NULL) {
			return NULL;
		}

		mmark(NewHunk);

		NewHunks = (hunk_s<Type, HunkSize> **)realloc(m_Hunks, (m_Count + 1) * sizeof(hunk_s<Type, HunkSize> *));

		if (NewHunks == NULL) {
			free(NewHunk);

			return NULL;
		}

		mmark(NewHunks);

		for (unsigned int i = 0; i < HunkSize; i++) {
			NewHunk->HunkObjects[i].Valid = false;
		}

		m_Hunks = NewHunks;
		m_Count++;

		m_Hunks[m_Count - 1] = NewHunk;

		return NewHunk;
	}

	/**
	 * Optimize
	 *
	 * Optimizes the zone by removing unused hunks.
	 */
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
	/**
	 * CZone
	 *
	 * Constructs a new memory zone which can be used for
	 * allocating fixed-sized memory blocks.
	 */
	CZone(void) {
		m_Hunks = NULL;
		m_Count = 0;
	}

	/**
	 * ~CZone
	 *
	 * Destructs a memory zone.
	 */
	virtual ~CZone(void) {
		for (unsigned int i = 0; i < m_Count; i++) {
			free(m_Hunks[i]);
		}

		free(m_Hunks);
	}

	/**
	 * Register
	 *
	 * Registers a zone.
	 */
	bool Register(void) {
		return RegisterZone(this);
	}

	/**
	 * Allocate
	 *
	 * Allocates a new object from the memory zone.
	 */
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

	/**
	 * Delete
	 *
	 * Marks an object as unused.
	 */
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

	/**
	 * PerformLeakCheck
	 *
	 * Checks the zone for leaked objects. This function should be called
	 * when there _should_ be no more active objects in this zone.
	 */
	virtual void PerformLeakCheck(void) const {
#ifdef _DEBUG
		int Count = GetCount();

		if (Count > 0) {
			printf("Leaked %d zone objects of type \"%s\" (size %d).\n", Count, GetTypeName(), GetTypeSize());
		}
#endif
	}

	/**
	 * GetCount
	 *
	 * Returns the number of active objects in the memory zone.
	 */
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

	/**
	 * GetCapacity
	 *
	 * Returns the number of objects which can be active in this zone
	 * without having to allocate new hunks.
	 */
	virtual unsigned int GetCapacity(void) const {
		return HunkSize * m_Count;
	}

	/**
	 * GetTypeName
	 *
	 * Returns the typename of the objects which can be allocated
	 * using the zone object.
	 */
	virtual const char *GetTypeName(void) const {
		return typeid(Type).name();
	}

	/**
	 * GetTypeSize
	 *
	 * Returns the size of the objects which can be allocated
	 * using the zone object.
	 */
	virtual size_t GetTypeSize(void) const {
		return sizeof(Type);
	}
};

/**
 * ZoneLeakCheck<>
 *
 * Performs a leakcheck on a zone object.
 */
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
	/**
	 * operator new
	 *
	 * Overrides the operator "new" for classes which inherit from this class.
	 *
	 * @param Size the requested size of the new memory block
	 */
	void *operator new(size_t Size) throw() {
		assert(Size <= sizeof(InheritedClass));

		return m_Zone.Allocate();
	}

	void *operator new(size_t Size, CMemoryManager *Manager) throw() {
		assert(Size <= sizeof(InheritedClass));

		if (!Manager->MemoryAddBytes(Size)) {
			return NULL;
		}

		Manager->MemoryRemoveBytes(Size);

		return m_Zone.Allocate();
	}

	/**
	 * operator delete
	 *
	 * Overrides the operator "delete" for classes which inherit from this class.
	 *
	 * @param Object the object whics is to be deleted
	 */
	void operator delete(void *Object) {
		m_Zone.Delete((InheritedClass *)Object);
	}

	const CZone<InheritedClass, HunkSize> *GetZone(void) {
		return &m_Zone;
	}
};

template<typename InheritedClass, int HunkSize>
CZone<InheritedClass, HunkSize> CZoneObject<InheritedClass, HunkSize>::m_Zone;

#define unew new (GETUSER())
