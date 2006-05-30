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

class CObjectBase {
	enum ObjectType_e {
		eUser,
		eObject
	} m_Type;

	union {
		CUser *User;
		CObjectBase *Object;
	} m_Owner;

protected:
	CObjectBase(void) {
		m_Owner.Object = NULL;
		m_Type = eObject;
	}

	CObjectBase *GetOwnerBase(void) const {
		return m_Owner.Object;
	}

	void SetOwnerBase(CObjectBase *Owner) {
		m_Owner.Object = Owner;
		m_Type = eObject;
	}

	int GetTypeBase(void) {
		return m_Type;
	}

	void SetTypeBase(int Type) {
		m_Type = (ObjectType_e)Type;
	}

	void SetUser(CUser *User) {
		m_Owner.User = User;
		m_Type = eUser;
	}

public:
	virtual CUser *GetUser(void) const {
		if (m_Type == eUser) {
			return m_Owner.User;
		}

		return NULL;
	}
};

template<typename ObjectType, typename OwnerType>
class CObject : public CObjectBase {
protected:
	CObject(void) {	}

	CObject(OwnerType *Owner) {
		SetOwner(Owner);
	}

	virtual ~CObject(void) {
		SetOwner(NULL);
	}

public:
	virtual CUser *GetUser(void) const {
		CUser *User;

		User = CObjectBase::GetUser();

		if (User != NULL) {
			return User;
		}

		if (GetOwnerBase() != NULL) {
			return GetOwnerBase()->GetUser();
		} else {
			return NULL;
		}
	}

	virtual OwnerType *GetOwner(void) const {
		return (OwnerType *)GetOwnerBase();
	}

	virtual void SetOwner(OwnerType *Owner) {
		CUser *User;

		if (GetOwnerBase() != NULL) {
			User = GetUser();

			SetOwnerHelper(User, sizeof(ObjectType), false);
		}

		if (typeid(Owner) == typeid(CUser *)) {
			User = (CUser *)Owner;

			SetUser(User);
		} else {
			SetOwnerBase(dynamic_cast<CObjectBase *>(Owner));
			User = GetUser();
		}

		if (User != NULL) {
			SetOwnerHelper(User, sizeof(ObjectType), true);
		}
	}
};
