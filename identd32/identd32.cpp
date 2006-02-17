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

class CIdentModule;

CCore *g_Bouncer;

class CIdentClient : public CSocketEvents {
	CConnection *m_Wrap;
public:
	CIdentClient(SOCKET Client) {
		m_Wrap = g_Bouncer->WrapSocket(Client);
	}

	virtual ~CIdentClient(void) {
		g_Bouncer->UnregisterSocket(m_Wrap->GetSocket());
		g_Bouncer->DeleteWrapper(m_Wrap);
	}

	bool Read(bool DontProcess) {
		char *Line;
		bool ReturnValue = m_Wrap->Read();

		if (ReturnValue == false) {
			return false;
		} else if (DontProcess) {
			return true;
		}

		while (m_Wrap->ReadLine(&Line) == true) {
			ParseLine(Line);

			g_Bouncer->GetUtilities()->Free(Line);
		}

		return true;
	}

	void ParseLine(const char* Line) {
		if (Line[0] == '\0') {
			return;
		}

		if (g_Bouncer->GetIdent() == NULL) {
			LOGERROR("GetIdent() failed. identd not functional.");

			Destroy();

			return;
		}

		int LocalPort = 0, RemotePort = 0;
		char *dupLine = strdup(Line);
		char *StringRemotePort;

		if ((StringRemotePort = strstr(dupLine, ",")) != NULL) {
			StringRemotePort[0] = '\0';
			StringRemotePort++;

			RemotePort = atoi(StringRemotePort);
		}

		LocalPort = atoi(dupLine);
		free(dupLine);

		if (LocalPort == 0 || RemotePort == 0) {
			g_Bouncer->Log("Received invalid ident-request.");

			return;
		}

		int i = 0, LPort, RPort;
		hash_t<CUser *> *UserHash;

		while ((UserHash = g_Bouncer->GetUsers()->Iterate(i++)) != NULL) {
			CUser *User = UserHash->Value;
			CIRCConnection *IRC = User->GetIRCConnection();

			if (IRC == NULL) {
				continue;
			}

			sockaddr *LocalAddress = IRC->GetLocalAddress();
			sockaddr *RemoteAddress = IRC->GetRemoteAddress();

			if (LocalAddress->sa_family != RemoteAddress->sa_family) {
				continue;
			}

			if (LocalAddress->sa_family == AF_INET) {
				LPort = htons(((sockaddr_in *)LocalAddress)->sin_port);
			} else {
				LPort = htons(((sockaddr_in6 *)LocalAddress)->sin6_port);
			}

			if (RemoteAddress->sa_family == AF_INET) {
				RPort = htons(((sockaddr_in *)RemoteAddress)->sin_port);
			} else {
				RPort = htons(((sockaddr_in6 *)RemoteAddress)->sin6_port);
			}

			if (LPort == LocalPort && RPort == RemotePort) {
				// 113 , 3559 : USERID : UNIX : shroud
				m_Wrap->WriteLine("%d , %d : USERID : UNIX : %s", LocalPort, RemotePort, UserHash->Name);

				g_Bouncer->Log("Answered ident-request for %s", User->GetUsername());

				return;
			}
		}

		// 113 , 3559 : USERID : UNIX : shroud
		m_Wrap->WriteLine("%d , %d : USERID : UNIX : %s", LocalPort, RemotePort, g_Bouncer->GetIdent());

		g_Bouncer->Log("Ident-request for unknown user.");
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

	const char *GetClassName(void) {
		return "CIdentClient";
	}
};

IMPL_SOCKETLISTENER(CIdentListener) {
public:
	CIdentListener(int Family) : CListenerBase(113, NULL, Family) { }

	void Accept(SOCKET Client, const sockaddr *PeerAddress) {
		CIdentClient *Handler = new CIdentClient(Client);

		g_Bouncer->RegisterSocket(Client, Handler);
	}
};

class CIdentModule : public CModuleImplementation {
	CIdentListener *m_Listener, *m_ListenerV6;

	void Init(CCore* Root) {
		CModuleImplementation::Init(Root);

		g_Bouncer = Root;

		m_Listener = new CIdentListener(AF_INET);

		if (!m_Listener->IsValid()) {
			g_Bouncer->Log("Could not create listener for identd.");

			delete m_Listener;
			m_Listener = NULL;
		}

		g_Bouncer->Log("Created IPv4 identd listener.");

#ifdef IPV6
		m_ListenerV6 = new CIdentListener(AF_INET6);

		if (!m_ListenerV6->IsValid()) {
			delete m_ListenerV6;

			m_ListenerV6 = NULL;
		} else {
			g_Bouncer->Log("Created IPv6 identd listener.");
		}
#endif
	}

	~CIdentModule(void) {
		if (m_Listener != NULL) {
			g_Bouncer->Log("Destroying IPv4 identd-listener.");

			m_Listener->Destroy();
		}

		if (m_ListenerV6 != NULL) {
			m_ListenerV6->Destroy();

			g_Bouncer->Log("Destroying IPv4 identd-listener.");
		}
	}
};

extern "C" EXPORT CModuleFar *bncGetObject(void) {
	return (CModuleFar *)new CIdentModule();
}

extern "C" EXPORT int bncGetInterfaceVersion(void) {
	return INTERFACEVERSION;
}
