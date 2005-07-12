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

#if !defined(AFX_CLIENTCONNECTION_H__4EDF9F6A_4713_4A92_AD82_EE7CA40BC214__INCLUDED_)
#define AFX_CLIENTCONNECTION_H__4EDF9F6A_4713_4A92_AD82_EE7CA40BC214__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef SWIG
bool AdnsTimeoutTimer(time_t Now, void* Client);
#endif

class CTimer;

class CClientConnection : public CConnection, public CDnsEvents {
#ifndef SWIG
	friend bool AdnsTimeoutTimer(time_t Now, void* Client);
#endif

	char* m_Nick;
	char* m_Password;
	char* m_Username;
	sockaddr_in m_Peer;
	char* m_PeerName;
	CTimer* m_AdnsTimeout;

#ifndef SWIG
	adns_query m_PeerA;
#endif

	void ValidateUser(void);
	virtual bool ReadLine(char** Out);
	void AdnsTimeout(void);
public:
#ifndef SWIG
	CClientConnection(SOCKET Socket, sockaddr_in Peer);
#endif
	virtual ~CClientConnection(void);

	virtual connection_role_e GetRole(void);

	virtual bool ParseLineArgV(int argc, const char** argv);
	virtual void ParseLine(const char* Line);
	virtual bool ProcessBncCommand(const char* Subcommand, int argc, const char** argv, bool NoticeUser);

	virtual const char* GetNick(void);

	virtual void SetOwner(CBouncerUser* Owner);

	virtual void AsyncDnsFinished(adns_query* query, adns_answer* response);
	virtual void SetPeerName(const char* PeerName);
	virtual adns_query GetPeerDNSQuery(void);

	virtual const char* GetPeerName(void);
	virtual sockaddr_in GetPeer(void);

	virtual bool Read(void);
	virtual void Destroy(void);
	virtual const char* ClassName(void);
};

#endif // !defined(AFX_CLIENTCONNECTION_H__4EDF9F6A_4713_4A92_AD82_EE7CA40BC214__INCLUDED_)
