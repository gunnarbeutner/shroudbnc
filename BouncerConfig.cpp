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
#include "BouncerConfig.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CBouncerConfig::CBouncerConfig(const char* Filename) {
	m_Settings = NULL;
	m_SettingsCount = 0;
	m_WriteLock = false;

	if (Filename) {
		m_File = strdup(Filename);
		ParseConfig(Filename);
	} else
		m_File = NULL;
}

void CBouncerConfig::ParseConfig(const char* Filename) {
	char Line[4096];

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

			WriteString(Line, ++Eq);
		}
	}

	fclose(Conf);

	m_WriteLock = false;
}

CBouncerConfig::~CBouncerConfig() {
	for (int i = 0; i < m_SettingsCount; i++) {
		free(m_Settings[i].Name);
		free(m_Settings[i].Value);
	}

	free(m_Settings);
	free(m_File);
}

void CBouncerConfig::ReadString(const char* Setting, char** Out) {
	config_t* Ptr = FindSetting(Setting);

	if (Ptr)
		*Out = Ptr->Value;
	else
		*Out = NULL;
}

void CBouncerConfig::ReadInteger(const char* Setting, int* Out) {
	config_t* Ptr = FindSetting(Setting);

	if (!Ptr) {
		*Out = 0;
		return;
	}

	char* Val = Ptr->Value;

	*Out = atoi(Val);
}

void CBouncerConfig::WriteInteger(const char* Setting, const int Value) {
	char ValueStr[50];

	snprintf(ValueStr, sizeof(ValueStr), "%d", Value);

	WriteString(Setting, ValueStr);
}

void CBouncerConfig::WriteString(const char* Setting, const char* Value) {
	config_t* Ptr;

	Ptr = FindSetting(Setting);

	if (!Ptr)
		Ptr = NewConfigT(Setting);
	else
		strcpy(Ptr->Name, Setting);

	free(Ptr->Value);

	if (Value)
		Ptr->Value = strdup(Value);
	else {
		Ptr->Name = NULL;
		Ptr->Value = NULL;
	}

	if (!m_WriteLock)
		Persist();
}

config_t* CBouncerConfig::FindSetting(const char* Setting) {
	for (int i = 0; i < m_SettingsCount; i++) {
		if (m_Settings[i].Name && strcmpi(m_Settings[i].Name, Setting) == 0) {
			return &m_Settings[i];
		}
	}

	return NULL;
}

config_t* CBouncerConfig::NewConfigT(const char* Setting) {
	m_Settings = (config_t*)realloc(m_Settings, sizeof(config_t) * ++m_SettingsCount);

	m_Settings[m_SettingsCount - 1].Name = (char*)malloc(strlen(Setting) + 1);
	strcpy(m_Settings[m_SettingsCount - 1].Name, Setting);
	m_Settings[m_SettingsCount - 1].Value = NULL;

	return &m_Settings[m_SettingsCount - 1];
}

void CBouncerConfig::Persist(void) {
	if (!m_File)
		return;

	FILE* Config = fopen(m_File, "w");

	if (Config) {
		for (int i = 0; i < m_SettingsCount; i++) {
			if (m_Settings[i].Name && m_Settings[i].Value) {
				fprintf(Config, "%s=%s\n", m_Settings[i].Name, m_Settings[i].Value);
			}
		}

		fclose(Config);
	}
}


const char* CBouncerConfig::GetFilename(void) {
	return m_File;
}

config_t* CBouncerConfig::GetSettings(void) {
	return m_Settings;
}

int CBouncerConfig::GetSettingCount(void) {
	return m_SettingsCount;
}
