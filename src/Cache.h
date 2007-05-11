/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007 Gunnar Beutner                                      *
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

#define CACHE(Name) struct configcache##Name

#define DEFINE_CACHE(Name) CACHE(Name) { \
	CConfig *BgConfig; \
	const char *BgPrefix;
#define END_DEFINE_CACHE };

#define DEFINE_OPTION_INT(Name) int Name
#define DEFINE_OPTION_STRING(Name) const char *Name

#define CacheInitialize(Cache, Config, Prefix) { \
	memset(&(Cache), 0xFF, sizeof((Cache))); \
	(Cache).BgConfig = Config; \
	(Cache).BgPrefix = Prefix; \
	}

#ifndef SWIG
int CacheGetIntegerReal(CConfig *Config, int *CacheValue, const char *Option, const char *Prefix);
const char *CacheGetStringReal(CConfig *Config, const char **CacheValue, const char *Option, const char *Prefix);

void CacheSetIntegerReal(CConfig *Config, int *CacheValue, const char *Option, int Value, const char *Prefix);
void CacheSetStringReal(CConfig *Config, const char **CacheValue, const char *Option, const char *Value, const char *Prefix);

#define CacheGetInteger(Cache, Option) (((Cache).Option == -1 || !(Cache).BgConfig->CanUseCache()) ? CacheGetIntegerReal((Cache).BgConfig, &((Cache).Option), #Option, (Cache).BgPrefix) : (Cache).Option)
#define CacheGetString(Cache, Option) (((Cache).Option == (char *)-1 || !(Cache).BgConfig->CanUseCache())? CacheGetStringReal((Cache).BgConfig, &((Cache).Option), #Option, (Cache).BgPrefix) : (Cache).Option)

#define CacheSetInteger(Cache, Option, Value) CacheSetIntegerReal((Cache).BgConfig, &((Cache).Option), #Option, Value, (Cache).BgPrefix)
#define CacheSetString(Cache, Option, Value) CacheSetStringReal((Cache).BgConfig, &((Cache).Option), #Option, Value, (Cache).BgPrefix)
#endif
