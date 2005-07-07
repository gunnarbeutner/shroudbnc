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

#if !defined(AFX_KEYRING_H__DBFDEBD5_5CF7_4A4D_B628_1F43E99ADB7F__INCLUDED_)
#define AFX_KEYRING_H__DBFDEBD5_5CF7_4A4D_B628_1F43E99ADB7F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class CBouncerConfig;

class CKeyring {
public:
#ifndef SWIG
	CKeyring(CBouncerConfig* Config);
	~CKeyring(void);
#endif

	virtual const char* GetKey(const char* Channel);
	virtual void AddKey(const char* Channel, const char* Key);
	virtual void DeleteKey(const char* Channel);
private:
	CBouncerConfig* m_Config;
};

#endif // !defined(AFX_KEYRING_H__DBFDEBD5_5CF7_4A4D_B628_1F43E99ADB7F__INCLUDED_)
