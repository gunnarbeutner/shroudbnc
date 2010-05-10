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
#include "LogBuffer.h"

CCore *g_Bouncer;
CHashtable<CHashtable<CLogBuffer *, false, 32> *, false, 64> *g_Logs;

bool LogAttachClientTimer(time_t Now, void *Cookie) {
	CClientConnection *Client = (CClientConnection *)Cookie;
	CIRCConnection *IRC = Client->GetOwner()->GetIRCConnection();

	if (IRC == NULL) {
		return false;
	}

	unsigned int i;
	hash_t<CLogBuffer *> *Log;
	CHashtable<CLogBuffer *, false, 32> *Logs = g_Logs->Get(Client->GetUser()->GetUsername());

	if (Logs == NULL) {
		return false;
	}

	i = 0;
	while ((Log = Logs->Iterate(i++)) != NULL) {
		if (IRC->GetChannel(Log->Name) != NULL) {
			Log->Value->PlayLog(Client, Log->Name, Play_Privmsg);
		}
	}

	return false;
}

class CLogClass : public CModuleImplementation {
	void Destroy(void) {
		delete this;
	}

	void Init(CCore *Core) {
		g_Logs = new CHashtable<CHashtable<CLogBuffer *, false, 32> *, false, 64>();
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
				Log->PlayLog(Client, ArgV[1], Play_Privmsg);
			}

			return true;
		}

		return false;
	}

	CLogBuffer *GetLog(CUser *User, const char *Channel) {
		CHashtable<CLogBuffer *, false, 32> *Logs;

		Logs = g_Logs->Get(User->GetUsername());

		if (Logs == NULL) {
			Logs = new CHashtable<CLogBuffer *, false, 32>();

			g_Logs->Add(User->GetUsername(), Logs);
		}

		CLogBuffer *Log;

		Log = Logs->Get(Channel);

		if (Log == NULL) {
			if (Logs->GetLength() >= 30) {
				return NULL;
			}

			Log = new CLogBuffer();

			Logs->Add(Channel, Log);
		}

		return Log;
	}

	bool InterceptClientMessage(CClientConnection* Client, int ArgC, const char **ArgV) {
		CIRCConnection *IRC;
		if (ArgC < 3) {
			return true;
		}

		if ((strcasecmp(ArgV[0], "PRIVMSG") != 0 && strcasecmp(ArgV[0], "NOTICE") != 0) || Client->GetOwner() == NULL) {
			return true;
		}

		IRC = Client->GetOwner()->GetIRCConnection();

		if (IRC == NULL || IRC->GetChannel(ArgV[1]) == NULL) {
			return true;
		}

		CLogBuffer *Log = GetLog(Client->GetOwner(), ArgV[1]);

		if (Log == NULL) {
			return true;
		}

		char *Mask;
		const utility_t *Utils;

		Utils = g_Bouncer->GetUtilities();

		if (Utils == NULL) {
			return true;
		}

		Utils->asprintf(&Mask, "%s!%s@%s", Client->GetNick(), Client->GetOwner()->GetUsername(), IRC->GetSite());
		Log->LogEvent(ArgV[0], Mask, ArgV[2]);
		Utils->Free(Mask);

		return true;
	}

	bool InterceptIRCMessage(CIRCConnection *Connection, int ArgC, const char **ArgV) {
		bool Pass = false;

		if (ArgC >= 4 && (strcasecmp(ArgV[1], "PRIVMSG") == 0)) {
			Pass = true;
		}

		if (ArgC >= 3 || (strcasecmp(ArgV[1], "JOIN") == 0 || strcasecmp(ArgV[1], "PART") == 0)) {
			Pass = true;
		}

		if (!Pass || Connection->GetChannel(ArgV[2]) == NULL) {
			return true;
		}

		CLogBuffer *Log = GetLog(Connection->GetOwner(), ArgV[2]);

		if (Log != NULL) {
			const char *Text;

			if (ArgC >= 3) {
				Text = ArgV[3];
			} else {
				Text = NULL;
			}

			Log->LogEvent(ArgV[1], ArgV[0], Text);
		}

		return true;
	}

	void AttachClient(CClientConnection *Client) {
		g_Bouncer->CreateTimer(0, false, LogAttachClientTimer, Client);
	}
};

extern "C" EXPORT CModuleFar* bncGetObject(void) {
	return (CModuleFar *)new CLogClass();
}

extern "C" EXPORT int bncGetInterfaceVersion(void) {
	return INTERFACEVERSION;
}
