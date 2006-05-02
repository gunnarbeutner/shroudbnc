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

#include "StdAfx.h"

/**
 * DestroyBan
 *
 * Used by CHashtable to destroy individual bans
 *
 * @param Ban the ban which is going to be destroyed
 */
void DestroyBan(ban_t *Ban) {
	free(Ban->Mask);
	free(Ban->Nick);
}

/**
 * CBanlist
 *
 * Constructs an empty banlist
 */
CBanlist::CBanlist(void) {
	m_Bans.RegisterValueDestructor(DestroyBan);
}

/**
 * SetBan
 *
 * Creates a new ban
 *
 * @param Mask the banmask
 * @param Nick the nick of the user who set the ban
 * @param Timestamp the timestamp of the ban
 */
RESULT<bool> CBanlist::SetBan(const char *Mask, const char *Nick, time_t Timestamp) {
	ban_t *Ban;

	Ban = (ban_t *)malloc(sizeof(ban_t));

	CHECK_ALLOC_RESULT(Ban, malloc) {
		THROW(bool, Generic_OutOfMemory, "malloc() failed.");
	} CHECK_ALLOC_RESULT_END;

	Ban->Mask = strdup(Mask);
	Ban->Nick = strdup(Nick);
	Ban->Timestamp = Timestamp;

	return m_Bans.Add(Mask, Ban);
}

/**
 * UnsetBan
 *
 * Removes a ban from the banlist.
 *
 * @param Mask the mask of the ban which is going to be removed
 */
RESULT<bool> CBanlist::UnsetBan(const char *Mask) {
	if (Mask != NULL) {
		return m_Bans.Remove(Mask);
	} else {
		THROW(bool, Generic_InvalidArgument, "Mask cannot be NULL.");
	}
}

/**
 * Iterate
 *
 * Iterates through the banlist
 *
 * @param Skip the index of the ban which is to be returned
 */
const hash_t<ban_t *> *CBanlist::Iterate(int Skip) const {
	return m_Bans.Iterate(Skip);
}

/**
 * GetBan
 *
 * Returns the ban whose banmask matches the given Mask
 *
 * @param Mask the banmask
 */
const ban_t *CBanlist::GetBan(const char *Mask) const {
	return m_Bans.Get(Mask);
}

/**
 * Freeze
 *
 * Persists a banlist.
 *
 * @param Box the box which should be used for persisting the banlist
 */
RESULT<bool> CBanlist::Freeze(CAssocArray *Box) {
	char *Index;
	ban_t *Ban;
	unsigned int Count;

	if (Box == NULL) {
		THROW(bool, Generic_InvalidArgument, "Box cannot be NULL.");
	}

	Count = m_Bans.GetLength();

	for (unsigned i = 0; i < Count; i++) {
		Ban = m_Bans.Iterate(i)->Value;

		asprintf(&Index, "%d.mask", i);
		CHECK_ALLOC_RESULT(Index, asprintf) {
			THROW(bool, Generic_OutOfMemory, "asprintf() failed.");
		} CHECK_ALLOC_RESULT_END;
		Box->AddString(Index, Ban->Mask);
		free(Index);

		asprintf(&Index, "%d.nick", i);
		CHECK_ALLOC_RESULT(Index, asprintf) {
			THROW(bool, Generic_OutOfMemory, "asprintf() failed.");
		} CHECK_ALLOC_RESULT_END;
		Box->AddString(Index, Ban->Nick);
		free(Index);

		asprintf(&Index, "%d.ts", i);
		CHECK_ALLOC_RESULT(Index, asprintf) {
			THROW(bool, Generic_OutOfMemory, "asprintf() failed.");
		} CHECK_ALLOC_RESULT_END;
		Box->AddInteger(Index, Ban->Timestamp);
		free(Index);
	}

	delete this;

	RETURN(bool, true);
}

/**
 * Thaw
 *
 * Depersists a banlist.
 *
 * @param Box the box
 */
RESULT<CBanlist *> CBanlist::Thaw(CAssocArray *Box) {
	CBanlist *Banlist;
	char *Index;
	const char *Mask, *Nick;
	time_t Timestamp;
	unsigned int i = 0;

	if (Box == NULL) {
		THROW(CBanlist *, Generic_InvalidArgument, "Box cannot be NULL.");
	}

	Banlist = new CBanlist();

	CHECK_ALLOC_RESULT(Banlist, new) {
		THROW(CBanlist *, Generic_OutOfMemory, "new operator failed.");
	} CHECK_ALLOC_RESULT_END;

	while (true) {
		asprintf(&Index, "%d.mask", i);
		CHECK_ALLOC_RESULT(Index, asprintf) {
			delete Banlist;
			THROW(CBanlist *, Generic_OutOfMemory, "asprintf() failed.");
		} CHECK_ALLOC_RESULT_END;
		Mask = Box->ReadString(Index);
		free(Index);

		if (Mask == NULL) {
			break;
		}

		asprintf(&Index, "%d.nick", i);
		CHECK_ALLOC_RESULT(Index, asprintf) {
			delete Banlist;
			THROW(CBanlist *, Generic_OutOfMemory, "asprintf() failed.");
		} CHECK_ALLOC_RESULT_END;
		Nick = Box->ReadString(Index);
		free(Index);

		if (Nick == NULL) {
			break;
		}

		asprintf(&Index, "%d.ts", i);
		CHECK_ALLOC_RESULT(Index, asprintf) {
			delete Banlist;
			THROW(CBanlist *, Generic_OutOfMemory, "asprintf() failed.");
		} CHECK_ALLOC_RESULT_END;
		Timestamp = Box->ReadInteger(Index);
		free(Index);

		Banlist->SetBan(Mask, Nick, Timestamp);

		i++;
	}

	RETURN(CBanlist *, Banlist);
}
