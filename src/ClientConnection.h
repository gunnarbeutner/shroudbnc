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

#ifdef SWIGINTERFACE
%template(COwnedObjectCUser) COwnedObject<class CUser>;
%template(CZoneObjectCClientConnection) CZoneObject<class CClientConnection, 16>;
#endif

/**
 * CClientConnection
 *
 * A class for clients of the IRC bouncer.
 */
class CClientConnection : public CConnection, public COwnedObject<CUser>,
	public CZoneObject<CClientConnection, 16> {
private:
	char *m_Nick; /**< the current nick of the user */
	char *m_PreviousNick; /**< the previous nick of the user */
	char *m_Password; /**< the password which was supplied by the user */
	char *m_Username; /**< the username the user supplied */
	char *m_PeerName; /**< the hostname of the user */
	char *m_PeerNameTemp; /**< a temporary variable for the hostname */
	commandlist_t m_CommandList; /**< a list of commands used by the "help" command */

#ifndef SWIG
public:
	virtual void AsyncDnsFinishedClient(hostent* response);

	CDnsQuery *m_ClientLookup; /**< dns query for looking up the user's hostname */
private:
#endif

	bool ValidateUser();
	void SetPeerName(const char *PeerName, bool LookupFailure);
	virtual bool Read(bool DontProcess = false);
	virtual const char* GetClassName(void) const;
	void WriteUnformattedLine(const char* In);
	bool ParseLineArgV(int argc, const char **argv);
	bool ProcessBncCommand(const char *Subcommand, int argc, const char **argv, bool NoticeUser);

	CClientConnection(void);
public:
#ifndef SWIG
	CClientConnection(SOCKET Socket, bool SSL = false);
	virtual ~CClientConnection(void);

	RESULT<bool> Freeze(CAssocArray *Box);
	static RESULT<CClientConnection *> Thaw(CAssocArray *Box);
#endif

	virtual void ParseLine(const char *Line);

	virtual const char *GetNick(void) const;
	virtual const char *GetPeerName(void) const;

	virtual void Kill(const char *Error);
	virtual void Destroy(void);

	virtual commandlist_t *GetCommandList(void);

	virtual void SetPreviousNick(const char *Nick);
	virtual const char *GetPreviousNick(void) const;

	virtual SOCKET Hijack(void);
};

#ifndef SWIG
class CFakeClient : public CClientConnection {
	CFIFOBuffer m_Queue;
	char *m_Data;

	virtual void WriteUnformattedLine(const char *Line) {
		m_Queue.WriteUnformattedLine(Line);
	}
public:
	CFakeClient(void) : CClientConnection(INVALID_SOCKET, false) {
		m_Data = NULL;
	}

	virtual ~CFakeClient(void) {
		free(m_Data);
	}
	virtual const char *GetData(void) {
		free(m_Data);

		m_Data = (char *)malloc(m_Queue.GetSize() + 1);

		memcpy(m_Data, m_Queue.Peek(), m_Queue.GetSize());
		m_Data[m_Queue.GetSize()] = '\0';

		return m_Data;
	}

	void *operator new (size_t Size) {
		return malloc(Size);
	}

	void operator delete(void *Object) {
		free(Object);
	}
};
#else
class CFakeClient;
#endif