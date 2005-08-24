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

CBouncerConfig::CBouncerConfig(const char* Filename) {
	m_WriteLock = false;
	m_Settings = new CHashtable<char*, false, 8>();

	m_Settings->RegisterValueDestructor(string_free);

	if (Filename) {
		m_File = strdup(Filename);
		ParseConfig(Filename);
	} else
		m_File = NULL;
}

void CBouncerConfig::ParseConfig(const char* Filename) {
	char Line[4096];

	if (!Filename)
		return;

	FILE* Conf = fopen(Filename, "r");

	if (!Conf)
		return;

	m_WriteLock = true;

	while (!feof(Conf)) {
		fgets(Line, sizeof(Line), Conf);

		if (Line[strlen(Line) - 1] == '\n')
			Line[strlen(Line) - 1] = '\0';

		if (Line[strlen(Line) - 1] == '\r')
			Line[strlen(Line) - 1] = '\0';

		char* Eq = strstr(Line, "=");

		if (Eq) {
			*Eq = '\0';

			m_Settings->Add(Line, strdup(++Eq));
		}
	}

	fclose(Conf);

	m_WriteLock = false;
}

CBouncerConfig::~CBouncerConfig() {
	free(m_File);
	delete m_Settings;
}

const char* CBouncerConfig::ReadString(const char* Setting) {
	return m_Settings->Get(Setting);
}

int CBouncerConfig::ReadInteger(const char* Setting) {
	const char* Value = m_Settings->Get(Setting);

	return Value ? atoi(Value) : 0;
}

void CBouncerConfig::WriteInteger(const char* Setting, const int Value) {
	char ValueStr[50];

	snprintf(ValueStr, sizeof(ValueStr), "%d", Value);

	WriteString(Setting, ValueStr);
}

void CBouncerConfig::WriteString(const char* Setting, const char* Value) {
	if (Value)
		m_Settings->Add(Setting, strdup(Value));
	else
		m_Settings->Remove(Setting);

	if (!m_WriteLock)
		Persist();
}

void CBouncerConfig::Persist(void) {
	if (!m_File)
		return;

	FILE* Config = fopen(m_File, "w");

	if (Config) {
		int i = 0;
		while (xhash_t<char*>* P = m_Settings->Iterate(i++)) {
			if (P->Name && P->Value) {
				fprintf(Config, "%s=%s\n", P->Name, P->Value);
			}
		}

		fclose(Config);
	}
}


const char* CBouncerConfig::GetFilename(void) {
	return m_File;
}

xhash_t<char*>* CBouncerConfig::Iterate(int Index) {
	return m_Settings->Iterate(Index);
}
