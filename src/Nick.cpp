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
 * CNick
 *
 * Constructs a new nick object.
 *
 * @param Owner the owning channel of this nick object
 * @param Nick the nickname of the user
 */
CNick::CNick(CChannel *Owner, const char *Nick) {
	assert(Nick != NULL);

	m_Nick = strdup(Nick);

	CHECK_ALLOC_RESULT(m_Nick, strdup) { } CHECK_ALLOC_RESULT_END;

	m_Prefixes = NULL;
	m_Site = NULL;
	m_Realname = NULL;
	m_Server = NULL;

	time(&m_Creation);
	m_IdleSince = m_Creation;

	m_Tags = NULL;

	m_Owner = Owner;
}

/**
 * ~CNick
 *
 * Destroys a nick object.
 */
CNick::~CNick() {
	free(m_Nick);
	free(m_Prefixes);
	free(m_Site);
	free(m_Realname);
	free(m_Server);

	delete m_Tags;
}

/**
 * GetNick
 *
 * Returns the current nick of the user.
 */
const char *CNick::GetNick(void) {
	return m_Nick;
}

/**
 * SetNick
 *
 * Sets the user's nickname.
 *
 * @param Nick the new nickname
 */
bool CNick::SetNick(const char *Nick) {
	char *NewNick;

	assert(Nick != NULL);

	NewNick = strdup(Nick);

	CHECK_ALLOC_RESULT(m_Nick, strdup) {
		return false;
	} CHECK_ALLOC_RESULT_END;

	free(m_Nick);
	m_Nick = NewNick;

	return true;
}

/**
 * IsOp
 *
 * Returns whether the user is an op.
 */
bool CNick::IsOp(void) {
	return HasPrefix('@');
}

/**
 * IsVoice
 *
 * Returns whether the user is voiced.
 */
bool CNick::IsVoice(void) {
	return HasPrefix('+');
}

/**
 * IsHalfop
 *
 * Returns whether the user is a half-op.
 */
bool CNick::IsHalfop(void) {
	return HasPrefix('%');
}

/**
 * HasPrefix
 *
 * Returns whether the user has the given prefix.
 *
 * @param Prefix the prefix (e.g. @, +)
 */
bool CNick::HasPrefix(char Prefix) {
	if (m_Prefixes != NULL && strchr(m_Prefixes, Prefix) != NULL) {
		return true;
	} else {
		return false;
	}
}

/**
 * AddPrefix
 *
 * Adds a prefix to a user.
 *
 * @param Prefix the new prefix
 */
bool CNick::AddPrefix(char Prefix) {
	char *Prefixes;
	int LengthPrefixes = m_Prefixes ? strlen(m_Prefixes) : 0;

	Prefixes = (char *)realloc(m_Prefixes, LengthPrefixes + 2);

	CHECK_ALLOC_RESULT(Prefixes, realloc) {
		return false;
	} CHECK_ALLOC_RESULT_END;

	m_Prefixes = Prefixes;
	m_Prefixes[LengthPrefixes] = Prefix;
	m_Prefixes[LengthPrefixes + 1] = '\0';

	return true;
}

/**
 * RemovePrefix
 *
 * Removes the specified prefix from the user.
 *
 * @param Prefix the prefix
 */
bool CNick::RemovePrefix(char Prefix) {
	int a = 0;
	unsigned int LengthPrefixes;

	if (m_Prefixes == NULL) {
		return true;
	}

	LengthPrefixes = strlen(m_Prefixes);

	char *Copy = (char *)malloc(LengthPrefixes + 1);

	CHECK_ALLOC_RESULT(Copy, malloc) {
		return false;
	} CHECK_ALLOC_RESULT_END;

	for (unsigned int i = 0; i < LengthPrefixes; i++) {
		if (m_Prefixes[i] != Prefix) {
			Copy[a++] = m_Prefixes[i];
		}
	}

	Copy[a] = '\0';

	free(m_Prefixes);
	m_Prefixes = Copy;

	return true;
}

/**
 * SetPrefixes
 *
 * Sets the prefixes for a user.
 *
 * @param Prefixes the new prefixes
 */
bool CNick::SetPrefixes(const char *Prefixes) {
	char *dupPrefixes;

	if (Prefixes) {
		dupPrefixes = strdup(Prefixes);

		CHECK_ALLOC_RESULT(dupPrefixes, strdup) {
			return false;
		} CHECK_ALLOC_RESULT_END;
	} else {
		dupPrefixes = NULL;
	}

	free(m_Prefixes);
	m_Prefixes = dupPrefixes;

	return true;
}

/**
 * GetPrefixes
 *
 * Returns all prefixes for a user.
 */
const char *CNick::GetPrefixes(void) {
	return m_Prefixes;
}

/**
 * IMPL_NICKSET
 *
 * Implements a Set*() function
 *
 * @param Name the name of the attribute
 * @param NewValue the new value
 * @param Static indicates whether the attribute can be modified
 *		  once its initial value has been set
 */
#define IMPL_NICKSET(Name, NewValue, Static) \
	char *DuplicateValue; \
\
	if ((Static && Name != NULL) || NewValue == NULL) { \
		return false; \
	} \
\
	DuplicateValue = strdup(NewValue); \
\
	if (DuplicateValue == NULL) { \
		LOGERROR("strdup() failed. New " #Name " was lost (%s, %s).", m_Nick, NewValue); \
\
		return false; \
	} else { \
		free(Name); \
		Name = DuplicateValue; \
\
		return true; \
	}

/**
 * SetSite
 *
 * Sets the site (ident\@host) for a user.
 *
 * @param Site the user's new site
 */
bool CNick::SetSite(const char *Site) {
	IMPL_NICKSET(m_Site, Site, false);
}

/**
 * SetRealname
 *
 * Sets the user's realname.
 *
 * @param Realname the new realname
 */
bool CNick::SetRealname(const char *Realname) {
	IMPL_NICKSET(m_Realname, Realname, true);
}

/**
 * SetServer
 *
 * Sets the server for a user.
 *
 * @param Server the server which the user is using
 */
bool CNick::SetServer(const char *Server) {
	IMPL_NICKSET(m_Server, Server, true);
}

/**
 * InternalGetSite
 *
 * Returns the user's site without querying other nick
 * objects if the information is not available.
 */
const char *CNick::InternalGetSite(void) {
	if (m_Site == NULL) {
		return NULL;
	}

	char *Host = strstr(m_Site, "!");

	if (Host) {
		return Host + 1;
	} else {
		return m_Site;
	}
}

/**
 * InternalGetRealname
 *
 * Returns the user's realname without querying other nick
 * objects if the information is not available.
 */
const char *CNick::InternalGetRealname(void) {
	return m_Realname;
}

/**
 * InternalGetServer
 *
 * Returns the user's server without querying other nick
 * objects if the information is not available.
 */
const char *CNick::InternalGetServer(void) {
	return m_Server;
}

/**
 * IMPL_NICKACCESSOR
 *
 * Implements a Get*() function
 *
 * @param Name the name of the attribute
 */
#define IMPL_NICKACCESSOR(Name) \
	const char *Value; \
	int a = 0; \
\
	if ((Value = Name()) != NULL) { \
		return Value; \
	} \
\
	while (hash_t<CChannel *> *Chan = m_Owner->GetOwner()->GetChannels()->Iterate(a++)) { \
		if (!Chan->Value->HasNames()) \
			continue; \
\
		CNick *NickObj = Chan->Value->GetNames()->Get(m_Nick); \
\
		if (NickObj && strcasecmp(NickObj->GetNick(), m_Nick) == 0 && NickObj->Name() != NULL) \
			return NickObj->Name(); \
	} \
\
	return NULL;

/**
 * GetSite
 *
 * Returns the user's site.
 */
const char *CNick::GetSite(void) {
	IMPL_NICKACCESSOR(InternalGetSite)
}

/**
 * GetRealname
 *
 * Returns the user's realname.
 */
const char *CNick::GetRealname(void) {
	IMPL_NICKACCESSOR(InternalGetRealname)
}

/**
 * GetServer
 *
 * Returns the user's server.
 */
const char *CNick::GetServer(void) {
	IMPL_NICKACCESSOR(InternalGetServer)
}

/**
 * GetChanJoin
 *
 * Returns a timestamp which determines when
 * the user joined the channel.
 */
time_t CNick::GetChanJoin(void) {
	return m_Creation;
}

/**
 * GetIdleSince
 *
 * Returns a timestamp which determines when
 * the user last said something.
 */
time_t CNick::GetIdleSince(void) {
	return m_IdleSince;
}

/**
 * SetIdleSince
 *
 * Sets the timestamp of the user's last channel PRIVMSG.
 *
 * @param Time the new timestamp
 */
bool CNick::SetIdleSince(time_t Time) {
	m_IdleSince = Time;

	return true;
}

/**
 * GetTag
 *
 * Returns the value of a user-specific tag.
 *
 * @param Name the name of the tag
 */
const char *CNick::GetTag(const char *Name) {
	if (m_Tags == NULL) {
		return NULL;
	} else {
		return m_Tags->ReadString(Name);
	}
}

/**
 * SetTag
 *
 * Sets a user-specific tag.
 *
 * @param Name the name of the tag
 * @param Value the value of the tag
 */
bool CNick::SetTag(const char *Name, const char *Value) {
	if (m_Tags == NULL) {
		m_Tags = new CConfig(NULL);

		CHECK_ALLOC_RESULT(m_Tags, new) {
			return false;
		} CHECK_ALLOC_RESULT_END;
	}

	return m_Tags->WriteString(Name, Value);
}

/**
 * Freeze
 *
 * Persists a nick object.
 *
 * @param Box the box which is used for storing the object's data
 */
bool CNick::Freeze(CAssocArray *Box) {
	Box->AddString("~nick.nick", m_Nick);
	Box->AddString("~nick.prefixes", m_Prefixes);
	Box->AddString("~nick.site", m_Site);
	Box->AddString("~nick.realname", m_Realname);
	Box->AddString("~nick.server", m_Server);
	Box->AddInteger("~nick.creation", m_Creation);
	Box->AddInteger("~nick.idlets", m_IdleSince);

	if (m_Tags != NULL) {
		FreezeObject<CConfig>(Box, "~nick.tags", m_Tags);
		m_Tags = NULL;
	}

	delete this;

	return true;
}

/**
 * Unfreeze
 *
 * Creates a new nick object by reading its data from a box.
 *
 * @param Box the box
 * @param Owner the channel
 */
CNick *CNick::Unfreeze(CAssocArray *Box) {
	const char *Name;
	CNick *Nick;
	CConfig *Tags;

	Name = Box->ReadString("~nick.nick");

	Nick = new CNick(NULL, Name);

	CHECK_ALLOC_RESULT(Nick, new) {
		return NULL;
	} CHECK_ALLOC_RESULT_END;

	Nick->SetPrefixes(Box->ReadString("~nick.prefixes"));
	Nick->SetSite(Box->ReadString("~nick.site"));
	Nick->SetRealname(Box->ReadString("~nick.realname"));
	Nick->SetServer(Box->ReadString("~nick.server"));
	Nick->m_Creation = Box->ReadInteger("~nick.creation");
	Nick->m_IdleSince = Box->ReadInteger("~nick.idlets");

	Tags = UnfreezeObject<CConfig>(Box, "~nick.tags");

	if (Tags != NULL) {
		Nick->m_Tags = Tags;
	}

	return Nick;
}
