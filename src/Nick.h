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

class CChannel;

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
%template(CZoneObjectCNick) CZoneObject<class CNick, 128>;
#endif

/**
 * CNick
 *
 * Represents a user on a single channel.
 */
class SBNCAPI CNick : public CObject<CNick, CChannel>, public CZoneObject<CNick, 128> {
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
	CNick(const char *Nick, CChannel *Owner, safe_box_t Box);
	virtual ~CNick(void);

	static RESULT<CNick *> Thaw(safe_box_t Box, CChannel *Owner);
#endif

	bool SetNick(const char *Nick);
	const char *GetNick(void) const;

	bool IsOp(void) const;
	bool IsVoice(void) const;
	bool IsHalfop(void) const;

	bool HasPrefix(char Prefix) const;
	bool AddPrefix(char Prefix);
	bool RemovePrefix(char Prefix);
	bool SetPrefixes(const char *Prefixes);
	const char *GetPrefixes(void) const;

	bool SetSite(const char *Site);
	const char *GetSite(void) const;

	bool SetRealname(const char *Realname);
	const char *GetRealname(void) const;

	bool SetServer(const char *Server);
	const char *GetServer(void) const;

	time_t GetChanJoin(void) const;

	bool SetIdleSince(time_t Time);
	time_t GetIdleSince(void) const;
	
	bool SetTag(const char *Name, const char *Value);
	const char *GetTag(const char *Name) const;
};
