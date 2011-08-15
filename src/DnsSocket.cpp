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

CDnsSocket::CDnsSocket(SOCKET Socket, bool Outbound) {
	m_Socket = Socket;
	m_Outbound = Outbound;

	g_Bouncer->RegisterSocket(m_Socket, this);
}

void CDnsSocket::Destroy(void) {
	g_Bouncer->UnregisterSocket(m_Socket);
}

int CDnsSocket::Read(bool DontProcess) {
	ares_process_fd(CDnsQuery::GetDnsChannel(), m_Socket, ARES_SOCKET_BAD);

	return 0;
}

int CDnsSocket::Write(void) {
	ares_process_fd(CDnsQuery::GetDnsChannel(), ARES_SOCKET_BAD, m_Socket);

	return 0;
}

void CDnsSocket::Error(int ErrorCode) {
}

bool CDnsSocket::HasQueuedData(void) const {
	return m_Outbound;
}

bool CDnsSocket::ShouldDestroy(void) const {
	return false;
}

const char *CDnsSocket::GetClassName(void) const {
	return "CDnsSocket";
}
