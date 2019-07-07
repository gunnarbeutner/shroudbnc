/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2014 Gunnar Beutner                                      *
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
	sockaddr_storage Address;
	socklen_t AddressLength = sizeof(Address);

	SetRole(Role);

	if (Socket != INVALID_SOCKET) {
		getsockname(Socket, (sockaddr *)&Address, &AddressLength);
		m_Family = Address.ss_family;
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
 * @param Family the address family
 */
CConnection::CConnection(const char *Host, unsigned int Port, const char *BindIp, bool SSL, int Family) {
	m_Family = AF_INET;

	SetRole(Role_Client);

	InitConnection(INVALID_SOCKET, SSL);

	m_Socket = INVALID_SOCKET;
	m_PortCache = Port;
	m_BindIpCache = BindIp ? strdup(BindIp) : NULL;
	m_BindDnsQuery = NULL;

	if (Host != NULL) {
		m_DnsQuery = new CDnsQuery(this, USE_DNSEVENTPROXY(CConnection, AsyncDnsFinished));
		m_DnsQuery->GetHostByName(Host, Family);

		// try to connect.. maybe we already have both addresses
		AsyncConnect();
	}
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

	m_Shutdown = false;
	m_Timeout = 0;

	m_Traffic = NULL;

	m_DnsQuery = NULL;
	m_BindDnsQuery = NULL;

	m_HostAddr = NULL;
	m_BindAddr = NULL;

	m_BindIpCache = NULL;
	m_PortCache = 0;

	m_LatchedDestruction = false;
	m_Connected = false;

	m_InboundTrafficReset = g_CurrentTime;
	m_InboundTraffic = 0;

#ifdef HAVE_LIBSSL
	m_HasSSL = SSL;
	m_SSL = NULL;

	if (GetRole() == Role_Server && g_Bouncer->GetSSLContext() == NULL && SSL) {
		m_HasSSL = false;

		g_Bouncer->Log("No SSL server certificate available. Falling back to"
			" non-SSL mode. This might not work.");
	}
#endif

	if (Client != INVALID_SOCKET) {
		InitSocket();
	}

	m_SendQ = new CFIFOBuffer();
	m_RecvQ = new CFIFOBuffer();
}

/**
 * ~CConnection
 *
 * Destructs the connection object.
 */
CConnection::~CConnection(void) {
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

	delete m_SendQ;
	delete m_RecvQ;

#ifdef HAVE_LIBSSL
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

#ifdef HAVE_LIBSSL
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
			BIO *Bio = BIO_new_socket(m_Socket, 0);
			SSL_set_bio(m_SSL, Bio, Bio);
			//SSL_set_fd(m_SSL, m_Socket);

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
int CConnection::Read(bool DontProcess) {
	int ReadResult;
	static char *Buffer = NULL;
	const int BufferSize = 8192;

	m_Connected = true;

	if (m_Shutdown) {
		return 0;
	}

	if (Buffer == NULL) {
		Buffer = (char *)malloc(BufferSize);
	}

	if (AllocFailed(Buffer)) {
		return -1;
	}

#ifdef HAVE_LIBSSL
	if (IsSSL()) {
		ReadResult = SSL_read(m_SSL, Buffer, BufferSize);

		if (ReadResult < 0) {
			switch (SSL_get_error(m_SSL, ReadResult)) {
				case SSL_ERROR_WANT_WRITE:
				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_NONE:
				case SSL_ERROR_ZERO_RETURN:

					return 0;
				default:
					return -1;
			}
		}

		ERR_print_errors_fp(stdout);
	} else {
#endif
		ReadResult = recv(m_Socket, Buffer, BufferSize, 0);
#ifdef HAVE_LIBSSL
	}
#endif

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
		int ErrorCode;

		if (ReadResult == 0) {
			return -1;
		}

		ErrorCode = errno;

#ifdef _WIN32
		if (ErrorCode == WSAEWOULDBLOCK) {
			return 0;
		}
#else
		if (ErrorCode == EAGAIN) {
			return 0;
		}
#endif

#ifdef HAVE_LIBSSL
		if (IsSSL()) {
			SSL_shutdown(m_SSL);
		}
#endif

		return ErrorCode;
	}

	if (!DontProcess) {
		ProcessBuffer();
	}

	return 0;
}

/**
 * Write
 *
 * Called when data can be written for the socket.
 */
int CConnection::Write(void) {
	size_t Size;
	int ReturnValue = 0;

	Size = m_SendQ->GetSize();

	if (Size > 0) {
		int WriteResult;

#ifdef HAVE_LIBSSL
		if (IsSSL()) {
			WriteResult = SSL_write(m_SSL, m_SendQ->Peek(), Size);

			if (WriteResult == -1) {
				switch (SSL_get_error(m_SSL, WriteResult)) {
					case SSL_ERROR_WANT_WRITE:
					case SSL_ERROR_WANT_READ:
						return 0;
					default:
						break;
				}
			}
		} else {
#endif
			WriteResult = send(m_Socket, m_SendQ->Peek(), Size, 0);
#ifdef HAVE_LIBSSL
		}
#endif

		ReturnValue = errno;

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
#ifdef HAVE_LIBSSL
		if (IsSSL()) {
			SSL_shutdown(m_SSL);
		}
#endif

		if (m_Socket != INVALID_SOCKET) {
			shutdown(m_Socket, SD_BOTH);
			closesocket(m_Socket);
		}
	}

	return ReturnValue;
}

/**
 * ProcessBuffer
 *
 * Processes the data which is in the recvq.
 */
void CConnection::ProcessBuffer(void) {
	char *RecvQ, *Line;
	size_t Size;
	
	Line = RecvQ = m_RecvQ->Peek();

	Size = m_RecvQ->GetSize();

	for (unsigned int i = 0; i < Size; i++) {
		if (RecvQ[i] == '\n' || (RecvQ[i] == '\r' && Size > i + 1 && RecvQ[i + 1] == '\n')) {
			char *dupLine = (char *)malloc(&(RecvQ[i]) - Line + 1);

			if (AllocFailed(dupLine)) {
				return;
			}

			memcpy(dupLine, Line, &(RecvQ[i]) - Line);
			dupLine[&(RecvQ[i]) - Line] = '\0';

			if (dupLine[0] != '\0') {
 				ParseLine(dupLine);
			}

			free(dupLine);

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
		*Out = (char *)malloc(Size);

		if (AllocFailed(*Out)) {
			return false;
		}

		strmcpy(*Out, m_RecvQ->Read(NewPtr - old_recvq), Size);

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
	m_SendQ->WriteUnformattedLine(Line);
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
	int rc = vasprintf(&Line, Format, Marker);
	va_end(Marker);

	if (RcFailed(rc)) {
		return;
	}

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
#ifdef HAVE_LIBSSL
	if (IsSSL()) {
		if (SSL_want_write(m_SSL)) {
			return true;
		}

		if (SSL_get_state(m_SSL) != TLS_ST_OK) {
			return false;
		}
	}
#endif

	return m_SendQ->GetSize() > 0;
}

/**
 * GetSendqSize
 *
 * Returns the size of the sendq.
 */
size_t CConnection::GetSendqSize(void) const {
	return m_SendQ->GetSize();
}

/**
 * GetRecvqSize
 *
 * Returns the size of the recvq.
 */
size_t CConnection::GetRecvqSize(void) const {
	return m_RecvQ->GetSize();
}

/**
 * Error
 *
 * Called when an error occured for the socket.
 *
 * @param ErrorCode the error code
 */
void CConnection::Error(int ErrorCode) {
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
	m_SendQ->Flush();
}

/**
 * IsSSL
 *
 * Returns whether SSL is enabled for the connection.
 */
bool CConnection::IsSSL(void) const {
#ifdef HAVE_LIBSSL
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
#ifdef HAVE_LIBSSL
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
		sockaddr *Remote = NULL, *Bind = NULL;
		sockaddr_in RemoteV4, BindV4;
		sockaddr_in6 RemoteV6, BindV6;

		if (m_Family == AF_INET) {
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
#ifdef HAVE_IPV6
		} else if (m_Family == AF_INET6) {
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
#endif /* HAVE_IPV6 */
		} else {
			Error(0);

			return;
		}

		int ErrorCode;
		m_Socket = SocketAndConnectResolved(Remote, Bind, &ErrorCode);

		free(m_HostAddr);
		m_HostAddr = NULL;

		if (m_Socket == INVALID_SOCKET) {
#ifndef _WIN32
			if (ErrorCode == 0) {
				ErrorCode = -1;
			}
#endif

			Error(ErrorCode);

			m_LatchedDestruction = true;
		} else {
			InitSocket();
		}
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
	if (Response == NULL || Response->h_addr_list[0] == NULL) {
		// we cannot destroy the object here as there might still be the other
		// dns query (bind ip) in the queue which would get destroyed in the
		// destructor; this causes a crash in the StartMainLoop() function
		m_LatchedDestruction = true;

		return;
	 }

	int Size = INADDR_LEN(Response->h_addrtype);

	m_HostAddr = malloc(Size);

	if (AllocFailed(m_HostAddr)) {
		m_LatchedDestruction = true;
		return;
	}

	m_Family = Response->h_addrtype;
	memcpy(m_HostAddr, Response->h_addr_list[0], Size);

	 if (m_BindIpCache != NULL) {
		m_BindDnsQuery = new CDnsQuery(this, USE_DNSEVENTPROXY(CConnection, AsyncBindIpDnsFinished));
		m_BindDnsQuery->GetHostByName(m_BindIpCache, Response->h_addrtype);
	}

	AsyncConnect();
}

/**
 * AsyncBindIpDnsFinished
 *
 * Called when the DNS query for the local bind address is finished.
 *
 * @param Response the response
 */

void CConnection::AsyncBindIpDnsFinished(hostent *Response) {
	if (Response != NULL && Response->h_addr_list[0] != NULL) {
		int Size = INADDR_LEN(Response->h_addrtype);

		m_BindAddr = malloc(Size);

		if (!AllocFailed(m_BindAddr)) {
			memcpy(m_BindAddr, Response->h_addr_list[0], Size);
		}
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
	if (m_Timeout > 0 && m_Timeout < g_CurrentTime) {
		return true;
	} else {
		return m_LatchedDestruction;
	}
}

/**
 * GetRemoteAddress
 *
 * Returns the remote side's address.
 */
sockaddr *CConnection::GetRemoteAddress(void) const {
	static sockaddr_storage Address;
	socklen_t AddressLength = sizeof(Address);

	if (m_Socket != INVALID_SOCKET && getpeername(m_Socket, (sockaddr *)&Address, &AddressLength) == 0) {
		return (sockaddr *)&Address;
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
	static sockaddr_storage Address;
	socklen_t AddressLength = sizeof(Address);

	if (m_Socket != INVALID_SOCKET && getsockname(m_Socket, (sockaddr *)&Address, &AddressLength) == 0) {
		return (sockaddr *)&Address;
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
		return m_InboundTraffic / (size_t)(g_CurrentTime - m_InboundTrafficReset);
	} else {
		return 0;
	}
}

/**
 * SetSendQ
 *
 * Deletes the current send queue and sets a new queue.
 *
 * @param Buffer the new queue
 */
void CConnection::SetSendQ(CFIFOBuffer *Buffer) {
	delete m_SendQ;

	m_SendQ = Buffer;

	if (m_SendQ == NULL) {
		m_SendQ = new CFIFOBuffer();
	}
}

/**
 * SetRecvQ
 *
 * Deletes the current receive queue and sets a new queue.
 *
 * @param Buffer the new queue
 */
void CConnection::SetRecvQ(CFIFOBuffer *Buffer) {
	delete m_RecvQ;

	m_RecvQ = Buffer;

	if (m_RecvQ == NULL) {
		m_RecvQ = new CFIFOBuffer();
	}
}

/**
 * SetSSL
 *
 * Sets the SSL object for the connection.
 *
 * @param SSLObject ssl object
 */
void CConnection::SetSSLObject(void *SSLObject) {
#ifdef HAVE_LIBSSL
	if (SSLObject == NULL) {
		m_HasSSL = false;
	} else {
		m_HasSSL = true;
	}

	if (m_SSL != NULL) {
		SSL_free(m_SSL);
	}

	m_SSL = (SSL *)SSLObject;
#endif
}
