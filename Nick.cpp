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

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CNick::CNick(const char* Nick) {
	m_Nick = strdup(Nick);
	m_Prefixes = (char*)malloc(1);
	m_Prefixes[0] = '\0';
}

CNick::~CNick() {
	free(m_Nick);
	free(m_Prefixes);
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
	m_Prefixes = (char*)realloc(m_Prefixes, strlen(m_Prefixes) + 2);

	m_Prefixes[strlen(m_Prefixes) - 1] = Prefix;
	m_Prefixes[strlen(m_Prefixes)] = '\0';
}

void CNick::RemovePrefix(char Prefix) {
	int a = 0;
	char* Copy = (char*)malloc(strlen(m_Prefixes) + 1);

	for (unsigned int i = 0; i < strlen(m_Prefixes); i++) {
		if (m_Prefixes[i] != Prefix)
			Copy[a++] = '\0';
	}

	Copy[a] = '\0';
}

void CNick::SetPrefixes(const char* Prefixes) {
	free(m_Prefixes);

	if (Prefixes)
		m_Prefixes = strdup(Prefixes);
	else {
		m_Prefixes = (char*)malloc(1);
		m_Prefixes[0] = '\0';
	}
}

const char* CNick::GetPrefixes(void) {
	return m_Prefixes;
}