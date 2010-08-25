/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007,2010 Gunnar Beutner                                 *
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

#ifndef LISTENER_H
#define LISTENER_H

/**
 * CListenerBase<InheritedClass>
 *
 * Implements a generic socket listener.
 */
template<typename InheritedClass>
class CListenerBase : public CSocketEvents {
private:
	SOCKET m_Listener; /**< the listening socket */

	virtual int Read(bool DontProcess) {
		sockaddr_storage PeerAddress;
		socklen_t PeerSize = sizeof(PeerAddress);
		SOCKET Client;

		Client = accept(m_Listener, (sockaddr *)&PeerAddress, &PeerSize);

		if (Client != INVALID_SOCKET) {
			Accept(Client, (sockaddr *)&PeerAddress);
		}

		return 0;
	}

	virtual int Write(void) { return 0; }
	virtual void Error(int ErrorCode) { }
	virtual bool HasQueuedData(void) const { return false; }
	virtual bool ShouldDestroy(void) const { return false; }

	virtual const char *GetClassName(void) const { return "CListenerBase"; }

protected:
	virtual void Accept(SOCKET Client, const sockaddr *PeerAddress) = 0;

public:
	/**
	 * CListenerBase
	 *
	 * Constructs a new CListenerBase object.
	 *
	 * @param Port the port of the socket listener
	 * @param BindIp the ip address used for binding the listener
	 * @param Family the socket family of the listener (AF_INET or AF_INET6)
	 */
	CListenerBase(unsigned int Port, const char *BindIp = NULL, int Family = AF_INET) {
		m_Listener = INVALID_SOCKET;

		if (m_Listener == INVALID_SOCKET) {
			m_Listener = g_Bouncer->CreateListener(Port, BindIp, Family);
		}

		if (m_Listener != INVALID_SOCKET) {
			g_Bouncer->RegisterSocket(m_Listener, static_cast<CSocketEvents *>(this));
		}
	}

	/**
	 * ~CListenerBase
	 *
	 * Destructs a listener object.
	 */
	virtual ~CListenerBase(void) {
		if (g_Bouncer != NULL && m_Listener != INVALID_SOCKET) {
			g_Bouncer->UnregisterSocket(m_Listener);
		}

		if (m_Listener != INVALID_SOCKET) {
			closesocket(m_Listener);
		}
	}

	/**
	 * Destroy
	 *
	 * Destroys a listener object and calls its destructor.
	 */
	virtual void Destroy(void) {
		delete this;
	}

	/**
	 * IsValid
	 *
	 * Verifies that the underlying socket object is valid.
	 */
	virtual bool IsValid(void) const { 
		if (m_Listener != INVALID_SOCKET) {
			return true;
		} else {
			return false;
		}
	}

	/**
	 * GetSocket
	 *
	 * Returns the socket which is used by the listener object.
	 */
	virtual SOCKET GetSocket(void) const {
		return m_Listener;
	}

	virtual unsigned int GetPort(void) const {
		sockaddr_storage Address;
		socklen_t Length = sizeof(Address);

		if (m_Listener == INVALID_SOCKET || getsockname(m_Listener, (sockaddr *)&Address, &Length) != 0) {
			return 0;
		} else {
			if (Address.ss_family == AF_INET) {
				return ntohs(((sockaddr_in *)&Address)->sin_port);
			} else if (Address.ss_family == AF_INET6) {
				return ntohs(((sockaddr_in6 *)&Address)->sin6_port);
			} else {
				return 0;
			}
		}
	}
};

/**
 * IMPL_SOCKETLISTENER
 *
 * Can be used to implement a custom listener.
 */
#define IMPL_SOCKETLISTENER(ClassName) class ClassName : public CListenerBase<ClassName>

#ifdef SBNC
 /**
  * CClientListener
  *
  * A listener which can be used by bouncer clients to connect to the bouncer.
  */
IMPL_SOCKETLISTENER(CClientListener) {
	bool m_SSL; /**< whether this is an SSL listener */
public:
	/**
	 * CClientListener
	 *
	 * Constructs  a new client listener.
	 *
	 * @param Port the port
	 * @param BindIp the bind address (or NULL)
	 * @param Family socket family (AF_INET or AF_INET6)
	 * @param SSL whether the listener should be using ssl
	 */
	CClientListener(unsigned int Port, const char *BindIp = NULL, int Family = AF_INET, bool SSL = false) : CListenerBase<CClientListener>(Port, BindIp, Family) {
		m_SSL = SSL;
	}

	/**
	 * Accept
	 *
	 * Accepts a new client.
	 *
	 * @param Client the client socket
	 * @param PeerAddress the remote address of the client
	 */
	virtual void Accept(SOCKET Client, const sockaddr *PeerAddress) {
		CClientConnection *ClientObject;
		unsigned long lTrue = 1;

		ioctlsocket(Client, FIONBIO, &lTrue);

		// destruction is controlled by the main loop
		ClientObject = new CClientConnection(Client, m_SSL);
	}

	/**
	 * SetSSL
	 *
	 * Sets whether this listener should accept SSL clients.
	 *
	 * @param SSL boolean flag
	 */
	void SetSSL(bool SSL) {
		m_SSL = SSL;
	}

	/**
	 * GetSSL
	 *
	 * Checks whether this listener accepts SSL clients.
	 */
	bool GetSSL(void) const {
		return m_SSL;
	}
};
#else /* SBNC */
class CClientListener;
#endif /* SBNC */

#endif /* LISTENER_H */
