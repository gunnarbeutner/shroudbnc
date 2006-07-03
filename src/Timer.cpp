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

static time_t g_NextCall = 0;
static CList<CTimer *> *g_Timers = NULL;

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
CTimer::CTimer(unsigned int Interval, bool Repeat, TimerProc Function, void *Cookie) {
	m_Interval = Interval; 
	m_Repeat = Repeat;
	m_Proc = Function;
	m_Cookie = Cookie;

	Reschedule(g_CurrentTime + Interval);

	if (g_Timers == NULL) {
		g_Timers = new CList<CTimer *>();
	}

	m_Link = g_Timers->Insert(this);
}

/**
 * ~CTimer
 *
 * Destroys a timer.
 */
CTimer::~CTimer(void) {
	g_Timers->Remove(m_Link);

	RescheduleTimers();
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
	time_t ThisCall;
	bool ReturnValue;

	if (m_Interval != 0) {
		ThisCall = m_Next;
		Reschedule(Now + m_Interval);
	}

	if (m_Proc == NULL) {
		if (m_Interval == 0) {
			Destroy();

			return false;
		}

		return true;
	}

	ReturnValue = m_Proc(ThisCall, m_Cookie);

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
time_t CTimer::GetNextCall(void) {
	if (g_NextCall == 0) {
		return g_CurrentTime + 120;
	} else {
		return g_NextCall;
	}
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

/**
 * Reschedule
 *
 * Reschedules the next call for the timer.
 *
 * @param Next the next call
 */
void CTimer::Reschedule(time_t Next) {
	m_Next = Next;

	if (Next < g_NextCall || g_NextCall == 0) {
		g_NextCall = Next;
	}
}

void CTimer::DestroyAllTimers(void) {
	for (CListCursor<CTimer *> TimerCursor(g_Timers); TimerCursor.IsValid(); TimerCursor.Proceed()) {
		delete *TimerCursor;
	}
}

void CTimer::CallTimers(void) {
	g_NextCall = 0;

	for (CListCursor<CTimer *> TimerCursor(g_Timers); TimerCursor.IsValid(); TimerCursor.Proceed()) {
		if (g_CurrentTime >= (*TimerCursor)->m_Next) {
			(*TimerCursor)->Call(g_CurrentTime);
		} else if ((*TimerCursor)->m_Next < g_NextCall) {
			g_NextCall = (*TimerCursor)->m_Next;
		}
	}
}

void CTimer::RescheduleTimers(void) {
	time_t Best;

	Best = g_CurrentTime + 120;

	for (CListCursor<CTimer *> TimerCursor(g_Timers); TimerCursor.IsValid(); TimerCursor.Proceed()) {
		if ((*TimerCursor)->m_Next < Best) {
			Best = (*TimerCursor)->m_Next;
		}
	}

	g_NextCall = Best;
}
