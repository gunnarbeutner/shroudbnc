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

template<typename InheritedClass>
class CListenerBase : public CSocketEvents {
private:
	SOCKET m_Listener;

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
	virtual bool HasQueuedData(void) { return false; }
	virtual bool DoTimeout(void) { return false; }
	virtual bool ShouldDestroy(void) { return false; }

	virtual const char *GetClassName(void) { return "CListenerBase"; }

protected:
	virtual void Accept(SOCKET Client, const sockaddr *PeerAddress) {
		closesocket(Client);
	}

public:
	CListenerBase(unsigned int Port, const char *BindIp = NULL, int Family = AF_INET) {
		Init(g_Bouncer->CreateListener(Port, BindIp, Family));
	}

	CListenerBase(void) {
		// used for unfreezing listeners
	}

	bool Freeze(CAssocArray *Box) {
		Box->AddInteger("~listener.fd", m_Listener);
		g_Bouncer->UnregisterSocket(m_Listener);
		m_Listener = INVALID_SOCKET;

		delete this;

		return true;
	}

	static InheritedClass *Unfreeze(CAssocArray *Box) {
		InheritedClass *Listener = new InheritedClass();
		
		Listener->Init(Box->ReadInteger("~listener.fd"));

		return Listener;
	}

	void Init(SOCKET Listener) {
		m_Listener = Listener;

		if (m_Listener == INVALID_SOCKET) {
			return;
		}

		g_Bouncer->RegisterSocket(m_Listener, static_cast<CSocketEvents *>(this));
	}

	virtual ~CListenerBase(void) {
		if (g_Bouncer != NULL && m_Listener != INVALID_SOCKET) {
			g_Bouncer->UnregisterSocket(m_Listener);
		}

		closesocket(m_Listener);
	}

	virtual void Destroy(void) {
		delete this;
	}

	virtual bool IsValid(void) { 
		if (m_Listener != INVALID_SOCKET) {
			return true;
		} else {
			return false;
		}
	}

	virtual SOCKET GetSocket(void) {
		return m_Listener;
	}

	virtual void SetSocket(SOCKET Socket) {
		m_Listener = Socket;
	}
};

#define IMPL_SOCKETLISTENER(ClassName) class ClassName : public CListenerBase<ClassName>
