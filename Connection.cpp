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
#include <openssl/err.h>

#if defined(_WIN32) && defined(USESSL)
extern "C" {
#include <openssl/applink.c>
}
#endif

#define BLOCKSIZE 4096
#define SENDSIZE 4096

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CConnection::CConnection(SOCKET Client, bool SSL) {
	m_Socket = Client;
	m_Owner = NULL;

	m_Locked = false;
	m_Shutdown = false;
	m_Timeout = 0;

	m_Traffic = NULL;

	m_Wrapper = false;

	m_SendQ = new CFIFOBuffer();
	m_RecvQ = new CFIFOBuffer();

	if (m_SendQ == NULL || m_RecvQ == NULL) {
		LOGERROR("new operator failed. Sendq/recvq could not be created.");

		Kill("Internal error.");
	}

#ifdef USESSL
	m_HasSSL = SSL;
#endif

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

#ifdef USESSL
	if (m_HasSSL) {
		BIO* rbio, *wbio;

		m_SSL = SSL_new(g_Bouncer->GetSSLContext());

		rbio = SSL_get_rbio(m_SSL);
		wbio = SSL_get_wbio(m_SSL);

		BIO_set_nbio(rbio, 1);
		BIO_set_nbio(wbio, 1);

		SSL_set_fd(m_SSL, m_Socket);

		if (GetRole() == Role_Client)
			SSL_set_connect_state(m_SSL);
		else
			SSL_set_accept_state(m_SSL);
	} else
		m_SSL = NULL;
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
	int n;
	char Buffer[8192];

	if (m_Shutdown)
		return true;

#ifdef USESSL
	int code;

	if (m_HasSSL) {
		if (SSL_want_write(m_SSL) && !SSL_want_read(m_SSL))
			return true;

		n = SSL_read(m_SSL, Buffer, sizeof(Buffer));

		if (n < 0) {
			code = SSL_get_error(m_SSL, n);

			if (code == SSL_ERROR_ZERO_RETURN || code == SSL_ERROR_NONE || code == SSL_ERROR_WANT_READ)
				return true;
			else {
				return false;
			}
		}

		ERR_print_errors_fp(stdout);
	} else
#endif
		n = recv(m_Socket, Buffer, sizeof(Buffer), 0);

#ifdef USESSL
	ERR_print_errors_fp(stdout);
#endif

	if (n > 0) {
		m_RecvQ->Write(Buffer, n);

		if (m_Traffic)
			m_Traffic->AddInbound(n);
	} else {
#ifdef USESSL
		if (m_HasSSL)
			SSL_shutdown(m_SSL);
#endif

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
	unsigned int Size;

	if (m_SendQ)
		Size = m_SendQ->GetSize();
	else
		Size = 0;

	if (Size > 0) {
		int n;

#ifdef USESSL
		if (m_HasSSL) {
			if (SSL_want_read(m_SSL))
				return;

			n = SSL_write(m_SSL, m_SendQ->Peek(), Size > SENDSIZE ? SENDSIZE : Size);

			if (n == -1 && SSL_get_error(m_SSL, n) == SSL_ERROR_WANT_READ)
				return;
		} else
#endif
			n = send(m_Socket, m_SendQ->Peek(), Size > SENDSIZE ? SENDSIZE : Size, 0);

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
	unsigned int Size;
	
	if (m_RecvQ == NULL)
		return;

	Size = m_RecvQ->GetSize();

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
	char* old_recvq;
	unsigned int Size;
	char* Pos = NULL;

	if (m_RecvQ == NULL) {
		*Out = NULL;
		return false;
	}

	old_recvq = m_RecvQ->Peek();

	if (!old_recvq)
		return false;

	Size = m_RecvQ->GetSize();

	for (unsigned int i = 0; i < Size; i++) {
		if (old_recvq[i] == '\r' || old_recvq[i] == '\n') {
			Pos = old_recvq + i;
			break;
		}
	}

	if (Pos) {
		*Pos = '\0';
		char* NewPtr = Pos + 1;

		*Out = strdup(m_RecvQ->Read(NewPtr - old_recvq));

		if (*Out == NULL) {
			LOGERROR("strdup() failed.");
			
			return false;
		}

		return true;
	} else {
		*Out = NULL;
		return false;
	}
}

void CConnection::InternalWriteLine(const char* In) {
	if (m_Locked || m_Shutdown)
		return;

	if (m_SendQ)
		m_SendQ->WriteLine(In);
}

void CConnection::WriteLine(const char* Format, ...) {
	char* Out;
	va_list marker;

	va_start(marker, Format);
	vasprintf(&Out, Format, marker);
	va_end(marker);

	if (Out == NULL) {
		LOGERROR("vasprintf() failed. Could not write line (%s).", Format);

		return;
	}

	InternalWriteLine(Out);

	free(Out);
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

		WriteLine(":Notice!notice!shroudbnc.org NOTICE * :%s", Error);
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
	if (m_SendQ)
		return m_SendQ->GetSize() > 0;
	else
		return 0;
}


int CConnection::SendqSize(void) {
	if (m_SendQ)
		return m_SendQ->GetSize();
	else
		return 0;
}

int CConnection::RecvqSize(void) {
	if (m_RecvQ)
		return m_RecvQ->GetSize();
	else
		return 0;
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
	if (m_SendQ)
		m_SendQ->Flush();
}

bool CConnection::IsSSL(void) {
#ifdef USESSL
	return m_HasSSL;
#else
	return false;
#endif
}

void* CConnection::GetPeerCertificate(void) {
#ifdef USESSL
	if (m_HasSSL/* && SSL_get_verify_result(m_SSL) == X509_V_OK*/) {
		return SSL_get_peer_certificate(m_SSL);
	}
#endif

	return NULL;
}
