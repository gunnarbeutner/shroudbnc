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

#ifdef USESSL
#include <openssl/err.h>
#endif

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

CConnection::CConnection(SOCKET Client, bool SSL) {
	InitConnection(Client, SSL);
}

CConnection::CConnection(const char* Host, unsigned short Port, const char* BindIp, bool SSL) {
	in_addr ip;

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
	m_Owner = NULL;

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
#endif

	if (Client != INVALID_SOCKET)
		InitSocket();
}

CConnection::~CConnection() {
	delete m_SendQ;
	delete m_RecvQ;

	g_Bouncer->UnregisterSocket(m_Socket);

	if (m_AdnsQuery)
		adns_cancel(*m_AdnsQuery);

	if (m_BindAdnsQuery)
		adns_cancel(*m_BindAdnsQuery);

	if (m_AdnsTimeout)
		m_AdnsTimeout->Destroy();

	if (m_BindIpCache)
		free(m_BindIpCache);

	if (m_DnsEvents)
		m_DnsEvents->Destroy();

	if (m_BindDnsEvents)
		m_BindDnsEvents->Destroy();

	if (m_Socket != INVALID_SOCKET)
		closesocket(m_Socket);
	
	free(m_HostAddr);
	free(m_BindAddr);

#ifdef USESSL
	if (IsSSL() && m_SSL)
		SSL_free(m_SSL);
#endif
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
	if (IsSSL()) {
		if (m_SSL)
			SSL_free(m_SSL);

		if (GetRole() == Role_IRC)
			m_SSL = SSL_new(g_Bouncer->GetSSLClientContext());
		else
			m_SSL = SSL_new(g_Bouncer->GetSSLContext());

		SSL_set_fd(m_SSL, m_Socket);

		if (GetRole() == Role_IRC)
			SSL_set_connect_state(m_SSL);
		else
			SSL_set_accept_state(m_SSL);

		SSL_set_ex_data(m_SSL, g_Bouncer->GetSSLCustomIndex(), this);
	} else
		m_SSL = NULL;
#endif

	g_Bouncer->RegisterSocket(m_Socket, (CSocketEvents*)this);
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
	if (IsSSL()) {
		n = SSL_read(m_SSL, Buffer, sizeof(Buffer));

		if (n < 0) {
			switch (SSL_get_error(m_SSL, n)) {
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
		n = recv(m_Socket, Buffer, sizeof(Buffer), 0);

	if (n > 0) {
		m_RecvQ->Write(Buffer, n);

		if (m_Traffic)
			m_Traffic->AddInbound(n);
	} else {
#ifdef USESSL
		if (IsSSL())
			SSL_shutdown(m_SSL);
#endif

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
		if (IsSSL()) {
			n = SSL_write(m_SSL, m_SendQ->Peek(), Size > SENDSIZE ? SENDSIZE : Size);

			if (n == -1) {
				switch (SSL_get_error(m_SSL, n)) {
					case SSL_ERROR_WANT_WRITE:
					case SSL_ERROR_WANT_READ:
						return;
					default:
						break;
				}
			}
		} else
#endif
			n = send(m_Socket, m_SendQ->Peek(), Size > SENDSIZE ? SENDSIZE : Size, 0);

		if (n > 0) {
			if (m_Traffic)
				m_Traffic->AddOutbound(n);

			m_SendQ->Read(n);
		} else if (n < 0)
			Shutdown();
	}

	if (m_Shutdown) {
#ifdef USESSL
		if (IsSSL())
			SSL_shutdown(m_SSL);
#endif

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

	if (m_SendQ && In)
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
#ifdef USESSL
	if (IsSSL() && SSL_want_write(m_SSL))
		return true;
#endif

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

X509* CConnection::GetPeerCertificate(void) {
#ifdef USESSL
	if (m_HasSSL/* && SSL_get_verify_result(m_SSL) == X509_V_OK*/) {
		return SSL_get_peer_certificate(m_SSL);
	}
#endif

	return NULL;
}

int CConnection::SSLVerify(int PreVerifyOk, X509_STORE_CTX* Context) {
	return 1;
}

bool IRCAdnsTimeoutTimer(time_t Now, void* IRC) {
	((CConnection*)IRC)->m_AdnsTimeout = NULL;

	((CConnection*)IRC)->AdnsTimeout();

	return false;
}

void CConnection::AsyncConnect(void) {
	if (m_HostAddr && (m_BindAddr || m_BindIpCache == NULL)) {
		m_Socket = SocketAndConnectResolved(*m_HostAddr, m_PortCache, m_BindAddr);

		InitSocket();
	}
}

void CConnection::AsyncDnsFinished(adns_query* query, adns_answer* response) {
	free(m_AdnsQuery);
	m_AdnsQuery = NULL;
	m_DnsEvents = NULL;

	if (response->status != adns_s_ok) {
		Destroy();

		return;
	} else {
		m_HostAddr = (in_addr *)malloc(sizeof(in_addr));
		*m_HostAddr = response->rrs.addr->addr.inet.sin_addr;

		m_AdnsTimeout->Destroy();
		m_AdnsTimeout = NULL;

		AsyncConnect();
	}
}

void CConnection::AsyncBindIpDnsFinished(adns_query *query, adns_answer *response) {
	free(m_BindAdnsQuery);
	m_BindAdnsQuery = NULL;
	m_BindDnsEvents = NULL;

	if (response->status == adns_s_ok) {
		m_BindAddr = (in_addr *)malloc(sizeof(in_addr));
		*m_BindAddr = response->rrs.addr->addr.inet.sin_addr;
	}

	free(m_BindIpCache);
	m_BindIpCache = NULL;

	AsyncConnect();
}

void CConnection::AdnsTimeout(void) {
	m_LatchedDestruction = true;

	if (m_AdnsQuery) {
		adns_cancel(*m_AdnsQuery);

		free(m_AdnsQuery);
		m_AdnsQuery = NULL;
	}

	if (m_BindAdnsQuery) {
		adns_cancel(*m_BindAdnsQuery);

		free(m_BindAdnsQuery);
		m_BindAdnsQuery = NULL;
	}
}

bool CConnection::ShouldDestroy(void) {
	return m_LatchedDestruction;
}
