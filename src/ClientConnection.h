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

class CTimer;
class CAssocArray;

class CClientConnection : public CConnection, public COwnedObject<CUser>, public CZoneObject<CClientConnection, 16> {
private:
	char *m_Nick;
	char *m_PreviousNick;
	char *m_Password;
	char *m_Username;
	char *m_PeerName;
	char *m_PeerNameTemp;

	commandlist_t m_CommandList;

#ifndef SWIG
public:
	virtual void AsyncDnsFinishedClient(hostent* response);

	CDnsQuery *m_ClientLookup;
private:
#endif

	bool ValidateUser();

	CClientConnection(void);
public:
#ifndef SWIG
	CClientConnection(SOCKET Socket, bool SSL = false);
#endif
	virtual ~CClientConnection(void);

#ifndef SWIG
	bool Freeze(CAssocArray *Box);
	static CClientConnection *Thaw(CAssocArray *Box);
#endif

	virtual bool ParseLineArgV(int argc, const char** argv);
	virtual void ParseLine(const char* Line);
	virtual bool ProcessBncCommand(const char* Subcommand, int argc, const char** argv, bool NoticeUser);

	virtual const char* GetNick(void);

	virtual void SetOwner(CUser* Owner);

	virtual void SetPeerName(const char* PeerName, bool LookupFailure);
	virtual const char* GetPeerName(void);

	virtual bool Read(bool DontProcess = false);
	virtual void Destroy(void);
	virtual const char* GetClassName(void);
	virtual void WriteUnformattedLine(const char* In);

	virtual void Kill(const char *Error);

	virtual commandlist_t *GetCommandList(void);

	virtual void SetPreviousNick(const char *Nick);
	virtual const char *GetPreviousNick(void);

	virtual SOCKET Hijack(void);
};
