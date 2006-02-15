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

/**
 * FreezeObject<Type>
 *
 * Persists an object and stores the persisted data in a box.
 *
 * @param Container the container for the object's box
 * @param Name the object's name in the container
 * @param Object the object
 */
template <typename Type>
bool FreezeObject(CAssocArray *Container, const char *Name, Type *Object) {
	CAssocArray *ObjectBox;

	if (Container == NULL || Name == NULL || Object == NULL) {
		return false;
	}

	ObjectBox = Container->Create();

	if (ObjectBox == NULL) {
		return false;
	}

	if (Object->Freeze(ObjectBox)) {
		Container->AddBox(Name, ObjectBox);
	} else {
		ObjectBox->Destroy();
	}

	return true;
}

/**
 * UnfreezeObject<Type>
 *
 * Depersists an object which is stored in a box.
 *
 * @param Container the container of the box
 * @param Name the object's name in that container
 */
template <typename Type>
Type *UnfreezeObject(CAssocArray *Container, const char *Name) {
	CAssocArray *ObjectBox;

	if (Container == NULL || Name == NULL) {
		return NULL;
	}

	ObjectBox = Container->ReadBox(Name);

	if (ObjectBox == NULL) {
		return NULL;
	}

	return Type::Unfreeze(ObjectBox);
}
