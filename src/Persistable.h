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
 * FreezeObject
 *
 * Persists an object and stores the persisted data in a box.
 *
 * @param Container the container for the object's box
 * @param Name the object's name in the container
 * @param Object the object
 */
template <typename Type>
RESULT<bool> FreezeObject(CAssocArray *Container, const char *Name, Type *Object) {
	CAssocArray *ObjectBox;
	RESULT<bool> Result;

	if (Container == NULL || Name == NULL || Object == NULL) {
		THROW(bool, Generic_InvalidArgument, "Container, Name and/or Object cannot be NULL.");
	}

	ObjectBox = Container->Create();

	if (ObjectBox == NULL) {
		THROW(bool, Generic_OutOfMemory, "Create() failed.");
	}

	Result = Object->Freeze(ObjectBox);

	if (!IsError(Result)) {
		Container->AddBox(Name, ObjectBox);
	} else {
		ObjectBox->Destroy();
	}

	return Result;
}

/**
 * ThawObject
 *
 * Depersists an object which is stored in a box.
 *
 * @param Container the container of the box
 * @param Name the object's name in that container
 */
template <typename Type, typename OwnerType>
RESULT<Type *> ThawObject(CAssocArray *Container, const char *Name, OwnerType *Owner) {
	CAssocArray *ObjectBox;

	if (Container == NULL || Name == NULL) {
		THROW(Type *, Generic_InvalidArgument, "Container and/or Name cannot be NULL.");
	}

	ObjectBox = Container->ReadBox(Name);

	if (ObjectBox == NULL) {
		THROW(Type *, Generic_Unknown, "There is no such box.");
	}

	return Type::Thaw(ObjectBox, Owner);
}
