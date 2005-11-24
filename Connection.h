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

enum connection_role_e {
	Role_Unknown,
	Role_Client,
	Role_IRC
};

class CConnection : public CSocketEvents {
	friend class CBouncerCore;
	friend class CBouncerUser;

	bool HandleSSLError(int RetVal);
public:
#ifndef SWIG
	CConnection(SOCKET Client, bool SSL = false);
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
	/* X509 */ void* GetPeerCertificate(void);
protected:
	virtual void ParseLine(const char* Line);
#ifndef SWIG
	void ReadLines(void);
#endif

	CBouncerUser* m_Owner;

	SOCKET m_Socket;

	bool m_Locked;
	bool m_Shutdown;
	time_t m_Timeout;

	CTrafficStats* m_Traffic;

	bool m_Wrapper;

private:
#ifdef USESSL
	bool m_HasSSL;
	bool m_CheckCert;
	SSL* m_SSL;
#endif

	CFIFOBuffer* m_SendQ;
	CFIFOBuffer* m_RecvQ;
};
