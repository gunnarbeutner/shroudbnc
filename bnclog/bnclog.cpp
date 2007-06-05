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
#include "LogBuffer.h"

CCore *g_Bouncer;
CHashtable<CHashtable<CLogBuffer *, false, 32> *, false, 64> *g_Logs;

class CLogClass : public CModuleImplementation {
	CLogBuffer *buf;

	void Destroy(void) {
		delete this;
	}

	void Init(CCore *Core) {
		g_Logs = new CHashtable<CHashtable<CLogBuffer *, false, 32> *, false, 64>();

		buf = new CLogBuffer();

		buf->LogEvent("JOIN", "Zyberdog!zyberdog@doghouse", NULL);
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PRIVMSG", "Zyberdog!zyberdog@doghouse", "hello world");
		buf->LogEvent("PART", "Zyberdog!zyberdog@doghouse", NULL);
	}

	bool InterceptClientCommand(CClientConnection *Client, const char *Subcommand, int ArgC, const char **ArgV, bool NoticeUser) {
		if (Client != NULL && strcasecmp(Subcommand, "chanlog") == 0 && ArgC >= 2) {
			CHashtable<CLogBuffer *, false, 32> *Logs = g_Logs->Get(Client->GetUser()->GetUsername());
			CLogBuffer *Log = NULL;
			
			if (Logs != NULL) {
				Log = Logs->Get(ArgV[1]);
			}

			if (Log == NULL) {
				if (NoticeUser) {
					Client->RealNotice("There is no log for that channel.");
				} else {
					Client->Privmsg("There is no log for that channel.");
				}
			} else {
				Log->PlayLog(Client, ArgV[1]);
			}

			return true;
		}

		return false;
	}

	bool InterceptIRCMessage(CIRCConnection *Connection, int ArgC, const char **ArgV) {
		bool Pass = false;

		if (ArgC >= 4 && (strcasecmp(ArgV[1], "PRIVMSG") == 0)) {
			Pass = true;
		}

		if (ArgC >= 3 || (strcasecmp(ArgV[1], "JOIN") == 0 || strcasecmp(ArgV[1], "PART") == 0)) {
			Pass = true;
		}

		if (!Pass) {
			return true;
		}

		CHashtable<CLogBuffer *, false, 32> *Logs;

		Logs = g_Logs->Get(Connection->GetUser()->GetUsername());

		if (Logs == NULL) {
			Logs = new CHashtable<CLogBuffer *, false, 32>();

			g_Logs->Add(Connection->GetUser()->GetUsername(), Logs);
		}

		CLogBuffer *Log;

		Log = Logs->Get(ArgV[2]);

		if (Log == NULL) {
			if (Logs->GetLength() >= 30) {
				return true;
			}

			Log = new CLogBuffer();

			Logs->Add(ArgV[2], Log);
		}

		Log->LogEvent(ArgV[1], ArgV[0], ArgV[3]);

		return true;
	}
};

extern "C" EXPORT CModuleFar* bncGetObject(void) {
	return (CModuleFar *)new CLogClass();
}

extern "C" EXPORT int bncGetInterfaceVersion(void) {
	return INTERFACEVERSION;
}
