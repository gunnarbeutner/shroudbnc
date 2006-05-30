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

class CChannel;
class CConfig;

/**
 * nicktag_t
 *
 * A tag which can be attached to nick objects.
 */
typedef struct nicktag_s {
	char *Name; /**< the name of the tag */
	char *Value; /**< the value of the tag */
} nicktag_t;

#ifdef SWIGINTERFACE
%template(CZoneObjectCNick) CZoneObject<class CNick, 1024>;
#endif

/**
 * CNick
 *
 * Represents a user on a single channel.
 */
class CNick : public CObject<CChannel>, public CZoneObject<CNick, 1024> {
	char *m_Nick; /**< the nickname of the user */
	char *m_Prefixes; /**< the user's prefixes (e.g. @, +) */
	char *m_Site; /**< the ident\@host of the user */
	char *m_Realname; /**< the realname of the user */
	char *m_Server; /**< the server this user is using */
	time_t m_Creation; /**< a timestamp, when this user object was created */
	time_t m_IdleSince; /**< a timestamp, when the user last said something */
	CVector<nicktag_t> m_Tags; /**< any tags which belong to this nick object */

	const char *InternalGetSite(void) const;
	const char *InternalGetRealname(void) const;
	const char *InternalGetServer(void) const;
public:
#ifndef SWIG
	CNick(const char *Nick, CChannel *Owner);
	virtual ~CNick(void);

	RESULT<bool> Freeze(CAssocArray *Box);
	static RESULT<CNick *> Thaw(CAssocArray *Box, CChannel *Owner);
#endif

	virtual bool SetNick(const char *Nick);
	virtual const char *GetNick(void) const;

	virtual bool IsOp(void) const;
	virtual bool IsVoice(void) const;
	virtual bool IsHalfop(void) const;

	virtual bool HasPrefix(char Prefix) const;
	virtual bool AddPrefix(char Prefix);
	virtual bool RemovePrefix(char Prefix);
	virtual bool SetPrefixes(const char *Prefixes);
	virtual const char *GetPrefixes(void) const;

	virtual bool SetSite(const char *Site);
	virtual const char *GetSite(void) const;

	virtual bool SetRealname(const char *Realname);
	virtual const char *GetRealname(void) const;

	virtual bool SetServer(const char *Server);
	virtual const char *GetServer(void) const;

	virtual time_t GetChanJoin(void) const;

	virtual bool SetIdleSince(time_t Time);
	virtual time_t GetIdleSince(void) const;
	
	virtual bool SetTag(const char *Name, const char *Value);
	virtual const char *GetTag(const char *Name) const;
};
