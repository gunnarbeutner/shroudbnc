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

class CUser;
class CTrafficStats;
class CFIFOBuffer;
struct sockaddr_in;

enum connection_role_e {
	Role_Unknown,
	Role_Server = Role_Unknown,
	Role_Client
};

class CConnection : public CSocketEvents {
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

	SOCKET m_Socket; /**< the socket */

	bool m_Locked; /**< determines whether data can be written for this connection */
	bool m_Shutdown; /**< are we about to close this socket? */
	time_t m_Timeout; /**< timeout for this socket */

	bool m_Wrapper; /**< was this object created as a wrapper for another socket? */

	SSL *m_SSL; /**< SSL context for this connection */
public:
	virtual void AsyncDnsFinished(hostent *Response);
	virtual void AsyncBindIpDnsFinished(hostent *Response);
private:
	bool m_HasSSL; /**< is this an ssl-enabled connection? */

	CFIFOBuffer *m_SendQ; /**< send queue */
	CFIFOBuffer *m_RecvQ; /**< receive queue */

	CDnsQuery *m_DnsQuery;
	CDnsQuery *m_BindDnsQuery;
	unsigned short m_PortCache;
	char *m_BindIpCache;

	bool m_LatchedDestruction;
	CTrafficStats *m_Traffic;

	void *m_BindAddr;
	void *m_HostAddr;

	connection_role_e m_Role;

	int m_Family;

	void InitConnection(SOCKET Client, bool SSL);
public:
#ifndef SWIG
	CConnection(SOCKET Socket, bool SSL = false, connection_role_e Role = Role_Unknown);
	CConnection(const char *Host, unsigned short Port, const char *BindIp = NULL, bool SSL = false, int Family = AF_INET);
#endif
	virtual ~CConnection(void);

	virtual SOCKET GetSocket(void);

	virtual void WriteUnformattedLine(const char *Line);
	virtual void WriteLine(const char *Format, ...);
	virtual bool ReadLine(char **Out);

	virtual connection_role_e GetRole(void);

	virtual void Kill(const char *Error);

	virtual int GetSendqSize(void);
	virtual int GetRecvqSize(void);

	virtual void Lock(void);
	virtual bool IsLocked(void);

	virtual void Shutdown(void);

	virtual void SetTrafficStats(CTrafficStats *Stats);
	virtual CTrafficStats *GetTrafficStats(void);

	virtual void FlushSendQ(void);

	virtual bool IsSSL(void);
	virtual X509 *GetPeerCertificate(void);
	virtual int SSLVerify(int PreVerifyOk, X509_STORE_CTX *Context);
	virtual bool ShouldDestroy(void);

	virtual sockaddr *GetRemoteAddress(void);
	virtual sockaddr *GetLocalAddress(void);

	virtual const char *GetClassName(void);

	void Destroy(void);

	// should really be "protected"
	virtual bool Read(bool DontProcess = false);
	virtual void Write(void);
	virtual bool DoTimeout(void);
	virtual void Error(void);
	virtual bool HasQueuedData(void);
};
