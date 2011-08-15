/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2011 Gunnar Beutner                                      *
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
	free(Ban->Mask);
	free(Ban->Nick);
	delete Ban;
}

/**
 * CBanlist
 *
 * Constructs an empty banlist.
 */
CBanlist::CBanlist(CChannel *Owner) {
	SetOwner(Owner);

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

	if (!GetUser()->IsAdmin() && m_Bans.GetLength() >= g_Bouncer->GetResourceLimit("bans", GetUser())) {
		THROW(bool, Generic_QuotaExceeded, "Too many bans.");
	}

	Ban = new ban_t;

	if (AllocFailed(Ban)) {
		THROW(bool, Generic_OutOfMemory, "new operator failed.");
	}

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
		RESULT<bool> Result = m_Bans.Remove(Mask);

		return Result;
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
