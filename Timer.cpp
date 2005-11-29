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

#include "StdAfx.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

int g_TimerStats = 0;

CTimer::CTimer(unsigned int Interval, bool Repeat, TimerProc Function, void* Cookie) {
	m_Interval = Interval; 
	m_Repeat = Repeat;
	m_Proc = Function;
	m_Cookie = Cookie;
	m_Next = time(NULL) + Interval;

	g_Bouncer->RegisterTimer(this);
}

CTimer::~CTimer(void) {
	g_Bouncer->UnregisterTimer(this);
}

void CTimer::Destroy(void) {
	delete this;
}

bool CTimer::Call(time_t Now) {
	g_TimerStats++;

	if (m_Interval)
		m_Next = Now + m_Interval;

	if (!m_Proc) {
		if (!m_Interval) {
			Destroy();

			return false;
		}

		return true;
	}

	bool Ret = m_Proc(Now, m_Cookie);

	if (!Ret || !m_Repeat) {
		Destroy();

		return false;
	}

	return true;
}

time_t CTimer::GetNextCall(void) {
	return m_Next;
}
