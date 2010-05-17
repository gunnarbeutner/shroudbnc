/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007,2010 Gunnar Beutner                                 *
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

#ifndef BANLIST_H
#define BANLIST_H

/**
 * ban_s
 *
 * The structure used for storing bans.
 */
typedef struct ban_s {
	char *Mask;
	char *Nick;
	time_t Timestamp;
} ban_t;

class CChannel;

/**
 * CBanlist
 *
 * a list of bans
 */
class SBNCAPI CBanlist : public CObject<CBanlist, CChannel> {
private:
	CHashtable<ban_t *, false> m_Bans; /**< the actual list of bans. */

public:
#ifndef SWIG
	CBanlist(CChannel *Owner);
#endif /* SWIG */

	RESULT<bool> SetBan(const char *Mask, const char *Nick, time_t Timestamp);
	RESULT<bool> UnsetBan(const char *Mask);

	const ban_t *GetBan(const char *Mask) const;
	const hash_t<ban_t *> *Iterate(int Skip) const;
};

#endif /* BANLIST_H */
