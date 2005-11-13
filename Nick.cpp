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

void DestroyCNick(CNick* P) {
	delete P;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CNick::CNick(const char* Nick) {
	assert(Nick != NULL);

	m_Nick = strdup(Nick);

	if (m_Nick == NULL && Nick) {
		LOGERROR("strdup() failed. Nick was lost (%s).", Nick);
	}

	m_Prefixes = NULL;
	m_Site = NULL;

	m_IdleSince = m_Creation = time(NULL);

	m_Tags = NULL;
}

CNick::~CNick() {
	free(m_Nick);
	free(m_Prefixes);
	free(m_Site);

	if (m_Tags)
		delete m_Tags;
}

const char* CNick::GetNick(void) {
	return m_Nick;
}

bool CNick::IsOp(void) {
	return HasPrefix('@');
}

bool CNick::IsVoice(void) {
	return HasPrefix('+');
}

bool CNick::IsHalfop(void) {
	return HasPrefix('%');
}

bool CNick::HasPrefix(char Prefix) {
	if (m_Prefixes && strchr(m_Prefixes, Prefix) != NULL)
		return true;
	else
		return false;
}

bool CNick::AddPrefix(char Prefix) {
	char* Prefixes;
	int n = m_Prefixes ? strlen(m_Prefixes) : 0;

	Prefixes = (char*)realloc(m_Prefixes, n + 2);

	if (Prefixes == NULL) {
		LOGERROR("realloc() failed. Prefixes might be inconsistent (%s, %c).", m_Nick, Prefix);

		return false;
	}

	m_Prefixes = Prefixes;
	m_Prefixes[n] = Prefix;
	m_Prefixes[n + 1] = '\0';

	return true;
}

bool CNick::RemovePrefix(char Prefix) {
	int a = 0;

	if (!m_Prefixes)
		return true;

	char* Copy = (char*)malloc(strlen(m_Prefixes) + 1);

	for (unsigned int i = 0; i < strlen(m_Prefixes); i++) {
		if (m_Prefixes[i] != Prefix)
			Copy[a++] = m_Prefixes[i];
	}

	Copy[a] = '\0';

	free(m_Prefixes);
	m_Prefixes = Copy;

	return true;
}

bool CNick::SetPrefixes(const char* Prefixes) {
	char* dupPrefixes;

	if (Prefixes) {
		dupPrefixes = strdup(Prefixes);

		if (dupPrefixes == NULL) {
			LOGERROR("strdup() failed. New prefixes were lost (%s, %s).", m_Nick, Prefixes);

			return false;
		} else {
			free(m_Prefixes);
			m_Prefixes = dupPrefixes;

			return true;
		}
	} else {
		free(m_Prefixes);
		m_Prefixes = NULL;

		return true;
	}
}

const char* CNick::GetPrefixes(void) {
	return m_Prefixes;
}

bool CNick::SetSite(const char* Site) {
	char* dupSite;

	dupSite = strdup(Site);

	if (dupSite == NULL) {
		LOGERROR("strdup() failed. New site was lost (%s, %s).", m_Nick, Site);

		return false;
	} else {
		free(m_Site);
		m_Site = dupSite;

		return true;
	}
}

const char* CNick::GetSite(void) {
	if (!m_Site)
		return NULL;

	char* Host = strstr(m_Site, "!");

	if (Host)
		return Host + 1;
	else
		return m_Site;
}

bool CNick::SetNick(const char* Nick) {
	assert(Nick != NULL);

	free(m_Nick);

	m_Nick = strdup(Nick);

	if (m_Nick == NULL && Nick) {
		LOGERROR("strdup() failed. Nick was lost (%s).", Nick);

		return false;
	} else
		return true;
}

time_t CNick::GetChanJoin(void) {
	return m_Creation;
}

time_t CNick::GetIdleSince(void) {
	return m_IdleSince;
}

bool CNick::SetIdleSince(time_t Time) {
	m_IdleSince = Time;

	return true;
}

bool CNick::SetTag(const char* Name, const char* Value) {
	if (m_Tags == NULL) {
		m_Tags = new CBouncerConfig(NULL);

		if (m_Tags == NULL) {
			LOGERROR("new operator failed. Tag was lost. (%s, %s)", Name, Value);

			return false;
		}
	}

	return m_Tags->WriteString(Name, Value);
}

const char* CNick::GetTag(const char* Name) {
	if (m_Tags == NULL)
		return NULL;
	else
		return m_Tags->ReadString(Name);
}
