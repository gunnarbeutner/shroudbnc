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
#include "BouncerUser.h"
#include "Queue.h"
#include "FloodControl.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFloodControl::CFloodControl(CIRCConnection* Owner) {
	m_Queues = NULL;
	m_QueueCount = 0;
	m_Bytes = 0;
	m_Owner = Owner;
	m_Control = true;
}

CFloodControl::~CFloodControl() {
	free(m_Queues);
}

void CFloodControl::AttachInputQueue(CQueue* Queue, int Priority) {
	m_Queues = (queue_t*)realloc(m_Queues, ++m_QueueCount * sizeof(queue_t));

	m_Queues[m_QueueCount - 1].Priority = Priority;
	m_Queues[m_QueueCount - 1].Queue = Queue;
}

char* CFloodControl::DequeueItem(void) {
	int LowestPriority = 100;
	queue_t* ThatQueue = NULL;

	if (m_Control && (m_Bytes > FLOODBYTES - 100))
		return NULL;

	for (int i = 0; i < m_QueueCount; i++) {
		if (m_Queues[i].Priority < LowestPriority && m_Queues[i].Queue->GetQueueSize() > 0) {
			LowestPriority = m_Queues[i].Priority;
			ThatQueue = &m_Queues[i];
		}
	}

	if (ThatQueue) {
		const char* PItem = ThatQueue->Queue->PeekItem();

		if (m_Control && (strlen(PItem) + m_Bytes > FLOODBYTES - 150 && m_Bytes > 1/4 * FLOODBYTES))
			return NULL;

		char* Item = ThatQueue->Queue->DequeueItem();
		
		if (Item && m_Control)
			m_Bytes += strlen(Item);

		return Item;
	} else
		return NULL;
}

void CFloodControl::QueueItem(const char* Item) {
	throw "Not supported.";
}

void CFloodControl::QueueItemNext(const char* Item) {
	throw "Not supported.";
}

int CFloodControl::GetQueueSize(void) {
	if ((m_Bytes > FLOODBYTES - 100) && m_Control)
		return 0;
	else
		return GetRealQueueSize();
}

void CFloodControl::Pulse(time_t Time) {
	m_Bytes -= 75 > m_Bytes ? m_Bytes : 75;
}

int CFloodControl::GetBytes(void) {
	return m_Bytes;
}

int CFloodControl::GetRealQueueSize(void) {
	int a = 0;

	for (int i = 0; i < m_QueueCount; i++)
		a += m_Queues[i].Queue->GetQueueSize();

	return a;
}

void CFloodControl::FlushQueue(void) {
	for (int i = 0; i < m_QueueCount; i++)
		m_Queues[i].Queue->FlushQueue();
}

void CFloodControl::Enable(void) {
	m_Control = true;
}

void CFloodControl::Disable(void) {
	m_Control = false;
}
