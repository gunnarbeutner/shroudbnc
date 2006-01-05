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

/**
 * CBouncerConfig
 *
 * Constructs a new configuration object and loads the given filename
 * if specified. If you specify NULL as the filename, a volatile
 * configuration object is constructed. Changes made to such an object
 * are not stored to disk.
 *
 * @param Filename the filename of the configuration file, can be NULL
 */
CBouncerConfig::CBouncerConfig(const char *Filename) {
	m_WriteLock = false;
	m_Settings = new CHashtable<char *, false, 8>();

	if (m_Settings == NULL && g_Bouncer != NULL) {
		LOGERROR("new operator failed.");

		g_Bouncer->Fatal();
	}

	m_Settings->RegisterValueDestructor(string_free);

	if (Filename) {
		m_File = strdup(Filename);
		ParseConfig();
	} else
		m_File = NULL;
}

/**
 * ParseConfig
 *
 * Parses a configuration file. Valid lines of the configuration file
 * have this syntax:
 *
 * setting=value
 */
bool CBouncerConfig::ParseConfig(void) {
	char Line[4096];
	char* dupEq;

	if (m_File == NULL)
		return false;

	FILE* Conf = fopen(m_File, "r");

	if (!Conf)
		return false;

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
				if (g_Bouncer != NULL) {
					LOGERROR("strdup() failed. Config option lost (%s=%s).",
						Line, Eq);

					g_Bouncer->Fatal();
				} else {
					printf("CBouncerConfig::ParseConfig: strdup() failed."
						" Config could not be parsed.");

					exit(0);
				}

				continue;
			}

			if (m_Settings->Add(Line, dupEq) == false) {
				LOGERROR("CHashtable::Add failed. Config could not be parsed"
					" (%s, %s).", Line, Eq);

				g_Bouncer->Fatal();
			}
		}
	}

	fclose(Conf);

	m_WriteLock = false;

	return true;
}

/**
 * ~CBouncerConfig
 *
 * Destructs the configuration object.
 */
CBouncerConfig::~CBouncerConfig() {
	free(m_File);
	delete m_Settings;
}

/**
 * ReadString
 *
 * Reads a configuration setting as a string. If the specified setting does
 * not exist, NULL is returned.
 *
 * @param Setting the configuration setting
 */
const char *CBouncerConfig::ReadString(const char *Setting) {
	char *Value = m_Settings->Get(Setting);

	if (Value && *Value)
		return Value;
	else
		return NULL;
}

/**
 * ReadInteger
 *
 * Reads a configuration setting as an integer. If the specified setting does
 * not exist, 0 is returned.
 *
 * @param Setting the configuration setting
 */
int CBouncerConfig::ReadInteger(const char *Setting) {
	const char *Value = m_Settings->Get(Setting);

	return Value ? atoi(Value) : 0;
}

/**
 * WriteString
 *
 * Set a configuration setting.
 *
 * @param Setting the configuration setting
 * @param Value the new value for the setting, can be NULL to indicate that
 *              the configuration setting is to be removed
 */
bool CBouncerConfig::WriteString(const char *Setting, const char *Value) {
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

/**
 * WriteInteger
 *
 * Sets a configuration setting.
 *
 * @param Setting the configuration setting
 * @param Value the new value for the setting
 */
bool CBouncerConfig::WriteInteger(const char *Setting, const int Value) {
	char ValueStr[50];

	snprintf(ValueStr, sizeof(ValueStr), "%d", Value);

	return WriteString(Setting, ValueStr);
}

/**
 * Persist
 *
 * Saves changes which have been made to the configuration object to disk
 * unless the configuration object is volatile.
 */
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
		if (g_Bouncer != NULL) {
			LOGERROR("Config file %s could not be opened.", m_File);
		} else {
			printf("Config file %s could not be opened.", m_File);
		}

		return false;
	}
}

/**
 * GetFilename
 *
 * Returns the filename of the configuration object. The return value will
 * be NULL if the configuration object is volatile.
 */
const char *CBouncerConfig::GetFilename(void) {
	return m_File;
}

/**
 * Iterate
 *
 * Iterates through the configuration object's settings.
 *
 * @param Index specifies the index of the setting which is to be returned
 */
xhash_t<char *> *CBouncerConfig::Iterate(int Index) {
	return m_Settings->Iterate(Index);
}
