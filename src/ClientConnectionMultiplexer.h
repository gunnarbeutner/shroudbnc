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

class CClientConnectionMultiplexer : public CClientConnection {
public:
	CClientConnectionMultiplexer(CUser *User);

	virtual void ParseLine(const char *Line);

	virtual const char *GetNick(void) const;
	virtual const char *GetPeerName(void) const;

	virtual void Kill(const char *Error);
	virtual void Destroy(void);

	virtual commandlist_t *GetCommandList(void);

	virtual SOCKET Hijack(void);

	virtual void ChangeNick(const char *NewNick);
	virtual void SetNick(const char *NewNick);

	virtual void Privmsg(const char *Text);
	virtual void RealNotice(const char *Text);

	virtual void WriteUnformattedLine(const char *Line);

	/**
	 * operator new
	 *
	 * Overrides the base class new operator. CClientConnection's would not work
	 * because CFakeClient objects are larger than CClientConnection objects.
	 */
	void *operator new (size_t Size) {
		return malloc(Size);
	}

	/**
	 * operator delete
	 *
	 * Overrides the base class new operator.
	 */
	void operator delete(void *Object) {
		free(Object);
	}
};
