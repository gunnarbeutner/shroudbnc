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

#include "../StdAfx.h"

#ifdef _WIN32
BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 ) {
    return TRUE;
}
#else
int main(int argc, char* argv[]) { return 0; }
#endif

class CHelloClass : public CModuleFar {
	CBouncerCore* m_Core;
	CBouncerUser* m_Bot;

	void Destroy(void) {
		m_Core->RemoveUser(m_Bot->GetUsername());

		delete this;
	}

	void Init(CBouncerCore* Root) {
		m_Core = Root;

		m_Bot = m_Core->CreateUser("bot", NULL);

		m_Bot->Lock();

		if (!m_Bot->GetServer()) {
			m_Bot->SetNick("shroudbot");
			m_Bot->SetRealname("Hello World BNC module");
			m_Bot->SetServer("dk.quakenet.org");
			m_Bot->SetPort(6667);
			m_Bot->SetConfigChannels("#shroudtest2");
		}

		m_Bot->ScheduleReconnect();
	}

	bool InterceptIRCMessage(CIRCConnection* IRC, int argc, const char** argv) {
		if (IRC == m_Bot->GetIRCConnection()) {
			if (argc >= 3 && strcmpi(argv[1], "privmsg") == 0 && strcmpi(argv[3], "!hello") == 0) {
				char Out[1024];

				const char* Other;

				if (strcmpi(argv[2], IRC->GetCurrentNick()) == 0)
					Other = IRC->NickFromHostmask(argv[0]);
				else
					Other = argv[2];

#undef sprintf
				sprintf(Out, "PRIVMSG %s :%s", Other, IRC->GetChannel("#shroudtest2")->GetChanModes());

				if (Other != argv[2])
					IRC->FreeNick(const_cast<char*>(Other));

				IRC->WriteLine(Out);
			}
		}

		return true;
	}

	bool InterceptClientMessage(CClientConnection* Client, int argc, const char** argv) {
		return true;
	}

	void AttachClient(const char* Client) { }
	void DetachClient(const char* Client) { }

	void ServerDisconnect(const char* Client) { }
	void ServerConnect(const char* Client) { }
	void ServerLogon(const char* Client) { }

	void UserLoad(const char* User) { }
	void UserCreate(const char* User) { }
	void UserDelete(const char* User) { }

	void SingleModeChange(CIRCConnection* Connection, const char* Channel, const char* Source, bool Flip, char Mode, const char* Parameter) { }

	const char* Command(const char* Cmd, const char* Parameters) { return NULL; }

	bool InterceptClientCommand(CClientConnection* Connection, const char* Subcommand, int argc, const char** argv, bool NoticeUser) { return true; }
};

extern "C" CModuleFar* bncGetObject(void) {
	return (CModuleFar*)new CHelloClass();
}
