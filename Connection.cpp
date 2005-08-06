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
#include "FIFOBuffer.h"
#include "Connection.h"
#include "BouncerCore.h"
#include "BouncerUser.h"
#include "TrafficStats.h"

#define BLOCKSIZE 4096
#define SENDSIZE 4096

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CConnection::CConnection(SOCKET Client) {
	m_Socket = Client;
	m_Owner = NULL;

	m_Locked = false;
	m_Shutdown = false;
	m_Timeout = 0;

	m_Traffic = NULL;

	m_Wrapper = false;

	m_SendQ = new CFIFOBuffer();
	m_RecvQ = new CFIFOBuffer();

	InitSocket();
}

CConnection::~CConnection() {
	delete m_SendQ;
	delete m_RecvQ;

	g_Bouncer->UnregisterSocket(m_Socket);
}

void CConnection::InitSocket(void) {
	if (m_Socket == INVALID_SOCKET)
		return;

#ifndef _WIN32
	const int optBuffer = 4 * SENDSIZE;
	const int optLowWat = SENDSIZE;
	const int optLinger = 0;

	setsockopt(m_Socket, SOL_SOCKET, SO_SNDBUF, &optBuffer, sizeof(optBuffer));
	setsockopt(m_Socket, SOL_SOCKET, SO_SNDLOWAT, &optLowWat, sizeof(optLowWat));
	setsockopt(m_Socket, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
#endif

}

SOCKET CConnection::GetSocket(void) {
	return m_Socket;
}

CBouncerUser* CConnection::GetOwningClient(void) {
	return m_Owner;
}

void* ResizeBuffer(void* Buffer, unsigned int OldSize, unsigned int NewSize) {
	if (OldSize != 0)
		OldSize += BLOCKSIZE - (OldSize % BLOCKSIZE);

	unsigned int CeilNewSize = NewSize + BLOCKSIZE - (NewSize % BLOCKSIZE);

	unsigned int OldBlocks = OldSize / BLOCKSIZE;
	unsigned int NewBlocks = CeilNewSize / BLOCKSIZE;

	assert(NewBlocks * BLOCKSIZE > NewSize);

	if (NewBlocks != OldBlocks)
		return realloc(Buffer, NewBlocks * BLOCKSIZE);
	else
		return Buffer;
}

bool CConnection::Read(bool DontProcess) {
	char Buffer[8192];

	if (m_Shutdown)
		return true;

	int n = recv(m_Socket, Buffer, sizeof(Buffer), 0);

	if (n > 0) {
		m_RecvQ->Write(Buffer, n);

		if (m_Traffic)
			m_Traffic->AddInbound(n);
	} else {
		shutdown(m_Socket, SD_BOTH);
		closesocket(m_Socket);

		return false;
	}

	if (m_Wrapper)
		return true;

	if (DontProcess == false)
		ReadLines();

	return true;
}

void CConnection::Write(void) {
	unsigned int Size = m_SendQ->GetSize();

	if (Size > 0) {
		int n = send(m_Socket, m_SendQ->Peek(), Size > SENDSIZE ? SENDSIZE : Size, 0);

		if (n > 0) {
			if (m_Traffic)
				m_Traffic->AddOutbound(n);

			m_SendQ->Read(n);
		}
	}

	if (m_Shutdown) {
		shutdown(m_Socket, SD_BOTH);
		closesocket(m_Socket);
	}
}

void CConnection::ReadLines(void) {
	char* recvq = m_RecvQ->Peek();
	char* line = recvq;

	unsigned int Size = m_RecvQ->GetSize();

	for (unsigned int i = 0; i < Size; i++) {
		if (recvq[i] == '\n' || recvq[i] == '\r') {
			recvq[i] = '\0';

			if (strlen(line) > 0)
				ParseLine(line);

			line = &recvq[i+1];
		}
	}

	m_RecvQ->Read(line - recvq);
}

// inefficient -- avoid this function at all costs, use ReadLines() instead
bool CConnection::ReadLine(char** Out) {
	char* old_recvq = m_RecvQ->Peek();

	if (!old_recvq)
		return false;

	char* Pos = NULL;

	unsigned int Size = m_RecvQ->GetSize();

	for (unsigned int i = 0; i < Size; i++) {
		if (old_recvq[i] == '\n') {
			Pos = old_recvq + i;
			break;
		}
	}

	if (Pos) {
		*Pos = '\0';
		char* NewPtr = Pos + 1;

		*Out = strdup(m_RecvQ->Read(NewPtr - old_recvq));

		return true;
	} else {
		*Out = NULL;
		return false;
	}
}

void CConnection::InternalWriteLine(const char* In) {
	if (m_Locked || m_Shutdown)
		return;

	m_SendQ->WriteLine(In);
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

	AttachStats(NULL);

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
	return m_SendQ->GetSize() > 0;
}


int CConnection::SendqSize(void) {
	return m_SendQ->GetSize();
}

int CConnection::RecvqSize(void) {
	return m_RecvQ->GetSize();
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

void CConnection::FlushSendQ(void) {
	m_SendQ->Flush();
}
