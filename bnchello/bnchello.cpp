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

#include "../src/StdAfx.h"

class CHelloClass : public CModuleImplementation {
	CUser *m_Bot;

	void Destroy(void) {
		GetCore()->RemoveUser(m_Bot->GetUsername());

		delete this;
	}

	void Init(CCore *Core) {
		m_Bot = GetCore()->CreateUser("bot", NULL);

		m_Bot->Lock();

		if (m_Bot->GetServer() == NULL) {
			m_Bot->SetNick("shroudbot");
			m_Bot->SetRealname("Hello World BNC module");
			m_Bot->SetServer("dk.quakenet.org");
			m_Bot->SetPort(6667);
			m_Bot->SetConfigChannels("#shroudtest2");
		}

		m_Bot->ScheduleReconnect();
	}

	bool InterceptIRCMessage(CIRCConnection *IRC, int argc, const char **argv) {
		CChannel *Channel;

		if (m_Bot->GetIRCConnection() == IRC) {
			if (argc >= 3 && strcasecmp(argv[1], "PRIVMSG") == 0 && strcasecmp(argv[3], "!hello") == 0) {
				const char *Other;

				if (strcasecmp(argv[2], IRC->GetCurrentNick()) == 0) {
					Other = IRC->NickFromHostmask(argv[0]);
				} else {
					Other = argv[2];
				}

				Channel = IRC->GetChannel("#shroudtest2");

				IRC->WriteLine("PRIVMSG %s :%s", Other, Channel ? Channel->GetChannelModes() : "unknown modes");

				if (Other != argv[2]) {
					IRC->FreeNick(const_cast<char*>(Other));
				}
			}
		}

		return true;
	}
};

extern "C" EXPORT CModuleFar* bncGetObject(void) {
	return (CModuleFar *)new CHelloClass();
}

extern "C" EXPORT int bncGetInterfaceVersion(void) {
	return INTERFACEVERSION;
}
