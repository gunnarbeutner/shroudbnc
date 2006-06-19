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

/**
 * CListenerBase<InheritedClass>
 *
 * Implements a generic socket listener.
 */
template<typename InheritedClass>
class CListenerBase : public CSocketEvents/*, public CObject<InheritedClass, CCore>*/ {
private:
	SOCKET m_Listener; /**< the listening socket */

	virtual bool Read(bool DontProcess) {
		char PeerAddress[MAX_SOCKADDR_LEN];
		socklen_t PeerSize = sizeof(PeerAddress);
		SOCKET Client;

		Client = accept(m_Listener, (sockaddr *)PeerAddress, &PeerSize);

		if (Client != INVALID_SOCKET) {
			Accept(Client, (sockaddr *)PeerAddress);
		}

		return true;
	}

	virtual void Write(void) { }
	virtual void Error(void) { }
	virtual bool HasQueuedData(void) const { return false; }
	virtual bool DoTimeout(void) { return false; }
	virtual bool ShouldDestroy(void) const { return false; }

	virtual const char *GetClassName(void) const { return "CListenerBase"; }

	/**
	 * Initialize
	 *
	 * Initialized the listener object.
	 *
	 * @param Listener a listening socket
	 */
	void Initialize(SOCKET Listener) {
		m_Listener = Listener;

		if (m_Listener == INVALID_SOCKET) {
			return;
		}

		g_Bouncer->RegisterSocket(m_Listener, static_cast<CSocketEvents *>(this));
	}

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
		Initialize(g_Bouncer->CreateListener(Port, BindIp, Family));
	}

	CListenerBase(void) {
		// used for unfreezing listeners
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

		closesocket(m_Listener);
	}

	/**
	 * Freeze
	 *
	 * Persists the socket listener in a box.
	 *
	 * @param Box the box
	 */
	RESULT<bool> Freeze(CAssocArray *Box) {
		Box->AddInteger("~listener.fd", m_Listener);
		g_Bouncer->UnregisterSocket(m_Listener);
		m_Listener = INVALID_SOCKET;

		delete this;

		RETURN(bool, true);
	}

	/**
	 * Thaw
	 *
	 * Depersists a listener object.
	 *
	 * @param Box the box which is being used for storing the listener
	 */
	static RESULT<InheritedClass *> Thaw(CAssocArray *Box, CCore *Owner) {
		InheritedClass *Listener = new InheritedClass();

		CHECK_ALLOC_RESULT(Listener, new) {
			THROW(InheritedClass *, Generic_OutOfMemory, "new operator failed.");
		} CHECK_ALLOC_RESULT_END;

//		Listener->SetOwner(Owner);
		Listener->Initialize(Box->ReadInteger("~listener.fd"));

		RETURN(InheritedClass *, Listener);
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

	virtual unsigned short GetPort(void) const {
		sockaddr_in Address;
		socklen_t Length = sizeof(Address);

		if (m_Listener == INVALID_SOCKET || getsockname(m_Listener, (sockaddr *)&Address, &Length) != 0) {
			return 0;
		} else {
			return ntohs(Address.sin_port);
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
	 * CClientListener
	 *
	 * Default constructor.
	 */
	CClientListener(void) {
		m_SSL = false;
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
#else
class CClientListener;
#endif
