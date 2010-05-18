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

#include "StdAfx.h"

/**
 * CIdentSupport
 *
 * Constructs a new ident support object.
 */
CIdentSupport::CIdentSupport(void) {
	m_Ident = NULL;
}

/**
 * ~CIdentSupport
 *
 * Destructs an ident support object.
 */
CIdentSupport::~CIdentSupport(void) {
	free(m_Ident);
}

/**
 * SetIdent
 *
 * Sets the default ident for the bouncer.
 *
 * @param Ident the ident
 */
void CIdentSupport::SetIdent(const char *Ident) {
	char *NewIdent;

#ifndef _WIN32
	passwd *pwd;
	uid_t uid;
	char *FilenameTemp, *Filename;
	int rc;

	uid = getuid();

	pwd = getpwuid(uid);

	if (pwd == NULL) {
		g_Bouncer->Log("Could not figure out the UNIX username. Not setting ident.");

		return;
	}

	rc = asprintf(&Filename, "%s/.oidentd.conf", pwd->pw_dir);

	if (RcFailed(rc)) {
		return;
	}

	rc = asprintf(&FilenameTemp, "%s.tmp", Filename);

	if (RcFailed(rc)) {
		free(Filename);

		return;
	}

	FILE *identConfig = fopen(FilenameTemp, "w");

	SetPermissions(FilenameTemp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (identConfig != NULL) {
		char *Buf = (char *)malloc(strlen(Ident) + 50);

		snprintf(Buf, strlen(Ident) + 50, "global { reply \"%s\" }", Ident);
		fputs(Buf, identConfig);

		free(Buf);

		fclose(identConfig);
	}

	rc = rename(FilenameTemp, Filename);

	free(Filename);
	free(FilenameTemp);

	if (RcFailed(rc)) {}
#endif

	NewIdent = strdup(Ident);

	if (AllocFailed(NewIdent)) {
		return;
	}

	free(m_Ident);
	m_Ident = NewIdent;
}

/**
 * GetIdent
 *
 * Returns the default ident.
 */
const char *CIdentSupport::GetIdent(void) const {
	return m_Ident;
}
