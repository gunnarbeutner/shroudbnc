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

#define FLOODBYTES 450
#define FLOODFADEOUT 65

/**
 * irc_queue_t
 *
 * A queue which has been attached to a CFloodControl object.
 */
typedef struct queue_s {
	int Priority; /**< the priority of the queue (0 = highest) */
	CQueue *Queue; /**< the queue object */
} irc_queue_t;

#ifdef SWIGINTERFACE
%template(CZoneObjectCFloodControl) CZoneObject<class CFloodControl, 16>;
#endif

/**
 * CFloodControl
 *
 * A queue which tries to avoid "Excess Flood" errors.
 */
class SBNCAPI CFloodControl : public CZoneObject<CFloodControl, 16> {
	CVector<irc_queue_t> m_Queues; /**< a list of queues which have been
								attached to this object */
	size_t m_Bytes; /**< the number of bytes which have recently been sent */
	bool m_Control; /**< determines whether this object is delaying the output */
	time_t m_LastCommand; /**< a TS when the last command was sent */

	static int CalculatePenaltyAmplifier(const char *Line);
public:
#ifndef SWIG
	CFloodControl(void);
#endif

	RESULT<char *> DequeueItem(bool Peek = false);
	int GetQueueSize(void);
	int GetBytes(void) const;
	void Clear(void);

	void AttachInputQueue(CQueue *Queue, int Priority);
	int GetRealLength(void) const;

	void Enable(void);
	void Disable(void);
};
