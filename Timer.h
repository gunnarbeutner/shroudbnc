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

#if !defined(AFX_TIMER_H__7D0C99FA_2766_4DB2_AB06_E03F3595EA58__INCLUDED_)
#define AFX_TIMER_H__7D0C99FA_2766_4DB2_AB06_E03F3595EA58__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

typedef bool (*timerproc)(time_t now, void* cookie);

class CTimer {
public:
#ifndef SWIG
	CTimer(unsigned int Interval, bool Repeat, timerproc Function, void* Cookie);
#endif
	virtual ~CTimer(void);

	virtual void Destroy(void);
	virtual bool Call(time_t Now);
	virtual time_t GetNextCall(void);

private:
	timerproc m_Proc;
	void* m_Cookie;
	unsigned int m_Interval;
	bool m_Repeat;
	time_t m_Next;
};

#endif // !defined(AFX_TIMER_H__7D0C99FA_2766_4DB2_AB06_E03F3595EA58__INCLUDED_)
