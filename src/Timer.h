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

typedef bool (*TimerProc)(time_t CurrentTime, void *Cookie);

#ifdef SWIGINTERFACE
%template(CZoneObjectCTimer) CZoneObject<class CTimer, 64>;
#endif

/**
 * CTimer
 *
 * A timer.
 */
class CTimer : public CZoneObject<CTimer, 512> {
private:
	friend class CCore;

	TimerProc m_Proc; /**< the function which should be called for the timer */
	void *m_Cookie; /**< a user-specific pointer which is passed to the timer's function */
	unsigned int m_Interval; /**< the timer's interval */
	bool m_Repeat; /**< determines whether the timer is executed repeatedly */
	time_t m_Next; /**< the next scheduled time of execution */
	link_t<CTimer *> *m_Link; /**< link in the timer list */

	bool Call(time_t Now);
public:
#ifndef SWIG
	CTimer(unsigned int Interval, bool Repeat, TimerProc Function, void *Cookie);
	virtual ~CTimer(void);
#endif

	virtual time_t GetNextCall(void) const;
	virtual int GetInterval(void) const;
	virtual bool GetRepeat(void) const;

	virtual void Reschedule(time_t Next);

	virtual void Destroy(void);
};
