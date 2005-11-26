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

#ifndef MKCONFIG
	if (m_Settings == NULL) {
		LOGERROR("new operator failed.");

		g_Bouncer->Fatal();
	}
#endif

	m_Settings->RegisterValueDestructor(string_free);

	if (Filename) {
		m_File = strdup(Filename);
		ParseConfig(Filename);
	} else
		m_File = NULL;
}

bool CBouncerConfig::ParseConfig(const char* Filename) {
	char Line[4096];
	char* dupEq;

	if (!Filename)
		return false;

	FILE* Conf = fopen(Filename, "r");

	if (!Conf) {
#ifndef MKCONFIG
		LOGERROR("Config file %s could not be opened.", Filename);
#endif

		return false;
	}

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

			dupEq = strdup(++Eq);

			if (dupEq == NULL) {
#ifndef MKCONFIG
				if (g_Bouncer != NULL) {
					LOGERROR("strdup() failed. Config option lost (%s=%s).", Line, Eq);

					g_Bouncer->Fatal();
				} else {
#endif
					printf("CBouncerConfig::ParseConfig: strdup() failed. Config could not be parsed.");

					exit(0);
#ifndef MKCONFIG
				}
#endif

				continue;
			}

			if (m_Settings->Add(Line, dupEq) == false) {
				LOGERROR("CHashtable::Add failed. Config could not be parsed (%s, %s).", Line, Eq);

				g_Bouncer->Fatal();
			}
		}
	}

	fclose(Conf);

	m_WriteLock = false;

	return true;
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

bool CBouncerConfig::WriteInteger(const char* Setting, const int Value) {
	char ValueStr[50];

	snprintf(ValueStr, sizeof(ValueStr), "%d", Value);

	return WriteString(Setting, ValueStr);
}

bool CBouncerConfig::WriteString(const char* Setting, const char* Value) {
	bool RetVal;

	if (Value)
		RetVal = m_Settings->Add(Setting, strdup(Value));
	else
		RetVal = m_Settings->Remove(Setting);

	if (RetVal == false)
		return false;

	if (!m_WriteLock)
		if (Persist() == false)
			return false;

	return true;
}

bool CBouncerConfig::Persist(void) {
	if (!m_File)
		return false;

	FILE* Config = fopen(m_File, "w");

	if (Config) {
		int i = 0;
		while (xhash_t<char*>* P = m_Settings->Iterate(i++)) {
			if (P->Name && P->Value) {
				fprintf(Config, "%s=%s\n", P->Name, P->Value);
			}
		}

		fclose(Config);

		return true;
	} else {
#ifndef MKCONFIG
		LOGERROR("Config file %s could not be opened.", m_File);
#endif

		return false;
	}
}

const char* CBouncerConfig::GetFilename(void) {
	return m_File;
}

xhash_t<char*>* CBouncerConfig::Iterate(int Index) {
	return m_Settings->Iterate(Index);
}
