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

#include "../src/StdAfx.h"

#ifndef _WIN32
#error This module cannot be used on *nix systems.
#endif

CBouncerCore* g_Bouncer;

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 ) {
    return TRUE;
}

class CIdentClient : public CSocketEvents {
	CConnection* m_Wrap;
public:
	CIdentClient(SOCKET Client) {
		m_Wrap = g_Bouncer->WrapSocket(Client);
	}

	virtual ~CIdentClient(void) {
		g_Bouncer->UnregisterSocket(m_Wrap->GetSocket());
		g_Bouncer->DeleteWrapper(m_Wrap);
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

			g_Bouncer->Free(Line);
		}

		return true;
	}

	void ParseLine(const char* Line) {
		if (*Line == '\0')
			return;

		if (g_Bouncer->GetIdent() == NULL) {
			LOGERROR("GetIdent() failed. identd not functional.");

			Destroy();

			return;
		}

		// 113 , 3559 : USERID : UNIX : shroud
		m_Wrap->WriteLine("%s : USERID : UNIX : %s", Line, g_Bouncer->GetIdent());

		g_Bouncer->Log("Answered ident-request for %s", g_Bouncer->GetIdent());
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

	bool ShouldDestroy(void) { return false; }

	const char* ClassName(void) {
		return "CIdentClient";
	}
};

class CIdentModule : public CModuleFar, public CSocketEvents {
	SOCKET m_Listener;
	void Destroy(void) {
		g_Bouncer->Log("Destroying identd-listener.");

		g_Bouncer->UnregisterSocket(m_Listener);
		closesocket(m_Listener);

		delete this;
	}

	void Init(CBouncerCore* Root) {
		g_Bouncer = Root;

		m_Listener = g_Bouncer->CreateListener(113);

		if (m_Listener == INVALID_SOCKET) {
			g_Bouncer->Log("Could not create listener for identd.");
			return;
		}

		g_Bouncer->Log("Created identd listener.");

		g_Bouncer->RegisterSocket(m_Listener, this);
	}

	int GetInterfaceVersion(void) {
		return INTERFACEVERSION;
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

		CIdentClient* Handler = new CIdentClient(Client);

		g_Bouncer->RegisterSocket(Client, Handler);

		return true;
	}

	void Write(void) {
	}

	void Error(void) {
	}

	bool HasQueuedData(void) {
		return false;
	}

	bool ShouldDestroy(void) { return false; }

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

extern "C" EXPORT CModuleFar* bncGetObject(void) {
	return (CModuleFar*)new CIdentModule();
}

extern "C" EXPORT int bncGetInterfaceVersion(void) {
	return INTERFACEVERSION;
}
