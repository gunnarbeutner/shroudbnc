/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007 Gunnar Beutner                                           *
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

class CUser;
class CTrafficStats;
class CFIFOBuffer;

/**
 * connection_role_e
 *
 * The role of a connection.
 */
enum connection_role_e {
	Role_Unknown,
	Role_Server = Role_Unknown,
	Role_Client
};

/**
 * CConnection
 *
 * A base class for connections.
 */
class SBNCAPI CConnection : public CSocketEvents {
#ifndef SWIG
	friend class CCore;
	friend class CUser;
#endif
protected:
	virtual void ParseLine(const char *Line);

	void Timeout(int TimeLeft);

	void SetRole(connection_role_e Role);

	void InitSocket(void);

	void ProcessBuffer(void);

	void AsyncConnect(void);

	bool m_Locked; /**< determines whether data can be written for this connection */
	bool m_Shutdown; /**< are we about to close this socket? */
	time_t m_Timeout; /**< timeout for this socket */

	bool m_HasSSL; /**< is this an ssl-enabled connection? */
	SSL *m_SSL; /**< SSL context for this connection */

	CFIFOBuffer *m_SendQ; /**< send queue */
	CFIFOBuffer *m_RecvQ; /**< receive queue */

public:
	virtual void AsyncDnsFinished(hostent *Response);
	virtual void AsyncBindIpDnsFinished(hostent *Response);

private:
	CDnsQuery *m_DnsQuery; /**< the dns query for looking up the hostname */
	CDnsQuery *m_BindDnsQuery; /**< the dns query for looking up the bind address */
	unsigned short m_PortCache; /**< the port or -1 if the cache is invalided */
	char *m_BindIpCache; /**< the bind address */

	bool m_LatchedDestruction; /**< should the connection object be destroyed? */
	CTrafficStats *m_Traffic; /**< the traffic statistics for this connection */

	void *m_BindAddr; /**< the bind address (an in_addr or in_addr6) */
	void *m_HostAddr; /** the remote address (an in_addr or in_addr6) */

	connection_role_e m_Role; /**< the role of this connection */

	SOCKET m_Socket; /**< the socket */
	int m_Family; /**< the socket's address family */

	bool m_Connected; /**< is the object connected? */

	time_t m_InboundTrafficReset; /**< when the inbound traffic was last reset */
	size_t m_InboundTraffic; /**< inbound traffic (in bytes) since last reset */

	void InitConnection(SOCKET Client, bool SSL);

	virtual const char *GetClassName(void) const;
public:
#ifndef SWIG
	CConnection(SOCKET Socket, bool SSL = false, connection_role_e Role = Role_Unknown);
	CConnection(const char *Host, unsigned short Port, const char *BindIp = NULL, bool SSL = false, int Family = AF_INET);
	virtual ~CConnection(void);
#endif

	void SetSocket(SOCKET Socket);
	SOCKET GetSocket(void) const;

	virtual void WriteUnformattedLine(const char *Line);
	virtual void WriteLine(const char *Format, ...);
	virtual bool ReadLine(char **Out);

	connection_role_e GetRole(void) const;

	virtual void Kill(const char *Error);

	size_t GetSendqSize(void) const;
	size_t GetRecvqSize(void) const;

	void Lock(void);
	bool IsLocked(void) const;

	void Shutdown(void);

	void SetTrafficStats(CTrafficStats *Stats);
	const CTrafficStats *GetTrafficStats(void) const;

	void FlushSendQ(void);

	bool IsSSL(void) const;
	const X509 *GetPeerCertificate(void) const;
	virtual int SSLVerify(int PreVerifyOk, X509_STORE_CTX *Context) const;

	sockaddr *GetRemoteAddress(void) const;
	sockaddr *GetLocalAddress(void) const;

	void Destroy(void);

	bool IsConnected(void);

	size_t GetInboundRate(void);

	void SetSendQ(CFIFOBuffer *Buffer);
	void SetRecvQ(CFIFOBuffer *Buffer);
	void SetSSLObject(void *SSLObject);

	// should really be "protected"
	virtual int Read(bool DontProcess = false);
	virtual int Write(void);
	virtual void Error(int ErrorCode);
	virtual bool HasQueuedData(void) const;
	virtual bool ShouldDestroy(void) const;
};
