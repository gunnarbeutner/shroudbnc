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

CClientConnectionMultiplexer::CClientConnectionMultiplexer(CUser *User) : CClientConnection(INVALID_SOCKET) {
	SetOwner(User);
}

void CClientConnectionMultiplexer::ParseLine(const char *Line) {
}

const char *CClientConnectionMultiplexer::GetNick(void) const {
	return GetOwner()->GetPrimaryClientConnection()->GetNick();
}

const char *CClientConnectionMultiplexer::GetPeerName(void) const {
	return GetOwner()->GetPrimaryClientConnection()->GetPeerName();
}

void CClientConnectionMultiplexer::Kill(const char *Error) {

}

void CClientConnectionMultiplexer::Destroy(void) {
	Kill("Destroying clients.");
}

commandlist_t *CClientConnectionMultiplexer::GetCommandList(void) {
	return NULL;
}

SOCKET CClientConnectionMultiplexer::Hijack(void) {
	return INVALID_SOCKET;
}

void CClientConnectionMultiplexer::ChangeNick(const char *NewNick) {
	CVector<client_t> *Clients = GetOwner()->GetClientConnections();

	for (unsigned int i = 0; i < Clients->GetLength(); i++) {
		(*Clients)[i].Client->ChangeNick(NewNick);
	}
}

void CClientConnectionMultiplexer::SetNick(const char *NewNick) {
	CVector<client_t> *Clients = GetOwner()->GetClientConnections();

	for (unsigned int i = 0; i < Clients->GetLength(); i++) {
		(*Clients)[i].Client->SetNick(NewNick);
	}
}

void CClientConnectionMultiplexer::Privmsg(const char *Text) {
	CVector<client_t> *Clients = GetOwner()->GetClientConnections();

	for (unsigned int i = 0; i < Clients->GetLength(); i++) {
		(*Clients)[i].Client->Privmsg(Text);
	}
}

void CClientConnectionMultiplexer::RealNotice(const char *Text) {
	CVector<client_t> *Clients = GetOwner()->GetClientConnections();

	for (unsigned int i = 0; i < Clients->GetLength(); i++) {
		(*Clients)[i].Client->RealNotice(Text);
	}
}
