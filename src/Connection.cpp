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

#if defined(_WIN32) && defined(USESSL)
extern "C" {
#include <openssl/applink.c>
}
#endif

#define BLOCKSIZE 4096
#define SENDSIZE 4096

IMPL_DNSEVENTCLASS(CConnectionDnsEvents, CConnection, AsyncDnsFinished);
IMPL_DNSEVENTCLASS(CBindIpDnsEvents, CConnection, AsyncBindIpDnsFinished);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CConnection::CConnection(SOCKET Socket, bool SSL, connection_role_e Role) {
	SetRole(Role);

	InitConnection(Socket, SSL);
}

CConnection::CConnection(const char *Host, unsigned short Port, const char *BindIp, bool SSL) {
	in_addr ip;

	SetRole(Role_Client);

	InitConnection(INVALID_SOCKET, SSL);

#ifdef _WIN32
	ip.S_un.S_addr = inet_addr(Host);

	if (ip.S_un.S_addr != INADDR_NONE) {
#else
	ip.s_addr = inet_addr(Host);

	if (ip.s_addr != INADDR_NONE) {
#endif
		m_HostAddr = (in_addr *)malloc(sizeof(in_addr));

		*m_HostAddr = ip;
	}

	if (BindIp) {
#ifdef _WIN32
		ip.S_un.S_addr = inet_addr(BindIp);

		if (ip.S_un.S_addr != INADDR_NONE) {
#else
		ip.s_addr = inet_addr(BindIp);

		if (ip.s_addr != INADDR_NONE) {
#endif
			m_BindAddr = (in_addr *)malloc(sizeof(in_addr));

			*m_BindAddr = ip;
		}
	}

	m_Socket = INVALID_SOCKET;
	m_PortCache = Port;
	m_BindIpCache = BindIp ? strdup(BindIp) : NULL;

	if (m_HostAddr == NULL) {
		m_AdnsQuery = (adns_query*)malloc(sizeof(adns_query));
		m_DnsEvents = new CConnectionDnsEvents(this);
		adns_submit(g_adns_State, Host, adns_r_addr, (adns_queryflags)0, m_DnsEvents, m_AdnsQuery);
		m_AdnsTimeout = g_Bouncer->CreateTimer(3, true, IRCAdnsTimeoutTimer, this);
	} else {
		m_AdnsQuery = NULL;
		m_DnsEvents = NULL;
		m_AdnsTimeout = NULL;
	}

	if (m_BindIpCache && m_BindAddr == NULL) {
		m_BindAdnsQuery = (adns_query*)malloc(sizeof(adns_query));
		m_BindDnsEvents = new CBindIpDnsEvents(this);
		adns_submit(g_adns_State, BindIp, adns_r_addr, (adns_queryflags)0, m_BindDnsEvents, m_BindAdnsQuery);
	} else {
		m_BindAdnsQuery = NULL;
		m_BindDnsEvents = NULL;
	}

	// try to connect.. maybe we already have both addresses
	AsyncConnect();
}

void CConnection::InitConnection(SOCKET Client, bool SSL) {
	m_Socket = Client;

	m_Locked = false;
	m_Shutdown = false;
	m_Timeout = 0;

	m_Traffic = NULL;

	m_Wrapper = false;

	m_DnsEvents = NULL;
	m_BindDnsEvents = NULL;

	m_SendQ = new CFIFOBuffer();
	m_RecvQ = new CFIFOBuffer();

	m_AdnsQuery = NULL;
	m_BindAdnsQuery = NULL;
	m_AdnsTimeout = NULL;

	m_HostAddr = NULL;
	m_BindAddr = NULL;

	m_BindIpCache = NULL;
	m_PortCache = 0;

	if (m_SendQ == NULL || m_RecvQ == NULL) {
		LOGERROR("new operator failed. Sendq/recvq could not be created.");

		Kill("Internal error.");
	}

	m_LatchedDestruction = false;

#ifdef USESSL
	m_HasSSL = SSL;
	m_SSL = NULL;

	if (GetRole() == Role_Client && g_Bouncer->GetSSLContext() == NULL && SSL == true) {
		m_HasSSL = false;

		LOGERROR("No SSL server certificate available. Falling back to non-SSL mode.");
	}
#endif

	if (Client != INVALID_SOCKET) {
		InitSocket();
	}
}

CConnection::~CConnection() {
	delete m_SendQ;
	delete m_RecvQ;

	g_Bouncer->UnregisterSocket(m_Socket);

	if (m_AdnsQuery) {
		adns_cancel(*m_AdnsQuery);
	}

	if (m_BindAdnsQuery) {
		adns_cancel(*m_BindAdnsQuery);
	}

	if (m_AdnsTimeout) {
		m_AdnsTimeout->Destroy();
	}

	free(m_BindIpCache);

	if (m_DnsEvents) {
		m_DnsEvents->Destroy();
	}

	if (m_BindDnsEvents) {
		m_BindDnsEvents->Destroy();
	}

	if (m_Socket != INVALID_SOCKET) {
		shutdown(m_Socket, SD_BOTH);
		closesocket(m_Socket);
	}
	
	free(m_HostAddr);
	free(m_BindAddr);

#ifdef USESSL
	if (IsSSL() && m_SSL != NULL) {
		SSL_free(m_SSL);
	}
#endif
}

void CConnection::InitSocket(void) {
	if (m_Socket == INVALID_SOCKET) {
		return;
	}

#ifndef _WIN32
	const int optBuffer = 4 * SENDSIZE;
	const int optLowWat = SENDSIZE;
	const int optLinger = 0;

	setsockopt(m_Socket, SOL_SOCKET, SO_SNDBUF, &optBuffer, sizeof(optBuffer));
	setsockopt(m_Socket, SOL_SOCKET, SO_SNDLOWAT, &optLowWat, sizeof(optLowWat));
	setsockopt(m_Socket, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
#endif

#ifdef USESSL
	if (IsSSL()) {
		if (m_SSL) {
			SSL_free(m_SSL);
		}

		if (GetRole() == Role_Client) {
			m_SSL = SSL_new(g_Bouncer->GetSSLClientContext());
		} else {
			m_SSL = SSL_new(g_Bouncer->GetSSLContext());
		}

		SSL_set_fd(m_SSL, m_Socket);

		if (GetRole() == Role_Client) {
			SSL_set_connect_state(m_SSL);
		} else {
			SSL_set_accept_state(m_SSL);
		}

		SSL_set_ex_data(m_SSL, g_Bouncer->GetSSLCustomIndex(), this);
	} else {
		m_SSL = NULL;
	}
#endif

	g_Bouncer->RegisterSocket(m_Socket, (CSocketEvents *)this);
}

SOCKET CConnection::GetSocket(void) {
	return m_Socket;
}

bool CConnection::Read(bool DontProcess) {
	int ReadResult;
	char Buffer[8192];

	if (m_Shutdown) {
		return true;
	}

#ifdef USESSL
	if (IsSSL()) {
		ReadResult = SSL_read(m_SSL, Buffer, sizeof(Buffer));

		if (ReadResult < 0) {
			switch (SSL_get_error(m_SSL, ReadResult)) {
				case SSL_ERROR_WANT_WRITE:
				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_NONE:
				case SSL_ERROR_ZERO_RETURN:

					return true;
				default:
					return false;
			}
		}

		ERR_print_errors_fp(stdout);
	} else
#endif
		ReadResult = recv(m_Socket, Buffer, sizeof(Buffer), 0);

	if (ReadResult > 0) {
		m_RecvQ->Write(Buffer, ReadResult);

		if (m_Traffic) {
			m_Traffic->AddInbound(ReadResult);
		}
	} else {
#ifdef USESSL
		if (IsSSL()) {
			SSL_shutdown(m_SSL);
		}
#endif

		return false;
	}

	if (m_Wrapper) {
		return true;
	}

	if (DontProcess == false) {
		ProcessBuffer();
	}

	return true;
}

void CConnection::Write(void) {
	unsigned int Size;

	if (m_SendQ) {
		Size = m_SendQ->GetSize();
	} else {
		Size = 0;
	}

	if (Size > 0) {
		int WriteResult;

#ifdef USESSL
		if (IsSSL()) {
			WriteResult = SSL_write(m_SSL, m_SendQ->Peek(), Size > SENDSIZE ? SENDSIZE : Size);

			if (WriteResult == -1) {
				switch (SSL_get_error(m_SSL, WriteResult)) {
					case SSL_ERROR_WANT_WRITE:
					case SSL_ERROR_WANT_READ:
						return;
					default:
						break;
				}
			}
		} else {
#endif
			WriteResult = send(m_Socket, m_SendQ->Peek(), Size > SENDSIZE ? SENDSIZE : Size, 0);
#ifdef USESSL
		}
#endif

		if (WriteResult > 0) {
			if (m_Traffic) {
				m_Traffic->AddOutbound(WriteResult);
			}

			m_SendQ->Read(WriteResult);
		} else if (WriteResult < 0) {
			Shutdown();
		}
	}

	if (m_Shutdown) {
#ifdef USESSL
		if (IsSSL()) {
			SSL_shutdown(m_SSL);
		}
#endif

		if (m_Socket != INVALID_SOCKET) {
			shutdown(m_Socket, SD_BOTH);
			closesocket(m_Socket);
		}
	}
}

void CConnection::ProcessBuffer(void) {
	char *RecvQ;
	char *Line;
	unsigned int Size;
	
	if (m_RecvQ == NULL) {
		return;
	}

	Line = RecvQ = m_RecvQ->Peek();

	Size = m_RecvQ->GetSize();

	for (unsigned int i = 0; i < Size; i++) {
		if (RecvQ[i] == '\n' || RecvQ[i] == '\r') {
			RecvQ[i] = '\0';

			if (strlen(Line) > 0) {
				ParseLine(Line);
			}

			Line = &RecvQ[i + 1];
		}
	}

	m_RecvQ->Read(Line - RecvQ);
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

void CConnection::WriteUnformattedLine(const char *Line) {
	if (m_Locked == false && m_SendQ != NULL && Line != NULL) {
		m_SendQ->WriteUnformattedLine(Line);
	}
}

void CConnection::WriteLine(const char *Format, ...) {
	char* Line;
	va_list Marker;

	if (m_Shutdown) {
		return;
	}

	va_start(Marker, Format);
	vasprintf(&Line, Format, Marker);
	va_end(Marker);

	if (Line == NULL) {
		LOGERROR("vasprintf() failed. Could not write line (%s).", Format);

		return;
	}

	WriteUnformattedLine(Line);

	free(Line);
}

void CConnection::ParseLine(const char *Line) {
	// default implementation ignores any data
}

connection_role_e CConnection::GetRole(void) {
	return m_Role;
}

void CConnection::SetRole(connection_role_e Role) {
	m_Role = Role;
}

void CConnection::Kill(const char *Error) {
	if (m_Shutdown) {
		return;
	}

	SetTrafficStats(NULL);

	Shutdown();
	Timeout(10);
}

bool CConnection::HasQueuedData(void) {
#ifdef USESSL
	if (IsSSL() && SSL_want_write(m_SSL)) {
		return true;
	}
#endif

	if (m_SendQ) {
		return m_SendQ->GetSize() > 0;
	} else {
		return 0;
	}
}


int CConnection::GetSendqSize(void) {
	if (m_SendQ) {
		return m_SendQ->GetSize();
	} else {
		return 0;
	}
}

int CConnection::GetRecvqSize(void) {
	if (m_RecvQ) {
		return m_RecvQ->GetSize();
	} else {
		return 0;
	}
}

void CConnection::Error(void) {
}

void CConnection::Destroy(void) {
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
		Destroy();

		return true;
	} else {
		return false;
	}
}

void CConnection::SetTrafficStats(CTrafficStats *Stats) {
	m_Traffic = Stats;
}

CTrafficStats* CConnection::GetTrafficStats(void) {
	return m_Traffic;
}

const char* CConnection::GetClassName(void) {
	return "CConnection";
}

void CConnection::FlushSendQ(void) {
	if (m_SendQ) {
		m_SendQ->Flush();
	}
}

bool CConnection::IsSSL(void) {
#ifdef USESSL
	return m_HasSSL;
#else
	return false;
#endif
}

X509 *CConnection::GetPeerCertificate(void) {
#ifdef USESSL
	if (m_HasSSL/* && SSL_get_verify_result(m_SSL) == X509_V_OK*/) {
		return SSL_get_peer_certificate(m_SSL);
	}
#endif

	return NULL;
}

int CConnection::SSLVerify(int PreVerifyOk, X509_STORE_CTX *Context) {
	return 1;
}

bool IRCAdnsTimeoutTimer(time_t Now, void *IRC) {
	((CConnection *)IRC)->m_AdnsTimeout = NULL;

	((CConnection *)IRC)->AdnsTimeout();

	return false;
}

void CConnection::AsyncConnect(void) {
	if (m_HostAddr != NULL && (m_BindAddr != NULL || m_BindIpCache == NULL)) {
		m_Socket = SocketAndConnectResolved(*m_HostAddr, m_PortCache, m_BindAddr);

		InitSocket();
	}
}

void CConnection::AsyncDnsFinished(adns_query *Query, adns_answer *Response) {
	free(m_AdnsQuery);
	m_AdnsQuery = NULL;
	m_DnsEvents = NULL;

	if (Response == NULL || Response->status != adns_s_ok) {
		// we cannot destroy the object here as there might still be the other
		// adns query (bind ip) in the queue which would get destroyed in the
		// destructor; this causes a crash in the StartMainLoop() function
		m_LatchedDestruction = true;
	} else {
		m_HostAddr = (in_addr *)malloc(sizeof(in_addr));
		*m_HostAddr = Response->rrs.addr->addr.inet.sin_addr;

		m_AdnsTimeout->Destroy();
		m_AdnsTimeout = NULL;

		AsyncConnect();
	}
}

void CConnection::AsyncBindIpDnsFinished(adns_query *Query, adns_answer *Response) {
	free(m_BindAdnsQuery);
	m_BindAdnsQuery = NULL;
	m_BindDnsEvents = NULL;

	if (Response != NULL && Response->status == adns_s_ok) {
		m_BindAddr = (in_addr *)malloc(sizeof(in_addr));
		*m_BindAddr = Response->rrs.addr->addr.inet.sin_addr;
	}

	free(m_BindIpCache);
	m_BindIpCache = NULL;

	AsyncConnect();
}

void CConnection::AdnsTimeout(void) {
	m_LatchedDestruction = true;

	if (m_AdnsQuery != NULL) {
		adns_cancel(*m_AdnsQuery);

		free(m_AdnsQuery);
		m_AdnsQuery = NULL;
	}

	if (m_BindAdnsQuery != NULL) {
		adns_cancel(*m_BindAdnsQuery);

		free(m_BindAdnsQuery);
		m_BindAdnsQuery = NULL;
	}
}

bool CConnection::ShouldDestroy(void) {
	return m_LatchedDestruction;
}

sockaddr_in CConnection::GetRemoteAddress(void) {
	sockaddr_in Result;
	int ResultLength = sizeof(Result);

	getpeername(m_Socket, (sockaddr *)&Result, &ResultLength);

	return Result;
}

sockaddr_in CConnection::GetLocalAddress(void) {
	sockaddr_in Result;
	int ResultLength = sizeof(Result);

	getsockname(m_Socket, (sockaddr *)&Result, &ResultLength);

	return Result;
}
