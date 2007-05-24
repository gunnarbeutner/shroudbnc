/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007 Gunnar Beutner                                      *
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
 * CPersistable
 *
 * Base class for persistable objects.
 */
class SBNCAPI CPersistable {
private:
	safe_box_t m_Box;

protected:
	void SetBox(safe_box_t Box) {
		m_Box = Box;
	}

public:
	CPersistable(void) {
		m_Box = NULL;
	}

	~CPersistable(void) {
		if (m_Box != NULL) {
			safe_box_t Parent = safe_get_parent(m_Box);
			const char *Name = safe_get_name(m_Box);

			safe_remove(Parent, Name);
		}
	}

	safe_box_t GetBox(void) {
		return m_Box;
	}
};

/**
 * ThawObject
 *
 * Depersists an object which is stored in a box.
 *
 * @param Container the container of the box
 * @param Name the object's name in that container
 */
template <typename Type, typename OwnerType>
RESULT<Type *> ThawObject(safe_box_t Container, const char *Name, OwnerType *Owner) {
	safe_box_t ObjectBox;

	if (Container == NULL || Name == NULL) {
		THROW(Type *, Generic_InvalidArgument, "Container and/or Name cannot be NULL.");
	}

	ObjectBox = safe_get_box(Container, Name);

	if (ObjectBox == NULL) {
		THROW(Type *, Generic_Unknown, "There is no such box.");
	}

	return Type::Thaw(ObjectBox, Owner);
}
