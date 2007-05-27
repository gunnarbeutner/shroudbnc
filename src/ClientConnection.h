/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007 Gunnar Beutner                                      *
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

#ifdef SWIGINTERFACE
%template(COwnedObjectCUser) COwnedObject<class CUser>;
%template(CZoneObjectCClientConnection) CZoneObject<class CClientConnection, 16>;
#endif

#ifndef SWIG
bool ClientAuthTimer(time_t Now, void *Client);
bool ClientPingTimer(time_t Now, void *ClientConnection);
#endif

/**
 * clientdata_t
 *
 * Contains a connection's socket, queues and SSL context.
 */
typedef struct clientdata_s {
	SOCKET Socket;
	CFIFOBuffer *SendQ;
	CFIFOBuffer *RecvQ;
	SSL *SSLObject;
} clientdata_t;

/**
 * CClientConnection
 *
 * A class for clients of the IRC bouncer.
 */
class SBNCAPI CClientConnection : public CConnection, public CObject<CClientConnection, CUser>,
	public CZoneObject<CClientConnection, 16> {
private:
	char *m_Nick; /**< the current nick of the user */
	char *m_Password; /**< the password which was supplied by the user */
	char *m_Username; /**< the username the user supplied */
	char *m_PeerName; /**< the hostname of the user */
	char *m_PeerNameTemp; /**< a temporary variable for the hostname */
	commandlist_t m_CommandList; /**< a list of commands used by the "help" command */
	bool m_NamesXSupport; /**< does this client support NAMESX? */
	CDnsQuery *m_ClientLookup; /**< dns query for looking up the user's hostname */
	char *m_QuitReason; /**< reason why the client was removed */
	CTimer* m_PingTimer; /**< timer for sending regular PINGs to the client */
	time_t m_LastResponse; /**< last response from the client */

#ifndef SWIG
	friend bool ClientAuthTimer(time_t Now, void *Client);
	friend bool ClientPingTimer(time_t Now, void *ClientConnection);

protected:
	CTimer *m_AuthTimer; /**< used for timing out unauthed connections */

public:
	void AsyncDnsFinishedClient(hostent *response);

private:
#endif

	bool ValidateUser(void);
	void SetPeerName(const char *PeerName, bool LookupFailure);
	virtual int Read(bool DontProcess = false);
	virtual const char *GetClassName(void) const;
	void WriteUnformattedLine(const char *Line);
	bool ParseLineArgV(int argc, const char **argv);
	bool ProcessBncCommand(const char *Subcommand, int argc, const char **argv, bool NoticeUser);

	CClientConnection(safe_box_t Box);

public:
#ifndef SWIG
	CClientConnection(SOCKET Socket, safe_box_t Box, bool SSL = false);
	virtual ~CClientConnection(void);
#endif

	virtual void ParseLine(const char *Line);

	virtual const char *GetNick(void) const;
	virtual const char *GetPeerName(void) const;

	virtual void Kill(const char *Error);
	virtual void Destroy(void);
	virtual void Error(int ErrorCode);

	virtual commandlist_t *GetCommandList(void);

	virtual clientdata_t Hijack(void);

	virtual void ChangeNick(const char *NewNick);
	virtual void SetNick(const char *NewNick);

	virtual void Privmsg(const char *Text);
	virtual void RealNotice(const char *Text);

	virtual void SetQuitReason(const char *Reason);
	virtual const char *GetQuitReason(void) const;
};

#ifdef SBNC
/**
 * CFakeClient
 *
 * A fake client connection which is used by CClientConnection::Simulate.
 */
class SBNCAPI CFakeClient : public CClientConnection {
	CFIFOBuffer m_Queue; /**< used for storing inbound data */
	char *m_Data; /**< temporary "static" copy of m_Queue's data */

	/**
	 * WriteUnformattedLine
	 *
	 * Re-implementation of CClientConnection::WriteUnformattedLine.
	 *
	 * @param Line the line
	 */
	virtual void WriteUnformattedLine(const char *Line) {
		m_Queue.WriteUnformattedLine(Line);
	}
public:
	/**
	 * CFakeClient
	 *
	 * Constructs a new fake client.
	 */
	CFakeClient(void) : CClientConnection(INVALID_SOCKET, false) {
		m_Data = NULL;
	}

	/**
	 * ~CFakeClient
	 *
	 * Destructs a fake client.
	 */
	virtual ~CFakeClient(void) {
		free(m_Data);
	}

	/**
	 * GetData
	 *
	 * Returns a temporary buffer containing the lines which have been
	 * sent to the client so far.
	 */
	const char *GetData(void) {
		free(m_Data);

		m_Data = (char *)malloc(m_Queue.GetSize() + 1);

		if (m_Data != NULL) {
			mmark(m_Data);
			memcpy(m_Data, m_Queue.Peek(), m_Queue.GetSize());
			m_Data[m_Queue.GetSize()] = '\0';
		}

		return m_Data;
	}

	/**
	 * operator new
	 *
	 * Overrides the base class new operator. CClientConnection's would not work
	 * because CFakeClient objects are larger than CClientConnection objects.
	 */
	void *operator new (size_t Size) {
		return malloc(Size);
	}

	/**
	 * operator delete
	 *
	 * Overrides the base class new operator.
	 */
	void operator delete(void *Object) {
		free(Object);
	}
};
#else
class CFakeClient;
#endif
