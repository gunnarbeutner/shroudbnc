/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007 Gunnar Beutner                                           *
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

/**
 * CClientConnectionMultiplexer
 *
 * A class which manages several client objects.
 */
class SBNCAPI CClientConnectionMultiplexer : public CClientConnection {
public:
	CClientConnectionMultiplexer(CUser *User);

	virtual void ParseLine(const char *Line);

	virtual const char *GetNick(void) const;
	virtual const char *GetPeerName(void) const;

	virtual void Kill(const char *Error);
	virtual void Destroy(void);

	virtual commandlist_t *GetCommandList(void);

	virtual clientdata_t Hijack(void);

	virtual void ChangeNick(const char *NewNick);
	virtual void SetNick(const char *NewNick);

	virtual void Privmsg(const char *Text);
	virtual void RealNotice(const char *Text);

	virtual void Shutdown(void);

	virtual void WriteUnformattedLine(const char *Line);
};
