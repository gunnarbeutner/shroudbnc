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
#include "Nick.h"
#include "BouncerConfig.h"

void DestroyCNick(CNick* P) {
	delete P;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CNick::CNick(const char* Nick) {
	m_Nick = strdup(Nick);
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

void CNick::AddPrefix(char Prefix) {
	int n = m_Prefixes ? strlen(m_Prefixes) : 0;

	m_Prefixes = (char*)realloc(m_Prefixes, n + 2);

	m_Prefixes[n] = Prefix;
	m_Prefixes[n + 1] = '\0';
}

void CNick::RemovePrefix(char Prefix) {
	int a = 0;

	if (!m_Prefixes)
		return;

	char* Copy = (char*)malloc(strlen(m_Prefixes) + 1);

	for (unsigned int i = 0; i < strlen(m_Prefixes); i++) {
		if (m_Prefixes[i] != Prefix)
			Copy[a++] = m_Prefixes[i];
	}

	Copy[a] = '\0';

	free(m_Prefixes);
	m_Prefixes = Copy;
}

void CNick::SetPrefixes(const char* Prefixes) {
	free(m_Prefixes);

	if (Prefixes)
		m_Prefixes = strdup(Prefixes);
	else
		m_Prefixes = NULL;
}

const char* CNick::GetPrefixes(void) {
	return m_Prefixes;
}

void CNick::SetSite(const char* Site) {
	free(m_Site);

	m_Site = strdup(Site);
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

void CNick::SetNick(const char* Nick) {
	free(m_Nick);

	m_Nick = strdup(Nick);
}

time_t CNick::GetChanJoin(void) {
	return m_Creation;
}

time_t CNick::GetIdleSince(void) {
	return m_IdleSince;
}

void CNick::SetIdleSince(time_t Time) {
	m_IdleSince = Time;
}

void CNick::SetTag(const char* Name, const char* Value) {
	if (m_Tags == NULL)
		m_Tags = new CBouncerConfig(NULL);

	m_Tags->WriteString(Name, Value);
}

const char* CNick::GetTag(const char* Name) {
	if (m_Tags == NULL)
		return NULL;
	else
		return m_Tags->ReadString(Name);
}
