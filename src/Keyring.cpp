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
	char *Setting = (char *)malloc(5 + strlen(Channel));

	if (Setting == NULL) {
		LOGERROR("malloc() failed. Key could not be retrieved (%s).", Channel);

		return NULL;
	}

	snprintf(Setting, 5 + strlen(Channel), "key.%s", Channel);

	const char *ReturnValue = m_Config->ReadString(Setting);

	free(Setting);

	return ReturnValue;
}

/**
 * AddKey
 *
 * Saves a key for a specific channel in the keyring. Returns true if the key
 * was successfully saved, false otherwise.
 *
 * @param Channel the channel
 * @param Key the key
 */
bool CKeyring::AddKey(const char *Channel, const char *Key) {
	bool ReturnValue;
	char *Setting = (char *)malloc(5 + strlen(Channel));

	if (Setting == NULL) {
		LOGERROR("malloc() failed. Key could not be added (%s, %s).",
			Channel, Key);

		return false;
	}

	snprintf(Setting, 5 + strlen(Channel), "key.%s", Channel);

	ReturnValue = m_Config->WriteString(Setting, Key);

	free(Setting);

	return ReturnValue;
}

/**
 * DeleteKey
 *
 * Removes a channel key from the keyring. Returns true if the key was
 * successfully removed, false otherwise.
 */
bool CKeyring::DeleteKey(const char *Channel) {
	bool ReturnValue;
	char *Setting = (char *)malloc(5 + strlen(Channel));

	if (Setting == NULL) {
		LOGERROR("malloc() failed. Key could not be removed (%s).", Channel);

		return false;
	}

	snprintf(Setting, 5 + strlen(Channel), "key.%s", Channel);

	ReturnValue = m_Config->WriteString(Setting, NULL);

	free(Setting);

	return ReturnValue;
}
