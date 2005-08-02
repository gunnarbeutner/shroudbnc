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
#include "../DnsEvents.h"
#include "../Connection.h"
#include "../ClientConnection.h"
#include "../BouncerUser.h"
#include "../Connection.h"

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
	CBouncerCore* m_Core;
	CConnection* m_Wrap;
public:
	CIdentClient(SOCKET Client, CBouncerCore* Core) {
		sockaddr_in Peer;
		int PeerSize = sizeof(Peer);
		getpeername(Client, (sockaddr*)&Peer, &PeerSize);

		m_Wrap = Core->WrapSocket(Client, Peer);

		m_Core = Core;
	}

	virtual ~CIdentClient(void) {
		m_Core->UnregisterSocket(m_Wrap->GetSocket());
		m_Core->DeleteWrapper(m_Wrap);
	}

	bool Read(bool DontProcess) {
		bool RetVal = m_Wrap->Read();

		if (!RetVal)
			return false;

		char* Line;

		if (DontProcess)
			return true;

		while (m_Wrap->ReadLine(&Line)) {
			ParseLine(Line);

			m_Core->Free(Line);
		}

		return true;
	}

	void ParseLine(const char* Line) {
		// 113 , 3559 : USERID : UNIX : shroud
		m_Wrap->WriteLine("%s : USERID : UNIX : %s", Line, m_Core->GetIdent());

		m_Core->Log("Answered ident-request for %s", m_Core->GetIdent());
	}

	void Write(void) {
		m_Wrap->Write();
	}

	void Error(void) {
		m_Wrap->Error();
	}

	bool HasQueuedData(void) {
		return m_Wrap->HasQueuedData();
	}

	void Destroy(void) {
		delete this;
	}

	bool DoTimeout(void) {
		return m_Wrap->DoTimeout();
	}

	const char* ClassName(void) {
		return "CIdentClient";
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

	bool InterceptIRCMessage(CIRCConnection* IRC, int argc, const char** argv) {
		return true;
	}

	bool InterceptClientMessage(CClientConnection* Client, int argc, const char** argv) {
		return true;
	}

	bool Read(bool DontProcess) {
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

	bool DoTimeout(void) { return false; }

	const char* ClassName(void) {
		return "CIdentModule";
	}

	void AttachClient(const char* Client) { }
	void DetachClient(const char* Client) { }

	void ServerDisconnect(const char* Client) { }
	void ServerConnect(const char* Client) { }
	void ServerLogon(const char* Client) { }

	void UserLoad(const char* User) { }
	void UserCreate(const char* User) { }
	void UserDelete(const char* User) { }

	void SingleModeChange(CIRCConnection* IRC, const char* Channel, const char* Source, bool Flip, char Mode, const char* Parameter) { }

	const char* Command(const char* Cmd, const char* Parameters) { return NULL; }

	bool InterceptClientCommand(CClientConnection* Connection, const char* Subcommand, int argc, const char** argv, bool NoticeUser) { return false; }
};

extern "C" CModuleFar* bncGetObject(void) {
	return (CModuleFar*)new CIdentModule();
}
