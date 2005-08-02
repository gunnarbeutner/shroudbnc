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

#if !defined(AFX_TCLSOCKET_H__840815A2_F117_4D3D_8593_4F97538DE148__INCLUDED_)
#define AFX_TCLSOCKET_H__840815A2_F117_4D3D_8593_4F97538DE148__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CBouncerCore;
class CConnection;

class CTclSocket : public CSocketEvents {
public:
	CTclSocket(const char* BindIp, unsigned short Port, const char* TclProc);
	virtual ~CTclSocket();

	virtual void Destroy(void);

	virtual bool Read(bool DontProcess);
	virtual void Write(void);
	virtual void Error(void);
	virtual bool HasQueuedData(void);
	virtual bool DoTimeout(void);

	virtual const char* ClassName(void);

	virtual bool IsValid(void);
	virtual int GetIdx(void);
private:
	SOCKET m_Listener;
	char* m_TclProc;
	int m_Idx;
};

#endif // !defined(AFX_TCLSOCKET_H__840815A2_F117_4D3D_8593_4F97538DE148__INCLUDED_)
