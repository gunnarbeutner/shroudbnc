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
class CNick;
class CBouncerConfig;

class CNick {
	CChannel *m_Owner;
	char *m_Nick;
	char *m_Prefixes;
	char *m_Site;
	char *m_Realname;
	char *m_Server;
	time_t m_Creation;
	time_t m_IdleSince;
	CBouncerConfig *m_Tags;
public:
#ifndef SWIG
	CNick(CChannel* Owner, const char* Nick);
#endif
	virtual ~CNick(void);

	virtual const char *GetNick(void);
	virtual bool SetNick(const char *Nick);

	virtual bool IsOp(void);
	virtual bool IsVoice(void);
	virtual bool IsHalfop(void);

	virtual bool HasPrefix(char Prefix);
	virtual bool AddPrefix(char Prefix);
	virtual bool RemovePrefix(char Prefix);
	virtual bool SetPrefixes(const char* Prefixes);
	virtual const char *GetPrefixes(void);

	virtual bool SetSite(const char *Site);
	virtual bool SetRealname(const char *Realname);
	virtual bool SetServer(const char *Server);

	virtual const char *InternalGetSite(void);
	virtual const char *InternalGetRealname(void);
	virtual const char *InternalGetServer(void);

	virtual const char *GetSite(void);
	virtual const char *GetRealname(void);
	virtual const char *GetServer(void);

	virtual time_t GetChanJoin(void);
	virtual time_t GetIdleSince(void);
	virtual bool SetIdleSince(time_t Time);
	
	virtual bool SetTag(const char *Name, const char *Value);
	virtual const char *GetTag(const char *Name);

	virtual CChannel *GetChannel(void);
};

#ifndef SWIG
void DestroyCNick(CNick *P);
#endif
