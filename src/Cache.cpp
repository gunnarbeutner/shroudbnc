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

int CacheGetIntegerReal(CConfig *Config, int *CacheValue, const char *Option, const char *Prefix) {
	char *OptionName;

	if (Prefix != NULL) {
		int rc = asprintf(&OptionName, "%s%s", Prefix, Option);

		if (RcFailed(rc)) {
			return 0;
		}
	} else {
		OptionName = const_cast<char *>(Option);
	}

	*CacheValue = Config->ReadInteger(OptionName);

	if (Prefix != NULL) {
		free(OptionName);
	}

	return *CacheValue;
}

const char *CacheGetStringReal(CConfig *Config, const char **CacheValue, const char *Option, const char *Prefix) {
	char *OptionName;

	if (Prefix != NULL) {
		int rc = asprintf(&OptionName, "%s%s", Prefix, Option);

		if (RcFailed(rc)) {
			return NULL;
		}
	} else {
		OptionName = const_cast<char *>(Option);
	}

	*CacheValue = Config->ReadString(OptionName);

	if (Prefix != NULL) {
		free(OptionName);
	}

	return *CacheValue;
}

void CacheSetIntegerReal(CConfig *Config, int *CacheValue, const char *Option, int Value, const char *Prefix) {
	char *OptionName;

	if (Prefix != NULL) {
		int rc = asprintf(&OptionName, "%s%s", Prefix, Option);

		if (RcFailed(rc)) {
			return;
		}
	} else {
		OptionName = const_cast<char *>(Option);
	}

	*CacheValue = Value;

	Config->WriteInteger(OptionName, Value);

	if (Prefix != NULL) {
		free(OptionName);
	}
}

void CacheSetStringReal(CConfig *Config, const char **CacheValue, const char *Option, const char *Value, const char *Prefix) {
	char *OptionName;

	if (Prefix != NULL) {
		int rc = asprintf(&OptionName, "%s%s", Prefix, Option);

		if (RcFailed(rc)) {
			return;
		}
	} else {
		OptionName = const_cast<char *>(Option);
	}

	Config->WriteString(OptionName, Value);
	*CacheValue = Config->ReadString(OptionName);

	if (Prefix != NULL) {
		free(OptionName);
	}
}
