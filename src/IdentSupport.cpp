/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007 Gunnar Beutner                                           *
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
	char *homedir = NULL;
	passwd *pwd;
	uid_t uid;

	uid = getuid();

	pwd = getpwuid(uid);

	if (pwd != NULL) {
		homedir = strdup(pwd->pw_dir)
	}

	char *Out = (char *)malloc(strlen(homedir) + 50);

	if (Out == NULL) {
		LOGERROR("malloc failed. Could not set new ident (%s).", Ident);

		free(homedir);

		return;
	}

	if (homedir != NULL) {
		snprintf(Out, strlen(homedir) + 50, "%s/.oidentd.conf", homedir);

		free(homedir);

		FILE *identConfig = fopen(Out, "w");

		SetPermissions(Out, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

		if (identConfig != NULL) {
			char *Buf = (char *)malloc(strlen(Ident) + 50);

			snprintf(Buf, strlen(Ident) + 50, "global { reply \"%s\" }", Ident);
			fputs(Buf, identConfig);

			free(Buf);

			fclose(identConfig);
		}
	}

	free(Out);
#endif

	NewIdent = strdup(Ident);

	if (NewIdent == NULL) {
		LOGERROR("strdup failed. Could not set new ident.");

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
