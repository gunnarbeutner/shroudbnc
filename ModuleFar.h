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

#if !defined(AFX_MODULEFAR_H__898BF56F_427C_4FC5_8D8D_A68E48A0B426__INCLUDED_)
#define AFX_MODULEFAR_H__898BF56F_427C_4FC5_8D8D_A68E48A0B426__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CBouncerCore;
class CIRCConnection;
class CClientConnection;

struct CModuleFar {
	virtual void Destroy(void) = 0;

	virtual void Init(CBouncerCore* Root) = 0;
	virtual void Pulse(time_t Now) = 0;

	virtual bool InterceptIRCMessage(CIRCConnection* Connection, int argc, const char** argv) = 0;
	virtual bool InterceptClientMessage(CClientConnection* Connection, int argc, const char** argv) = 0;

	virtual void AttachClient(const char* Client) = 0;
	virtual void DetachClient(const char* Client) = 0;
};

#endif // !defined(AFX_MODULEFAR_H__898BF56F_427C_4FC5_8D8D_A68E48A0B426__INCLUDED_)
