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

class CBouncerUser;
class CTrafficStats;
class CFIFOBuffer;
struct sockaddr_in;

#ifndef USESSL
typedef void SSL;
typedef void X509;
typedef void X509_STORE_CTX;
#endif

enum connection_role_e {
	Role_Unknown,
	Role_Client,
	Role_IRC
};

class CConnectionDnsEvents;

class CConnection : public CSocketEvents {
#ifndef SWIG
	friend class CBouncerCore;
	friend class CBouncerUser;
	friend class CConnectionDnsEvents;
	friend bool IRCAdnsTimeoutTimer(time_t Now, void* IRC);
#endif

	CTimer* m_AdnsTimeout;
	adns_query* m_AdnsQuery;
	unsigned short m_PortCache;
	char* m_BindIpCache;
	CConnectionDnsEvents* m_DnsEvents;

	bool m_LatchedDestruction;
	CTrafficStats* m_Traffic;

	void InitConnection(SOCKET Client, bool SSL);
public:
#ifndef SWIG
	CConnection(SOCKET Client, bool SSL = false);
	CConnection(const char* Host, unsigned short Port, const char* BindIp, bool SSL = false);
#endif
	virtual ~CConnection(void);

	virtual SOCKET GetSocket(void);
	virtual CBouncerUser* GetOwningClient(void);

	virtual void InternalWriteLine(const char* In);
	virtual void WriteLine(const char* Format, ...);

	virtual connection_role_e GetRole(void);

	virtual void Kill(const char* Error);

	virtual bool HasQueuedData(void);

	virtual int SendqSize(void);
	virtual int RecvqSize(void);

	virtual void Destroy(void);
	virtual void Error(void);
	virtual const char* ClassName(void);

	virtual bool Read(bool DontProcess = false);
	virtual void Write(void);

	virtual void Lock(void);
	virtual bool IsLocked(void);

	virtual void Shutdown(void);
	virtual void Timeout(int TimeLeft);
	virtual bool DoTimeout(void);

	virtual void AttachStats(CTrafficStats* Stats);
	virtual CTrafficStats* GetTrafficStats(void);

	virtual bool ReadLine(char** Out);

	virtual void FlushSendQ(void);

	virtual void InitSocket(void);

	virtual bool IsSSL(void);
	virtual X509* GetPeerCertificate(void);
	virtual int SSLVerify(int PreVerifyOk, X509_STORE_CTX* Context);

	virtual bool ShouldDestroy(void);
protected:
	virtual void ParseLine(const char* Line);
#ifndef SWIG
	void ReadLines(void);
#endif

	void AsyncDnsFinished(adns_query* query, adns_answer* response);
	void AdnsTimeout(void);

	CBouncerUser* m_Owner;

	SOCKET m_Socket;

	bool m_Locked;
	bool m_Shutdown;
	time_t m_Timeout;

	bool m_Wrapper;

	SSL* m_SSL;
private:
	bool m_HasSSL;

	CFIFOBuffer* m_SendQ;
	CFIFOBuffer* m_RecvQ;
};
