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

#define BLOCKSIZE 4096

IMPL_DNSEVENTPROXY(CConnection, AsyncDnsFinished);
IMPL_DNSEVENTPROXY(CConnection, AsyncBindIpDnsFinished);

/**
 * CConnection
 *
 * Constructs a new connection object.
 *
 * @param Socket the underlying socket
 * @param SSL whether to use SSL
 * @param Role the role of the connection
 */
CConnection::CConnection(SOCKET Socket, bool SSL, connection_role_e Role) {
	char Address[MAX_SOCKADDR_LEN];
	socklen_t AddressLength = sizeof(Address);

	SetRole(Role);

	if (Socket != INVALID_SOCKET) {
		getsockname(Socket, (sockaddr *)Address, &AddressLength);
		m_Family = ((sockaddr *)Address)->sa_family;
	} else {
		m_Family = AF_INET;
	}

	InitConnection(Socket, SSL);
}

/**
 * CConnection
 *
 * Constructs a new connection object.
 *
 * @param Host the remote host
 * @param Port the port
 * @param BindIp the local bind address
 * @param SSL whether to use SSL for the connection
 * @param Family the socket family
 */
CConnection::CConnection(const char *Host, unsigned short Port, const char *BindIp, bool SSL, int Family) {
	m_Family = Family;

	SetRole(Role_Client);

	InitConnection(INVALID_SOCKET, SSL);

	m_Socket = INVALID_SOCKET;
	m_PortCache = Port;
	m_BindIpCache = BindIp ? strdup(BindIp) : NULL;

	m_DnsQuery = new CDnsQuery(this, USE_DNSEVENTPROXY(CConnection, AsyncDnsFinished));

	m_DnsQuery->GetHostByName(Host, Family);

	if (m_BindIpCache != NULL) {
		m_BindDnsQuery = new CDnsQuery(this, USE_DNSEVENTPROXY(CConnection, AsyncBindIpDnsFinished));

		m_BindDnsQuery->GetHostByName(BindIp, Family);
	} else {
		m_BindDnsQuery = NULL;
	}

	// try to connect.. maybe we already have both addresses
	AsyncConnect();
}

/**
 * InitConnection
 *
 * Initializes a new connection object.
 *
 * @param Client the socket
 * @param SSL whether to use SSL
 */
void CConnection::InitConnection(SOCKET Client, bool SSL) {
	m_Socket = Client;

	m_Locked = false;
	m_Shutdown = false;
	m_Timeout = 0;

	m_Traffic = NULL;

	m_Wrapper = false;

	m_SendQ = new CFIFOBuffer();
	m_RecvQ = new CFIFOBuffer();

	m_DnsQuery = NULL;
	m_BindDnsQuery = NULL;

	m_HostAddr = NULL;
	m_BindAddr = NULL;

	m_BindIpCache = NULL;
	m_PortCache = 0;

	if (m_SendQ == NULL || m_RecvQ == NULL) {
		LOGERROR("new operator failed. Sendq/recvq could not be created.");

		Kill("Internal error.");
	}

	m_LatchedDestruction = false;
	m_Connected = false;

	m_InboundTrafficReset = g_CurrentTime;
	m_InboundTraffic = 0;

#ifdef USESSL
	m_HasSSL = SSL;
	m_SSL = NULL;

	if (GetRole() == Role_Server && g_Bouncer->GetSSLContext() == NULL && SSL) {
		m_HasSSL = false;

		LOGERROR("No SSL server certificate available. Falling back to"
			" non-SSL mode. This might not work.");
	}
#endif

	if (Client != INVALID_SOCKET) {
		InitSocket();
	}
}

/**
 * ~CConnection
 *
 * Destructs the connection object.
 */
CConnection::~CConnection(void) {
	delete m_SendQ;
	delete m_RecvQ;

	g_Bouncer->UnregisterSocket(m_Socket);

	delete m_DnsQuery;
	delete m_BindDnsQuery;

	free(m_BindIpCache);

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

/**
 * InitSocket
 *
 * Initializes the socket.
 */
void CConnection::InitSocket(void) {
	if (m_Socket == INVALID_SOCKET) {
		return;
	}

#ifndef _WIN32
	const int optLinger = 0;

	setsockopt(m_Socket, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
#endif

#ifdef USESSL
	if (IsSSL()) {
		if (m_SSL != NULL) {
			SSL_free(m_SSL);
		}

		if (GetRole() == Role_Client) {
			m_SSL = SSL_new(g_Bouncer->GetSSLClientContext());
		} else {
			m_SSL = SSL_new(g_Bouncer->GetSSLContext());
		}

		if (m_SSL != NULL) {
			SSL_set_fd(m_SSL, m_Socket);

			if (GetRole() == Role_Client) {
				SSL_set_connect_state(m_SSL);
			} else {
				SSL_set_accept_state(m_SSL);
			}

			SSL_set_ex_data(m_SSL, g_Bouncer->GetSSLCustomIndex(), this);
		}
	} else {
		m_SSL = NULL;
	}
#endif

	g_Bouncer->RegisterSocket(m_Socket, (CSocketEvents *)this);
}

/**
 * SetSocket
 *
 * Sets the socket for the connection object.
 *
 * @param Socket the new socket
 */
void CConnection::SetSocket(SOCKET Socket) {
	m_Socket = Socket;

	if (Socket != INVALID_SOCKET) {
		InitSocket();
	}
}

/**
 * GetSocket
 *
 * Returns the socket for the connection object.
 */
SOCKET CConnection::GetSocket(void) const {
	return m_Socket;
}

/**
 * Read
 *
 * Called when data is available on the socket.
 *
 * @param DontProcess determines whether to process the data
 */
bool CConnection::Read(bool DontProcess) {
	int ReadResult;
	static char *Buffer;
	static int BufferSize = 0;
	int NewBufferSize;
	socklen_t OptLenght = sizeof(BufferSize);

	m_Connected = true;

	if (m_Shutdown) {
		return true;
	}

	if (getsockopt(m_Socket, SOL_SOCKET, SO_RCVBUF, (char *)&NewBufferSize, &OptLenght) != 0) {
		NewBufferSize = 8192;
	}

	if (NewBufferSize > BufferSize) {
		BufferSize = NewBufferSize;
		Buffer = (char *)realloc(Buffer, BufferSize);
	}

	CHECK_ALLOC_RESULT(Buffer, malloc) {
		return false;
	} CHECK_ALLOC_RESULT_END;

#ifdef USESSL
	if (IsSSL()) {
		ReadResult = SSL_read(m_SSL, Buffer, BufferSize);

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
		ReadResult = recv(m_Socket, Buffer, BufferSize, 0);

	if (ReadResult > 0) {
		if (g_CurrentTime - m_InboundTrafficReset > 30) {
			m_InboundTrafficReset = g_CurrentTime;
			m_InboundTraffic = 0;
		}

		m_InboundTraffic += ReadResult;

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

	if (!DontProcess) {
		ProcessBuffer();
	}

	return true;
}

/**
 * Write
 *
 * Called when data can be written for the socket.
 */
void CConnection::Write(void) {
	size_t Size;

	if (m_SendQ) {
		Size = m_SendQ->GetSize();
	} else {
		Size = 0;
	}

	if (Size > 0) {
		int WriteResult;

#ifdef USESSL
		if (IsSSL()) {
			WriteResult = SSL_write(m_SSL, m_SendQ->Peek(), Size);

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
			WriteResult = send(m_Socket, m_SendQ->Peek(), Size, 0);
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

/**
 * ProcessBuffer
 *
 * Processes the data which is in the recvq.
 */
void CConnection::ProcessBuffer(void) {
	char *RecvQ, *Line;
	size_t Size;
	
	if (m_RecvQ == NULL) {
		return;
	}

	Line = RecvQ = m_RecvQ->Peek();

	Size = m_RecvQ->GetSize();

	for (unsigned int i = 0; i < Size; i++) {
		if (RecvQ[i] == '\n' || (RecvQ[i] == '\r' && Size > i + 1 && RecvQ[i + 1] == '\n')) {
			RecvQ[i] = '\0';

			if (Line[0] != '\0') {
				ParseLine(Line);
			}

			if (RecvQ[i] == '\r') {
				i++;
			}

			Line = &RecvQ[i + 1];
		}
	}

	m_RecvQ->Read(Line - RecvQ);
}

/**
 * ReadLine
 *
 * Reads a line from the recvq and places a pointer to the line in "Out". This
 * function is rather inefficient. Use ReadLines() instead.
 *
 * @param Out points to the line
 */
bool CConnection::ReadLine(char **Out) {
	char *old_recvq;
	size_t Size;
	char *Pos = NULL;
	bool advance = false;

	if (m_RecvQ == NULL) {
		*Out = NULL;
		return false;
	}

	old_recvq = m_RecvQ->Peek();

	if (old_recvq == NULL) {
		return false;
	}

	Size = m_RecvQ->GetSize();

	for (unsigned int i = 0; i < Size; i++) {
		if (old_recvq[i] == '\n' || (Size > i + 1 && old_recvq[i] == '\r' && old_recvq[i + 1] == '\n')) {
			if (old_recvq[i] == '\r') {
				advance = true;
			}

			Pos = old_recvq + i;
			break;
		}
	}

	if (Pos != NULL) {
		size_t Size;
		*Pos = '\0';
		char *NewPtr = Pos + 1 + (advance ? 1 : 0);

		Size = NewPtr - old_recvq + 1;
		*Out = (char *)g_Bouncer->GetUtilities()->Alloc(Size);
		strmcpy(*Out, m_RecvQ->Read(NewPtr - old_recvq), Size);

		CHECK_ALLOC_RESULT(*Out, strdup) {
			return false;
		} CHECK_ALLOC_RESULT_END;

		return true;
	} else {
		*Out = NULL;
		return false;
	}
}

/**
 * WriteUnformattedLine
 *
 * Writes a line for the connection.
 *
 * @param Line the line
 */
void CConnection::WriteUnformattedLine(const char *Line) {
	if (!m_Locked && m_SendQ != NULL && Line != NULL) {
		m_SendQ->WriteUnformattedLine(Line);
	}
}

/**
 * WriteLine
 *
 * Writes a line for the connection.
 *
 * @param Format the format string
 * @param ... additional parameters used in the format string
 */
void CConnection::WriteLine(const char *Format, ...) {
	char *Line;
	va_list Marker;

	if (m_Shutdown) {
		return;
	}

	va_start(Marker, Format);
	vasprintf(&Line, Format, Marker);
	va_end(Marker);

	CHECK_ALLOC_RESULT(Line, vasprintf) {
		return;
	} CHECK_ALLOC_RESULT_END;

	WriteUnformattedLine(Line);

	free(Line);
}

/**
 * ParseLine
 *
 * Processes a line.
 *
 * @param Line the line
 */
void CConnection::ParseLine(const char *Line) {
	// default implementation ignores any data
}

/**
 * GetRole
 *
 * Returns the role of the connection.
 */
connection_role_e CConnection::GetRole(void) const {
	return m_Role;
}

/**
 * SetRole
 *
 * Sets the role of the connection.
 *
 * @param Role the role of the connection
 */
void CConnection::SetRole(connection_role_e Role) {
	m_Role = Role;
}

/**
 * Kill
 *
 * "Kills" the connection.
 *
 * @param Error the reason
 */
void CConnection::Kill(const char *Error) {
	if (m_Shutdown) {
		return;
	}

	SetTrafficStats(NULL);

	Shutdown();
	Timeout(10);
}

/**
 * HasQueuedData
 *
 * Checks whether the connection object has data which can be
 * written to the socket.
 */
bool CConnection::HasQueuedData(void) const {
#ifdef USESSL
	if (IsSSL() && SSL_want_write(m_SSL)) {
		return true;
	}
#endif

	if (m_SendQ != NULL) {
		return m_SendQ->GetSize() > 0;
	} else {
		return 0;
	}
}

/**
 * GetSendqSize
 *
 * Returns the size of the sendq.
 */
size_t CConnection::GetSendqSize(void) const {
	if (m_SendQ) {
		return m_SendQ->GetSize();
	} else {
		return 0;
	}
}

/**
 * GetRecvqSize
 *
 * Returns the size of the recvq.
 */
size_t CConnection::GetRecvqSize(void) const {
	if (m_RecvQ != NULL) {
		return m_RecvQ->GetSize();
	} else {
		return 0;
	}
}

/**
 * Error
 *
 * Called when an error occured for the socket.
 */
void CConnection::Error(void) {
}

/**
 * Destroy
 *
 * Destroys the connection object.
 */
void CConnection::Destroy(void) {
	delete this;
}

/**
 * Lock
 *
 * Locks the connection object.
 */
void CConnection::Lock(void) {
	m_Locked = true;
}

/**
 * IsLocked
 *
 * Checks whether the connection object is locked.
 */
bool CConnection::IsLocked(void) const {
	return m_Locked;
}

/**
 * Shutdown
 *
 * Shuts down the connection object.
 */
void CConnection::Shutdown(void) {
	m_Shutdown = true;
}

/**
 * Timeout
 *
 * Sets the timeout TS for the connection.
 *
 * @param TimeLeft the time
 */
void CConnection::Timeout(int TimeLeft) {
	m_Timeout = g_CurrentTime + TimeLeft;
}

/**
 * DoTimeout
 *
 * Checks whether a timeout occured.
 */
bool CConnection::DoTimeout(void) {
	if (m_Timeout > 0 && m_Timeout < g_CurrentTime) {
		Destroy();

		return true;
	} else {
		return false;
	}
}

/**
 * SetTrafficStats
 *
 * Sets the traffic stats object for the connection.
 *
 * @param Stats the new stats object
 */
void CConnection::SetTrafficStats(CTrafficStats *Stats) {
	m_Traffic = Stats;
}

/**
 * GetTrafficStats
 *
 * Returns the traffic stats object for the connection.
 */
const CTrafficStats *CConnection::GetTrafficStats(void) const {
	return m_Traffic;
}

/**
 * GetClassName
 *
 * Returns the class' name.
 */
const char *CConnection::GetClassName(void) const {
	return "CConnection";
}

/**
 * FlushSendQ
 *
 * Flushes the sendq.
 */
void CConnection::FlushSendQ(void) {
	if (m_SendQ != NULL) {
		m_SendQ->Flush();
	}
}

/**
 * IsSSL
 *
 * Returns whether SSL is enabled for the connection.
 */
bool CConnection::IsSSL(void) const {
#ifdef USESSL
	return m_HasSSL;
#else
	return false;
#endif
}

/**
 * GetPeerCertificate
 *
 * Gets the remote side's certificate.
 */
const X509 *CConnection::GetPeerCertificate(void) const {
#ifdef USESSL
	if (IsSSL()) {
		return SSL_get_peer_certificate(m_SSL);
	}
#endif

	return NULL;
}

/**
 * SSLVerify
 *
 * Used for verifying the peer's client certificate.
 */
int CConnection::SSLVerify(int PreVerifyOk, X509_STORE_CTX *Context) const {
	return 1;
}

/**
 * AsyncConnect
 *
 * Tries to establish a connection.
 */
void CConnection::AsyncConnect(void) {
	if (m_HostAddr != NULL && (m_BindAddr != NULL || m_BindIpCache == NULL)) {
		sockaddr *Remote, *Bind = NULL;

		if (m_Family == AF_INET) {
			sockaddr_in RemoteV4, BindV4;

			memset(&RemoteV4, 0, sizeof(RemoteV4));
			RemoteV4.sin_family = m_Family;
			RemoteV4.sin_port = htons(m_PortCache);
			RemoteV4.sin_addr.s_addr = ((in_addr *)m_HostAddr)->s_addr;

			Remote = (sockaddr *)&RemoteV4;

			if (m_BindAddr != NULL) {
				memset(&BindV4, 0, sizeof(BindV4));
				BindV4.sin_family = m_Family;
				BindV4.sin_port = 0;
				BindV4.sin_addr.s_addr = ((in_addr *)m_BindAddr)->s_addr;

				Bind = (sockaddr *)&BindV4;
			}
#ifdef IPV6
		} else {
			sockaddr_in6 RemoteV6, BindV6;

			memset(&RemoteV6, 0, sizeof(RemoteV6));
			RemoteV6.sin6_family = m_Family;
			RemoteV6.sin6_port = htons(m_PortCache);
			memcpy(&(RemoteV6.sin6_addr), m_HostAddr, sizeof(in6_addr));

			Remote = (sockaddr *)&RemoteV6;

			if (m_BindAddr != NULL) {
				memset(&BindV6, 0, sizeof(BindV6));
				BindV6.sin6_family = m_Family;
				BindV6.sin6_port = 0;
				memcpy(&(BindV6.sin6_addr), m_BindAddr, sizeof(in6_addr));

				Bind = (sockaddr *)&BindV6;
			}
#endif
		}

		m_Socket = SocketAndConnectResolved(Remote, Bind);

		free(m_HostAddr);
		m_HostAddr = NULL;

		InitSocket();
	}
}

/**
 * AsyncDnsFinished
 *
 * Called when the DNS query for the remote host is finished.
 *
 * @param Response the response
 */
void CConnection::AsyncDnsFinished(hostent *Response) {
	if (Response == NULL) {
		// we cannot destroy the object here as there might still be the other
		// dns query (bind ip) in the queue which would get destroyed in the
		// destructor; this causes a crash in the StartMainLoop() function
		m_LatchedDestruction = true;
	} else {
		int Size;

		if (Response->h_addrtype == AF_INET) {
			Size = sizeof(in_addr);
		} else {
			Size = sizeof(in6_addr);
		}

		m_HostAddr = (in_addr *)malloc(Size);

		CHECK_ALLOC_RESULT(m_HostAddr, malloc) {
			m_LatchedDestruction = true;

			return;
		} CHECK_ALLOC_RESULT_END;

		memcpy(m_HostAddr, Response->h_addr_list[0], Size);

		AsyncConnect();
	}
}

/**
 * AsyncBindIpDnsFinished
 *
 * Called when the DNS query for the local bind address is finished.
 *
 * @param Response the response
 */

void CConnection::AsyncBindIpDnsFinished(hostent *Response) {
	if (Response != NULL) {
		int Size;

		if (Response->h_addrtype == AF_INET) {
			Size = sizeof(in_addr);
		} else {
			Size = sizeof(in6_addr);
		}

		m_BindAddr = (in_addr *)malloc(Size);
		memcpy(m_BindAddr, Response->h_addr_list[0], Size);
	}

	free(m_BindIpCache);
	m_BindIpCache = NULL;

	AsyncConnect();
}

/**
 * ShouldDestroy
 *
 * Called to determine whether this connection object is to be destroyed.
 */
bool CConnection::ShouldDestroy(void) const {
	return m_LatchedDestruction;
}

/**
 * GetRemoteAddress
 *
 * Returns the remote side's address.
 */
sockaddr *CConnection::GetRemoteAddress(void) const {
	static char Result[MAX_SOCKADDR_LEN];
	socklen_t ResultLength = sizeof(Result);

	if (m_Socket != INVALID_SOCKET && getpeername(m_Socket, (sockaddr *)Result, &ResultLength) == 0) {
		return (sockaddr *)Result;
	} else {
		return NULL;
	}
}

/**
 * GetLocalAddress
 *
 * Returns the local side's address.
 */
sockaddr *CConnection::GetLocalAddress(void) const {
	static char Result[MAX_SOCKADDR_LEN];
	socklen_t ResultLength = sizeof(Result);

	if (m_Socket != INVALID_SOCKET && getsockname(m_Socket, (sockaddr *)Result, &ResultLength) == 0) {
		return (sockaddr *)Result;
	} else {
		return NULL;
	}
}

/**
 * IsConnected
 *
 * Checks whether the connection object is connected.
 */
bool CConnection::IsConnected(void) {
	return m_Connected;
}

/**
 * GetInboundRate
 *
 * Gets the current inbound rate (in bytes per second).
 */
size_t CConnection::GetInboundRate(void) {
	if (g_CurrentTime - m_InboundTrafficReset > 0) {
		return m_InboundTraffic / (g_CurrentTime - m_InboundTrafficReset);
	} else {
		return 0;
	}
}
