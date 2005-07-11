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
#include "SocketEvents.h"
#include "Connection.h"
#include "BouncerCore.h"
#include "BouncerUser.h"
#include "TrafficStats.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CConnection::CConnection(SOCKET Client, sockaddr_in Peer) {
	m_Socket = Client;
	m_Owner = NULL;

	sendq = NULL;
	sendq_size = 0;

	recvq = NULL;
	recvq_size = 0;

	m_Locked = false;
	m_Shutdown = false;
	m_Timeout = 0;

	m_Traffic = NULL;

	m_Wrapper = false;
}

CConnection::~CConnection() {
	free(recvq);
	free(sendq);

	g_Bouncer->UnregisterSocket(m_Socket);
}

SOCKET CConnection::GetSocket(void) {
	return m_Socket;
}

CBouncerUser* CConnection::GetOwningClient(void) {
	return m_Owner;
}

bool CConnection::Read(void) {
	char Buffer[8192];

	if (m_Shutdown)
		return true;

	int n = recv(m_Socket, Buffer, sizeof(Buffer), 0);

	if (n > 0) {
		recvq_size += n;
		recvq = (char*)realloc(recvq, recvq_size);
		memcpy(recvq + recvq_size - n, Buffer, n);

		if (m_Traffic)
			m_Traffic->AddInbound(n);
	} else {
		shutdown(m_Socket, SD_BOTH);
		closesocket(m_Socket);

		return false;
	}

	if (m_Wrapper)
		return true;

	char* Line;

	while (ReadLine(&Line)) {
		ParseLine(Line);

		free(Line);
	}

	return true;
}

void CConnection::Write(void) {
	if (sendq_size > 0) {
		int n = send(m_Socket, sendq, sendq_size > 2000 ? 2000 : sendq_size , 0);

		if (n > 0 && m_Traffic)
			m_Traffic->AddOutbound(n);

		if (sendq_size > 2000) {
			char* sendq_new = (char*)malloc(sendq_size - 2000);
			memcpy(sendq_new, &sendq[2000], sendq_size - 2000);
			sendq_size -= 2000;
			
			free(sendq);
			sendq = sendq_new;
		} else
			sendq_size = 0;
	}

	if (m_Shutdown) {
		shutdown(m_Socket, SD_BOTH);
		closesocket(m_Socket);
	}
}

bool CConnection::ReadLine(char** Out) {
	char* old_recvq = recvq;

	if (!recvq)
		return false;

	char* Pos = NULL;

	for (int i = 0; i < recvq_size; i++) {
		if (recvq[i] == '\n') {
			Pos = recvq + i;
			break;
		}
	}

	if (Pos) {
		*Pos = '\0';
		char* NewPtr = Pos + 1;

		recvq_size -= NewPtr - recvq;

		if (recvq_size == 0)
			recvq = NULL;
		else {
			recvq = (char*)malloc(recvq_size);
			memcpy(recvq, NewPtr, recvq_size);
		}

		char* Line = (char*)malloc(strlen(old_recvq) + 1);
		strcpy(Line, old_recvq);
		if (Line[strlen(Line) - 1] == '\r')
			Line[strlen(Line) - 1] = '\0';

		free(old_recvq);

		*Out = Line;
		return true;
	} else {
		*Out = NULL;
		return false;
	}
}

void CConnection::InternalWriteLine(const char* In) {
	if (m_Locked || m_Shutdown)
		return;

	sendq_size += strlen(In) + 2;
	sendq = (char*)realloc(sendq, sendq_size);
	memcpy(sendq + sendq_size - (strlen(In) + 2), In, strlen(In));
	memcpy(sendq + sendq_size - 2, "\r\n", 2);
}

void CConnection::WriteLine(const char* Format, ...) {
	char Out[1024];
	va_list marker;

	va_start(marker, Format);
	vsnprintf(Out, sizeof(Out), Format, marker);
	va_end(marker);

	InternalWriteLine(Out);
}

void CConnection::ParseLine(const char* Line) {
}

connection_role_e CConnection::GetRole(void) {
	return Role_Unknown;
}

void CConnection::Kill(const char* Error) {
	if (m_Shutdown)
		return;

	if (GetRole() == Role_Client) {
		if (m_Owner) {
			m_Owner->SetClientConnection(NULL);
			m_Owner = NULL;
		}

		WriteLine(":Notice!sBNC@shroud.nhq NOTICE * :%s", Error);
	} else if (GetRole() == Role_IRC) {
		if (m_Owner) {
			m_Owner->SetIRCConnection(NULL);
			m_Owner = NULL;
		}

		WriteLine("QUIT :%s", Error);
	}

	Shutdown();
	Timeout(10);
}

bool CConnection::HasQueuedData(void) {
	return sendq_size > 0;
}


int CConnection::SendqSize(void) {
	return sendq_size;
}

int CConnection::RecvqSize(void) {
	return recvq_size;
}

void CConnection::Error(void) {
}

void CConnection::Destroy(void) {
	if (m_Owner)
		m_Owner->SetClientConnection(NULL);

	delete this;
}

void CConnection::Lock(void) {
	m_Locked = true;
}

bool CConnection::IsLocked(void) {
	return m_Locked;
}

void CConnection::Shutdown(void) {
	m_Shutdown = true;
}

void CConnection::Timeout(int TimeLeft) {
	m_Timeout = time(NULL) + TimeLeft;
}

bool CConnection::DoTimeout(void) {
	if (m_Timeout > 0 && m_Timeout < time(NULL)) {
		shutdown(m_Socket, SD_BOTH);
		closesocket(m_Socket);

		delete this;

		return true;
	} else
		return false;
}

void CConnection::AttachStats(CTrafficStats* Stats) {
	m_Traffic = Stats;
}

CTrafficStats* CConnection::GetTrafficStats(void) {
	return m_Traffic;
}

const char* CConnection::ClassName(void) {
	return "CConnection";
}
