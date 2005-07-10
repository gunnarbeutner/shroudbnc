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
#include "Hashtable.h"
#include "Banlist.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

void DestroyBan(ban_t* Obj) {
	free(Obj->Mask);
	free(Obj->Nick);
	free(Obj);
}

CBanlist::CBanlist() {
	m_Bans = new CHashtable<ban_t*, false>();
	m_Bans->RegisterValueDestructor(DestroyBan);
}

CBanlist::~CBanlist() {
	delete m_Bans;
}

void CBanlist::SetBan(const char* Mask, const char* Nick, time_t TS) {
	ban_t* Obj = (ban_t*)malloc(sizeof(ban_t));

	Obj->Mask = strdup(Mask);
	Obj->Nick = strdup(Nick);
	Obj->TS = TS;

	m_Bans->Add(Mask, Obj);
}

void CBanlist::UnsetBan(const char* Mask) {
	m_Bans->Remove(Mask);
}

const ban_t* CBanlist::Iterate(int Skip) {
	xhash_t<ban_t*>* Obj = m_Bans->Iterate(Skip);

	if (Obj)
		return Obj->Value;
	else
		return NULL;
}

const ban_t* CBanlist::GetBan(const char* Mask) {
	return m_Bans->Get(Mask);
}
