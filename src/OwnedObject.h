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

class CUser;

class COwnedObjectBase {
protected:
	void *m_Owner; /**< the owner of this object */

public:
	virtual CUser *GetUser(void) { return NULL; };
};

/**
 * COwnedObject<Type>
 *
 * A class which is owned by another class.
 */
template <typename Type>
class COwnedObject : protected COwnedObjectBase {
protected:
	/**
	 * COwnedObject
	 *
	 * Constructs a new COwnedObject.
	 */
	COwnedObject(void) {
		m_Owner = NULL;
	}

public:
	/**
	 * SetOwner
	 *
	 * Sets the owner of this object.
	 *
	 * @param Owner the new owner
	 */
	virtual void SetOwner(Type *Owner) {
		m_Owner = Owner;
	}

	/**
	 * GetOwner
	 *
	 * Returns the owner of this object.
	 */
	virtual Type *GetOwner(void) const {
		return (Type *)m_Owner;
	}

	virtual CUser *GetUser(void) const {
		if (typeid(Type) == typeid(CUser)) {
			return (CUser *)m_Owner;
		} else if (m_Owner != NULL) {
			return ((COwnedObjectBase *)m_Owner)->GetUser();
		} else {
			return NULL;
		}
	}
};
