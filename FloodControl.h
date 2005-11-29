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

typedef struct queue_s {
	int Priority;
	CQueue* Queue;
} queue_t;

class CIRCConnection;
class CTimer;

#ifndef SWIG
bool FloodTimer(time_t Now, void *FloodControl);
#endif

class CFloodControl : public CQueue {
#ifndef SWIG
	friend bool FloodTimer(time_t Now, void *FloodControl);
#endif

	queue_t *m_Queues;
	int m_QueueCount;
	int m_Bytes;
	CIRCConnection *m_Owner;
	bool m_Control;
	CTimer *m_FloodTimer;

	bool Pulse(time_t Time);

	int CalculatePenaltyAmplifier(const char *Line);
public:
#ifndef SWIG
	CFloodControl(CIRCConnection *Owner);
#endif
	virtual ~CFloodControl(void);

	virtual char *DequeueItem(bool Peek = false);
	virtual bool QueueItem(const char *Item);
	virtual bool QueueItemNext(const char *Item);
	virtual int GetQueueSize(void);

	virtual void AttachInputQueue(CQueue *Queue, int Priority);
	virtual int GetBytes(void);
	virtual int GetRealQueueSize(void);
	virtual void FlushQueue(void);

	virtual void Enable(void);
	virtual void Disable(void);
};
