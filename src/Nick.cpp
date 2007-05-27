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

#include "StdAfx.h"

/**
 * CNick
 *
 * Constructs a new nick object.
 *
 * @param Nick the nickname of the user
 * @param Owner the owning channel of this nick object
 */
CNick::CNick(const char *Nick, CChannel *Owner, safe_box_t Box) {
	assert(Nick != NULL);

	SetOwner(Owner);
	SetBox(Box);

	m_Nick = ustrdup(Nick);

	CHECK_ALLOC_RESULT(m_Nick, ustrdup) { } CHECK_ALLOC_RESULT_END;

	m_Prefixes = NULL;
	m_Site = NULL;
	m_Realname = NULL;
	m_Server = NULL;
	m_Creation = 0;
	m_IdleSince = 0;

	if (GetBox() != NULL) {
		safe_set_ro(Box, 1);


		SetPrefixes(safe_get_string(Box, "Prefixes"));
		SetSite(safe_get_string(Box, "Site"));
		SetRealname(safe_get_string(Box, "Realname"));
		SetServer(safe_get_string(Box, "Server"));
		m_Creation = safe_get_integer(Box, "CreationTimestamp");
		m_IdleSince = safe_get_integer(Box, "IdleTimestamp");

		safe_set_ro(Box, 0);
	}

	if (m_Creation == 0) {
		m_Creation = g_CurrentTime;
		safe_put_integer(GetBox(), "CreationTimestamp", m_Creation);
	}

	if (m_IdleSince == 0) {
		m_IdleSince = m_Creation;
		safe_put_integer(GetBox(), "IdleTimestamp", m_IdleSince);
	}
}

/**
 * ~CNick
 *
 * Destroys a nick object.
 */
CNick::~CNick() {
	ufree(m_Nick);
	ufree(m_Prefixes);
	ufree(m_Site);
	ufree(m_Realname);
	ufree(m_Server);

	for (unsigned int i = 0; i < m_Tags.GetLength(); i++) {
		ufree(m_Tags[i].Name);
		ufree(m_Tags[i].Value);
	}
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

	NewNick = ustrdup(Nick);

	CHECK_ALLOC_RESULT(m_Nick, ustrdup) {
		return false;
	} CHECK_ALLOC_RESULT_END;

	if (GetBox() != NULL) {
		// TODO: is this sane?
		/*safe_box_t Parent = safe_get_parent(GetBox());

		if (Parent != NULL) {
			safe_rename(Parent, m_Nick, Nick);
		}*/

		safe_put_string(GetBox(), "Nick", Nick);
	}

	ufree(m_Nick);
	m_Nick = NewNick;

	return true;
}

/**
 * GetNick
 *
 * Returns the current nick of the user.
 */
const char *CNick::GetNick(void) const {
	return m_Nick;
}

/**
 * IsOp
 *
 * Returns whether the user is an op.
 */
bool CNick::IsOp(void) const {
	return HasPrefix('@');
}

/**
 * IsVoice
 *
 * Returns whether the user is voiced.
 */
bool CNick::IsVoice(void) const {
	return HasPrefix('+');
}

/**
 * IsHalfop
 *
 * Returns whether the user is a half-op.
 */
bool CNick::IsHalfop(void) const {
	return HasPrefix('%');
}

/**
 * HasPrefix
 *
 * Returns whether the user has the given prefix.
 *
 * @param Prefix the prefix (e.g. @, +)
 */
bool CNick::HasPrefix(char Prefix) const {
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
	size_t LengthPrefixes = m_Prefixes ? strlen(m_Prefixes) : 0;

	Prefixes = (char *)urealloc(m_Prefixes, LengthPrefixes + 2);

	CHECK_ALLOC_RESULT(Prefixes, realloc) {
		return false;
	} CHECK_ALLOC_RESULT_END;

	m_Prefixes = Prefixes;
	m_Prefixes[LengthPrefixes] = Prefix;
	m_Prefixes[LengthPrefixes + 1] = '\0';

	if (GetBox() != NULL) {
		safe_put_string(GetBox(), "Prefixes", m_Prefixes);
	}

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
	size_t LengthPrefixes;

	if (m_Prefixes == NULL) {
		return true;
	}

	LengthPrefixes = strlen(m_Prefixes);

	char *Copy = (char *)umalloc(LengthPrefixes + 1);

	CHECK_ALLOC_RESULT(Copy, umalloc) {
		return false;
	} CHECK_ALLOC_RESULT_END;

	for (unsigned int i = 0; i < LengthPrefixes; i++) {
		if (m_Prefixes[i] != Prefix) {
			Copy[a++] = m_Prefixes[i];
		}
	}

	Copy[a] = '\0';

	ufree(m_Prefixes);
	m_Prefixes = Copy;

	if (GetBox() != NULL) {
		safe_put_string(GetBox(), "Prefixes", m_Prefixes);
	}

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
		dupPrefixes = ustrdup(Prefixes);

		CHECK_ALLOC_RESULT(dupPrefixes, ustrdup) {
			return false;
		} CHECK_ALLOC_RESULT_END;
	} else {
		dupPrefixes = NULL;
	}

	ufree(m_Prefixes);
	m_Prefixes = dupPrefixes;

	if (GetBox() != NULL) {
		safe_put_string(GetBox(), "Prefixes", m_Prefixes);
	}

	return true;
}

/**
 * GetPrefixes
 *
 * Returns all prefixes for a user.
 */
const char *CNick::GetPrefixes(void) const {
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
	DuplicateValue = ustrdup(NewValue); \
\
	if (DuplicateValue == NULL) { \
		LOGERROR("ustrdup() failed. New " #Name " was lost (%s, %s).", m_Nick, NewValue); \
\
		return false; \
	} else { \
		ufree(Name); \
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
	if (GetBox() != NULL) {
		safe_put_string(GetBox(), "Site", Site);
	}

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
	if (GetBox() != NULL) {
		safe_put_string(GetBox(), "Realname", Realname);
	}

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
	if (GetBox() != NULL) {
		safe_put_string(GetBox(), "Server", Server);
	}

	IMPL_NICKSET(m_Server, Server, true);
}

/**
 * InternalGetSite
 *
 * Returns the user's site without querying other nick
 * objects if the information is not available.
 */
const char *CNick::InternalGetSite(void) const {
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
const char *CNick::InternalGetRealname(void) const {
	return m_Realname;
}

/**
 * InternalGetServer
 *
 * Returns the user's server without querying other nick
 * objects if the information is not available.
 */
const char *CNick::InternalGetServer(void) const {
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
	while (hash_t<CChannel *> *Chan = GetOwner()->GetOwner()->GetChannels()->Iterate(a++)) { \
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
const char *CNick::GetSite(void) const {
	IMPL_NICKACCESSOR(InternalGetSite)
}

/**
 * GetRealname
 *
 * Returns the user's realname.
 */
const char *CNick::GetRealname(void) const {
	IMPL_NICKACCESSOR(InternalGetRealname)
}

/**
 * GetServer
 *
 * Returns the user's server.
 */
const char *CNick::GetServer(void) const {
	IMPL_NICKACCESSOR(InternalGetServer)
}

/**
 * GetChanJoin
 *
 * Returns a timestamp which determines when
 * the user joined the channel.
 */
time_t CNick::GetChanJoin(void) const {
	return m_Creation;
}

/**
 * GetIdleSince
 *
 * Returns a timestamp which determines when
 * the user last said something.
 */
time_t CNick::GetIdleSince(void) const {
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

	if (GetBox() != NULL) {
		safe_put_integer(GetBox(), "IdleTimestamp", Time);
	}

	return true;
}

/**
 * GetTag
 *
 * Returns the value of a user-specific tag.
 *
 * @param Name the name of the tag
 */
const char *CNick::GetTag(const char *Name) const {
	for (unsigned int i = 0; i < m_Tags.GetLength(); i++) {
		if (strcasecmp(m_Tags[i].Name, Name) == 0) {
			return m_Tags[i].Value;
		}
	}

	return NULL;
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
	nicktag_t NewTag;

	if (Name == NULL) {
		return false;
	}

	for (unsigned int i = 0; i < m_Tags.GetLength(); i++) {
		if (strcasecmp(m_Tags[i].Name, Name) == 0) {
			ufree(m_Tags[i].Name);
			ufree(m_Tags[i].Value);

			m_Tags.Remove(i);

			break;
		}
	}

	if (Value == NULL) {
		return true;
	}

	NewTag.Name = ustrdup(Name);

	CHECK_ALLOC_RESULT(NewTag.Name, ustrdup) {
		return false;
	} CHECK_ALLOC_RESULT_END;

	NewTag.Value = ustrdup(Value);

	CHECK_ALLOC_RESULT(NewTag.Value, ustrdup) {
		ufree(NewTag.Name);

		return false;
	} CHECK_ALLOC_RESULT_END;

	return m_Tags.Insert(NewTag);
}
