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

CKeyring::CKeyring(CBouncerConfig* Config) {
	m_Config = Config;
}

CKeyring::~CKeyring(void) {
}

const char* CKeyring::GetKey(const char* Channel) {
	char* Buf = (char*)malloc(5 + strlen(Channel));

	if (Buf == NULL) {
		g_Bouncer->Log("CKeyring::GetKey: malloc() failed. Key could not be retrieved.");

		return false;
	}

	snprintf(Buf, 5 + strlen(Channel), "key.%s", Channel);

	const char* Ret = m_Config->ReadString(Buf);

	free(Buf);

	return Ret;
}

bool CKeyring::AddKey(const char* Channel, const char* Key) {
	bool RetVal;
	char* Buf = (char*)malloc(5 + strlen(Channel));

	if (Buf == NULL) {
		g_Bouncer->Log("CKeyring::AddKey: malloc() failed. Key could not be added.");

		return false;
	}

	snprintf(Buf, 5 + strlen(Channel), "key.%s", Channel);

	RetVal = m_Config->WriteString(Buf, Key);

	free(Buf);

	return RetVal;
}

bool CKeyring::DeleteKey(const char* Channel) {
	bool RetVal;
	char* Buf = (char*)malloc(5 + strlen(Channel));

	if (Buf == NULL) {
		g_Bouncer->Log("CKeyring::DeleteKey: malloc() failed. Key could not be removed.");

		return false;
	}

	snprintf(Buf, 5 + strlen(Channel), "key.%s", Channel);

	RetVal = m_Config->WriteString(Buf, NULL);

	free(Buf);

	return RetVal;
}
