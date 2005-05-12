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

#if !defined(AFX_BOUNCERLOG_H__A6DB1C48_D094_400E_B622_10F193CF879C__INCLUDED_)
#define AFX_BOUNCERLOG_H__A6DB1C48_D094_400E_B622_10F193CF879C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CBouncerUser;

class CBouncerLog {
	char* m_File;
public:
	CBouncerLog(const char* Filename);
	virtual ~CBouncerLog();

	virtual void Clear(void);
	virtual void InternalWriteLine(const char* Line);
	virtual void WriteLine(const char* Format, ...);
	virtual void PlayToUser(CBouncerUser* User, bool NoticeUser);
	virtual bool IsEmpty(void);
};

#endif // !defined(AFX_BOUNCERLOG_H__A6DB1C48_D094_400E_B622_10F193CF879C__INCLUDED_)
