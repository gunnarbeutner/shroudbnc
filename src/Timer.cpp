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

int g_TimerStats = 0;

/**
 * CTimer
 *
 * Constructs a timer.
 *
 * @param Interval the interval (in seconds) between calls to the timer's function
 * @param Repeat whether the timer should repeat itself
 * @param Function the timer's function
 * @param Cookie a timer-specific cookie
 */
CTimer::CTimer(unsigned int Interval, bool Repeat, TimerProc Function, void* Cookie) {
	m_Interval = Interval; 
	m_Repeat = Repeat;
	m_Proc = Function;
	m_Cookie = Cookie;

	time(&m_Next);
	m_Next += Interval;

	g_Bouncer->RegisterTimer(this);
}

/**
 * ~CTimer
 *
 * Destroys a timer.
 */
CTimer::~CTimer(void) {
	g_Bouncer->UnregisterTimer(this);
}

/**
 * Destroy
 *
 * Manually destroys a timer.
 */
void CTimer::Destroy(void) {
	delete this;
}

/**
 * Call
 *
 * Calls the timer's function
 *
 * @param Now the current time
 */
bool CTimer::Call(time_t Now) {
	bool ReturnValue;

#ifdef _DEBUG
	g_TimerStats++;
#endif

	if (m_Interval != 0) {
		m_Next = Now + m_Interval;
	}

	if (m_Proc == NULL) {
		if (m_Interval == 0) {
			Destroy();

			return false;
		}

		return true;
	}

	ReturnValue = m_Proc(Now, m_Cookie);

	if (ReturnValue == false || m_Repeat == false) {
		Destroy();

		return false;
	}

	return true;
}

/**
 * GetNextCall
 *
 * Returns the next scheduled time of execution.
 */
time_t CTimer::GetNextCall(void) const {
	return m_Next;
}

/**
 * GetInterval
 *
 * Returns the timer's interval.
 */
int CTimer::GetInterval(void) const {
	return m_Interval;
}

/**
 * GetRepeat
 *
 * Returns whether the timer is being called repeatedly.
 */
bool CTimer::GetRepeat(void) const {
	return m_Repeat;
}
