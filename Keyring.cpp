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
 * CKeyring
 *
 * Constructs a new keyring object which can be used for storing channel keys.
 *
 * @param Config the configuration object which should be used for storing
 *               the keys
 */
CKeyring::CKeyring(CBouncerConfig *Config) {
	m_Config = Config;
}

/**
 * GetKey
 *
 * Returns the key for the specified channel or NULL if there is no key
 * or the key is unknown.
 *
 * @param Channel the channel for which the key should be retrieved
 */
const char *CKeyring::GetKey(const char* Channel) {
	char *Buf = (char *)malloc(5 + strlen(Channel));

	if (Buf == NULL) {
		LOGERROR("malloc() failed. Key could not be retrieved (%s).", Channel);

		return NULL;
	}

	snprintf(Buf, 5 + strlen(Channel), "key.%s", Channel);

	const char *Ret = m_Config->ReadString(Buf);

	free(Buf);

	return Ret;
}

/**
 * AddKey
 *
 * Saves a key for a specific channel in the keyring. Returns true if the key
 * was successfully saves, false otherwise.
 *
 * @param Channel the channel
 * @param Key the key
 */
bool CKeyring::AddKey(const char *Channel, const char *Key) {
	bool RetVal;
	char *Buf = (char *)malloc(5 + strlen(Channel));

	if (Buf == NULL) {
		LOGERROR("malloc() failed. Key could not be added (%s, %s).",
			Channel, Key);

		return false;
	}

	snprintf(Buf, 5 + strlen(Channel), "key.%s", Channel);

	RetVal = m_Config->WriteString(Buf, Key);

	free(Buf);

	return RetVal;
}

/**
 * DeleteKey
 *
 * Removes a channel key from the keyring. Returns true if the key was
 * successfully removed, false otherwise.
 */
bool CKeyring::DeleteKey(const char *Channel) {
	bool RetVal;
	char *Buf = (char *)malloc(5 + strlen(Channel));

	if (Buf == NULL) {
		LOGERROR("malloc() failed. Key could not be removed (%s).", Channel);

		return false;
	}

	snprintf(Buf, 5 + strlen(Channel), "key.%s", Channel);

	RetVal = m_Config->WriteString(Buf, NULL);

	free(Buf);

	return RetVal;
}
