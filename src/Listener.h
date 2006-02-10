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

template <typename EventClassName>
class CListenerBase : public CSocketEvents {
private:
	SOCKET m_Listener;
	EventClassName *m_EventObject;

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
	EventClassName *GetEventObject(void) {
		return m_EventObject;
	}

	virtual void Accept(SOCKET Client, const sockaddr *PeerAddress) {
		closesocket(Client);
	}
public:
	CListenerBase(SOCKET Listener, EventClassName *EventObject = NULL) {
		Init(Listener, EventObject);
	}

	CListenerBase(unsigned int Port, const char *BindIp = NULL, EventClassName *EventObject = NULL, int Family = AF_INET) {
		Init(g_Bouncer->CreateListener(Port, BindIp, Family), EventObject);
	}

	void Init(SOCKET Listener, EventClassName *EventObject) {
		m_EventObject = EventObject;
		m_Listener = Listener;

		if (m_Listener == INVALID_SOCKET) {
			return;
		}

		g_Bouncer->RegisterSocket(m_Listener, static_cast<CSocketEvents *>(this));
	}

	virtual ~CListenerBase(void) {
		if (g_Bouncer && m_Listener != INVALID_SOCKET) {
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

#define IMPL_SOCKETLISTENER(ClassName, EventClassName) class ClassName : public CListenerBase<EventClassName>
