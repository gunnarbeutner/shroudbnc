/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2011 Gunnar Beutner                                      *
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
CKeyring::CKeyring(CConfig *Config, CUser *Owner) {
	SetOwner(Owner);

	m_Config = Config;
}

/**
 * SetKey
 *
 * Saves or removes a key for a specific channel in the keyring. Returns true if the key
 * was successfully set, false otherwise.
 *
 * @param Channel the channel
 * @param Key the key
 */
RESULT<bool> CKeyring::SetKey(const char *Channel, const char *Key) {
	bool ReturnValue;
	char *Setting;

	if (!RemoveRedundantKeys()) {
		THROW(bool, Generic_QuotaExceeded, "Too many keys.");
	}

	int rc = asprintf(&Setting, "key.%s", Channel);

	if (RcFailed(rc)) {
		THROW(bool, Generic_OutOfMemory, "Out of memory.");
	}

	ReturnValue = m_Config->WriteString(Setting, Key);

	free(Setting);

	RETURN(bool, ReturnValue);
}

/**
 * GetKey
 *
 * Returns the key for the specified channel or NULL if there is no key
 * or the key is unknown.
 *
 * @param Channel the channel for which the key should be retrieved
 */
RESULT<const char *> CKeyring::GetKey(const char *Channel) {
	char *Setting;
	const char *ReturnValue;

	int rc = asprintf(&Setting, "key.%s", Channel);

	if (RcFailed(rc)) {
		THROW(const char *, Generic_OutOfMemory, "Out of memory.");
	}

	ReturnValue = m_Config->ReadString(Setting);

	free(Setting);

	RETURN(const char *, ReturnValue);
}

/**
 * RemoveRedundantKeys
 *
 * Removes obsolete keys from a keyring.
 */
bool CKeyring::RemoveRedundantKeys(void) {
	int i, Count = 0;
	const char *Channel, *Key;
	char **Keys;

	if (GetUser()->GetIRCConnection() == NULL) {
		return false;
	}

	Keys = m_Config->GetInnerHashtable()->GetSortedKeys();

	i = 0;
	while ((Key = Keys[i++]) != NULL) {
		if (strstr(Key, "key.") == Key) {
			Count++;
		}
	}

	if (!GetUser()->IsAdmin() && Count >= g_Bouncer->GetResourceLimit("keys")) {
		i = 0;
		while ((Key = Keys[i++]) != NULL) {
			if (strstr(Key, "key.") == Key) {
				Channel = Key + strlen("key.");

				if (GetUser()->GetIRCConnection()->GetChannel(Channel) != NULL) {
					m_Config->WriteString(Key, NULL);
					Count--;
				}
			}
		}

		if (Count >= g_Bouncer->GetResourceLimit("keys")) {
			free(Keys);

			return false;
		}
	}

	free(Keys);

	return true;
}
