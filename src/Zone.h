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
class SBNCAPI CZone : public CZoneInformation {

	template<typename HunkType, int HunkObjectSize>
	struct hunk_s {
		bool Full;
		hunk_s<HunkType, HunkObjectSize> *NextHunk;
		hunkobject_s<HunkType> HunkObjects[HunkObjectSize]; /**< the objects in this hunk */
	};

	hunk_s<Type, HunkSize> *m_FirstHunk;

	unsigned int m_DeletedItems;
	unsigned int m_Count;
	bool m_Registered;

	/**
	 * AddHunk
	 *
	 * Creates a new hunk of memory in this zone.
	 */
	hunk_s<Type, HunkSize> *AddHunk(void) {
		hunk_s<Type, HunkSize> *NewHunk;

		NewHunk = (hunk_s<Type, HunkSize> *)malloc(sizeof(hunk_s<Type, HunkSize>));

		if (NewHunk == NULL) {
			return NULL;
		}

		mmark(NewHunk);

		NewHunk->NextHunk = m_FirstHunk;
		m_FirstHunk = NewHunk;

		NewHunk->Full = false;

		for (unsigned int i = 0; i < HunkSize; i++) {
			NewHunk->HunkObjects[i].Valid = false;
		}

		return NewHunk;
	}

	/**
	 * Optimize
	 *
	 * Optimizes the zone by removing unused hunks.
	 */
	void Optimize(void) {
		bool UnusedHunk;
		hunk_s<Type, HunkSize> *PreviousHunk = m_FirstHunk;
		hunk_s<Type, HunkSize> *Hunk = m_FirstHunk->NextHunk;

		while (Hunk != NULL) {
			UnusedHunk = true;

			if (!Hunk->Full) {
				for (unsigned int h = 0; h < HunkSize; h++) {
					hunkobject_s<Type> *HunkObject = &(Hunk->HunkObjects[h]);

					if (HunkObject->Valid) {
						UnusedHunk = false;

						break;
					}
				}
			} else {
				UnusedHunk = false;
			}

			if (UnusedHunk) {
				PreviousHunk->NextHunk = Hunk->NextHunk;

				free(Hunk);

				Hunk = PreviousHunk->NextHunk;
			} else {
				PreviousHunk = Hunk;

				Hunk = Hunk->NextHunk;
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
		m_Count = 0;
		m_DeletedItems = 0;
		m_FirstHunk = NULL;
	}

	/**
	 * ~CZone
	 *
	 * Destructs a memory zone.
	 */
	virtual ~CZone(void) {
		hunk_s<Type, HunkSize> *Hunk;

		if (m_FirstHunk != NULL) {
			Hunk = m_FirstHunk->NextHunk;

			while (Hunk != NULL) {
				hunk_s<Type, HunkSize> *NextHunk = Hunk->NextHunk;

				free(Hunk);

				Hunk = NextHunk;
			}
		}
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

		Hunk = m_FirstHunk;

		while (Hunk != NULL) {
			if (!Hunk->Full) {
				for (unsigned int h = 0; h < HunkSize; h++) {
					hunkobject_s<Type> *HunkObject = &(Hunk->HunkObjects[h]);

					if (!HunkObject->Valid) {
						HunkObject->Valid = true;

						m_Count++;

						return (Type *)HunkObject->Data;
					}
				}

				Hunk->Full = true;
			}

			Hunk = Hunk->NextHunk;
		}

		Hunk = AddHunk();

		if (Hunk == NULL) {
			return NULL;
		}

		m_Count++;

		Hunk->HunkObjects[0].Valid = true;

		return (Type *)Hunk->HunkObjects[0].Data;
	}

	/**
	 * Delete
	 *
	 * Marks an object as unused.
	 */
	void Delete(Type *Object) {
		hunk_s<Type, HunkSize> *Hunk = m_FirstHunk;
		hunkobject_s<Type> *HunkObject = NULL;
		hunk_s<Type, HunkSize> *OwningHunk;

		HunkObject = (hunkobject_s<Type> *)((char *)Object - ((char *)HunkObject->Data - (char *)HunkObject));

		if (!HunkObject->Valid) {
			printf("Double free for zone object %p", Object);
		} else {
			m_Count--;

			OwningHunk = NULL;

			while (Hunk != NULL) {
				if (HunkObject >= Hunk->HunkObjects && HunkObject < &(Hunk->HunkObjects[HunkSize])) {
					OwningHunk = Hunk;

					break;
				}

				Hunk = Hunk->NextHunk;
			}

			if (OwningHunk != NULL) {
				OwningHunk->Full = false;
			} else {
				printf("CZone::Delete(): Couldn't find hunk for an object.\n");
			}
		}

		HunkObject->Valid = false;

		m_DeletedItems++;

		if ((m_DeletedItems % 10) == 0) {
			Optimize();
		}
	}

	/**
	 * PerformLeakCheck
	 *
	 * Checks the zone for leaked objects. This function should be called
	 * when there _should_ be no more active objects in this zone.
	 */
	void PerformLeakCheck(void) const {
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
	unsigned int GetCount(void) const {
		return m_Count;
	}

	/**
	 * GetTypeName
	 *
	 * Returns the typename of the objects which can be allocated
	 * using the zone object.
	 */
	const char *GetTypeName(void) const {
		return typeid(Type).name();
	}

	/**
	 * GetTypeSize
	 *
	 * Returns the size of the objects which can be allocated
	 * using the zone object.
	 */
	size_t GetTypeSize(void) const {
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
