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

#include "StdAfx.h"
#include "../ModuleFar.h"
#include "../BouncerCore.h"
#include "../SocketEvents.h"
#include "../Connection.h"
#include "../ClientConnection.h"
#include "../BouncerUser.h"

#ifndef _WIN32
#error This module cannot be used on *nix systems.
#endif

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 ) {
    return TRUE;
}

class CIdentClient : public CSocketEvents {
	SOCKET m_Client;
	CBouncerCore* m_Core;

	char* recvq;
	int recvq_size;

	char* sendq;
	int sendq_size;
public:
	CIdentClient(SOCKET Client, CBouncerCore* Core) {
		m_Client = Client;
		m_Core = Core;

		recvq = NULL;
		recvq_size = 0;

		sendq = NULL;
		sendq_size = 0;
	}

	virtual ~CIdentClient(void) {
		m_Core->UnregisterSocket(m_Client);
	}

	bool Read(void) {
		char nullBuffer[512];

		int n = recv(m_Client, nullBuffer, sizeof(nullBuffer), 0);

		if (n > 0) {
			recvq = (char*)realloc(recvq, recvq_size + n + 200);

			memcpy(recvq + recvq_size, nullBuffer, n);
			recvq[recvq_size + n] = '\0';

			recvq_size += n;

			if (strstr(recvq, "\n") != 0) {
				for (unsigned int i = 0; i < strlen(recvq); i++) {
					if (recvq[i] == '\r' || recvq[i] == '\n') {
						recvq[i] = '\0';
						break;
					}
				}

				char Out[1024];
				sprintf(Out, "%s : USERID : UNIX : %s\r\n", recvq, m_Core->GetIdent());

				sendq = strdup(Out);
				sendq_size = strlen(sendq);

				sprintf(Out, "Answered ident-request for %s", m_Core->GetIdent());
				m_Core->Log(Out);
			}		
		}

		return true;
	}

	// 113 , 3559 : USERID : UNIX : shroud
	void Write(void) {
		if (sendq) {
			send(m_Client, sendq, sendq_size, 0);
			free(sendq);
			sendq = NULL;
			sendq_size = 0;

			closesocket(m_Client);
		}
	}

	void Error(void) {

	}

	bool HasQueuedData(void) {
		return sendq_size > 0;
	}

	void Destroy(void) {
		delete this;
	}
};

class CIdentModule : public CModuleFar, public CSocketEvents {
	SOCKET m_Listener;
	CBouncerCore* m_Core;

	void Destroy(void) {
		m_Core->Log("Destroying identd-listener.");

		closesocket(m_Listener);

		delete this;
	}

	void Init(CBouncerCore* Root) {
		m_Core = Root;

		m_Listener = m_Core->CreateListener(113);

		if (m_Listener == INVALID_SOCKET) {
			m_Core->Log("Could not create listener for identd.");
			return;
		}

		m_Core->Log("Created identd listener.");

		m_Core->RegisterSocket(m_Listener, this);
	}

	void Pulse(time_t Now) {
	}

	bool InterceptIRCMessage(CIRCConnection* IRC, int argc, const char** argv) {
		return true;
	}

	bool InterceptClientMessage(CClientConnection* Client, int argc, const char** argv) {
		return true;
	}

	bool Read(void) {
		sockaddr_in sin;
		int len = sizeof(sin);

		SOCKET Client = accept(m_Listener, (sockaddr*)&sin, &len);

		CIdentClient* Handler = new CIdentClient(Client, m_Core);

		m_Core->RegisterSocket(Client, Handler);

		return true;
	}

	void Write(void) {
	}

	void Error(void) {
	}

	bool HasQueuedData(void) {
		return false;
	}

	void AttachClient(const char* Client) { }
	void DetachClient(const char* Client) { }

	void SingleModeChange(CIRCConnection* IRC, const char* Channel, const char* Source, bool Flip, char Mode, const char* Parameter) { }
};

extern "C" CModuleFar* bncGetObject(void) {
	return (CModuleFar*)new CIdentModule();
}
