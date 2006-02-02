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

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIdentSupport::CIdentSupport(void) {
	m_Ident = NULL;
}

CIdentSupport::~CIdentSupport(void) {
	free(m_Ident);
}

void CIdentSupport::SetIdent(const char *Ident) {
	char *NewIdent;

	NewIdent = strdup(Ident);

	if (NewIdent == NULL) {
		LOGERROR("strdup failed. Could not set new ident.");

		return;
	}

	free(m_Ident);
	m_Ident = NewIdent;

	Update();
}

const char *CIdentSupport::GetIdent(void) {
	return m_Ident;
}

void CIdentSupport::Update(void) {
//#ifndef _WIN32
	char *homedir = getenv("HOME");

	if (homedir) {
		char *Filename;

		asprintf(&Filename, "%s/.oidentd.conf", homedir);

		FILE *identConfig = fopen(Filename, "w");

		if (identConfig) {
			int i = 0;
			while (hash_t<CUser *> *User = g_Bouncer->GetUsers()->Iterate(i++)) {
				CIRCConnection *IRC = User->Value->GetIRCConnection();

				if (IRC == NULL) {
					continue;
				}

				int LocalPort = htons(IRC->GetLocalAddress().sin_port);
				int RemotePort = IRC->GetOwner()->GetPort();
				const char *Ident = User->Value->GetIdent();

				fprintf(identConfig, "fport %d lport %d { reply \"%s\" }\n", RemotePort, LocalPort, Ident ? Ident : User->Name);
			}

			fprintf(identConfig, "global { reply \"%s\" }\n", m_Ident);

			fclose(identConfig);
		}

		free(Filename);
	}
//#endif
}