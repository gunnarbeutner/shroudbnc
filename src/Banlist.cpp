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

#include "StdAfx.h"

/**
 * DestroyBan
 *
 * Used by CHashtable to destroy individual bans.
 *
 * @param Ban the ban which is going to be destroyed
 */
void DestroyBan(ban_t *Ban) {
	ufree(Ban->Mask);
	ufree(Ban->Nick);
}

/**
 * CBanlist
 *
 * Constructs an empty banlist.
 */
CBanlist::CBanlist(CChannel *Owner, safe_box_t Box) {
	SetOwner(Owner);
	SetBox(Box);

	m_Bans.RegisterValueDestructor(DestroyBan);
}

/**
 * SetBan
 *
 * Creates a new ban.
 *
 * @param Mask the banmask
 * @param Nick the nick of the user who set the ban
 * @param Timestamp the timestamp of the ban
 */
RESULT<bool> CBanlist::SetBan(const char *Mask, const char *Nick, time_t Timestamp) {
	ban_t *Ban;
	safe_box_t BanBox;

	UnsetBan(Mask);

	if (!GetUser()->IsAdmin() && m_Bans.GetLength() >= g_Bouncer->GetResourceLimit("bans")) {
		THROW(bool, Generic_QuotaExceeded, "Too many bans.");
	}

	Ban = (ban_t *)umalloc(sizeof(ban_t));

	CHECK_ALLOC_RESULT(Ban, umalloc) {
		THROW(bool, Generic_OutOfMemory, "umalloc() failed.");
	} CHECK_ALLOC_RESULT_END;

	Ban->Mask = ustrdup(Mask);
	Ban->Nick = ustrdup(Nick);
	Ban->Timestamp = Timestamp;

	if (GetBox() != NULL) {
		BanBox = safe_put_box(GetBox(), Mask);
		safe_put_string(BanBox, "Nick", Nick);
		safe_put_integer(BanBox, "Timestamp", Timestamp);
	}

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
		if (GetBox() != NULL) {
			safe_remove(GetBox(), Mask);
		}

		return m_Bans.Remove(Mask);
	} else {
		THROW(bool, Generic_InvalidArgument, "Mask cannot be NULL.");
	}
}

/**
 * Iterate
 *
 * Iterates through the banlist.
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
 * Thaw
 *
 * Depersists a banlist.
 *
 * @param Box the box
 */
RESULT<CBanlist *> CBanlist::Thaw(safe_box_t Box, CChannel *Owner) {
	CBanlist *Banlist;
	safe_element_t *Previous = NULL;
	safe_box_t BanBox;
	char Mask[256];
	const char *Nick;
	time_t Timestamp;

	if (Box == NULL) {
		THROW(CBanlist *, Generic_InvalidArgument, "Box cannot be NULL.");
	}

	Banlist = new CBanlist(Owner, Box);

	CHECK_ALLOC_RESULT(Banlist, new) {
		THROW(CBanlist *, Generic_OutOfMemory, "new operator failed.");
	} CHECK_ALLOC_RESULT_END;

	while (safe_enumerate(Box, &Previous, Mask, sizeof(Mask)) != -1) {
		BanBox = safe_get_box(Box, Mask);

		if (BanBox == NULL) {
			continue;
		}

		Timestamp = safe_get_integer(BanBox, "Timestamp");
		Nick = safe_get_string(BanBox, "Nick");

		Banlist->SetBan(Mask, Nick, Timestamp);
	}

	RETURN(CBanlist *, Banlist);
}
