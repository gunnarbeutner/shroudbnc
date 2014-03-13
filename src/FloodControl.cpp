/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2014 Gunnar Beutner                                      *
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

/**
 * CFloodControl
 *
 * Constructs a new flood control object.
 */
CFloodControl::CFloodControl(void) {
	m_BytesSent = 0;
	m_Enabled = true;
	m_Plugged = false;
}

/**
 * AttachInputQueue
 *
 * Attaches a queue to this flood control object.
 *
 * @param Queue the new queue
 * @param Priority the priority of the queue (the highest priority is 0)
 */
void CFloodControl::AttachInputQueue(CQueue *Queue, int Priority) {
	irc_queue_t IrcQueue;

	IrcQueue.Queue = Queue;
	IrcQueue.Priority = Priority;

	m_Queues.Insert(IrcQueue);
}

/**
 * DequeueItem
 *
 * Removes the next item from the queue and returns it.
 *
 * @param Peek determines whether to actually remove the item
 */
RESULT<char *> CFloodControl::DequeueItem(bool Peek) {
	int LowestPriority = 100;
	irc_queue_t *ThatQueue = NULL;

	if (m_Enabled && m_Plugged) {
		RETURN(char *, NULL);
	}

	for (int i = 0; i < m_Queues.GetLength(); i++) {
		if (m_Queues[i].Priority < LowestPriority && m_Queues[i].Queue->GetLength() > 0) {
			LowestPriority = m_Queues[i].Priority;
			ThatQueue = &m_Queues[i];
		}
	}

	if (ThatQueue == NULL) {
		RETURN(char *, NULL);
	}

	RESULT<const char *> PeekItem = ThatQueue->Queue->PeekItem();

	if (IsError(PeekItem)) {
		g_Bouncer->Log("PeekItem() failed.");

		THROWRESULT(char *, PeekItem);
	}

	if (Peek) {
		RETURN(char *, const_cast<char *>((const char *)PeekItem));
	}

	if (m_Enabled && m_BytesSent > 0 && m_BytesSent + strlen(PeekItem) + 2 + strlen(FLOODMSG) + 2 > FLOODBYTES) {
		Plug();

		RETURN(char *, strdup(FLOODMSG));
	}

	RESULT<char *> Item = ThatQueue->Queue->DequeueItem();

	THROWIFERROR(char *, Item);

	m_BytesSent += strlen(Item) + 2;

	RETURN(char *, Item);
}

/**
 * GetQueueSize
 *
 * Returns 1 if there is at least one item in the queue, which
 * could be immediately retrieved using DequeueItem().
 */
int CFloodControl::GetQueueSize(void) {
	if (m_Plugged)
		return 0;
	else
		return (GetRealLength() > 0);
}

/**
 * GetRealLength
 *
 * Returns the actual size of the queues.
 */
int CFloodControl::GetRealLength(void) const {
	int Count = 0;

	for (int i = 0; i < m_Queues.GetLength(); i++) {
		Count += m_Queues[i].Queue->GetLength();
	}

	return Count;
}

/**
 * Clear
 *
 * Clears all queues which have been attached to the flood control object.
 */
void CFloodControl::Clear(void) {
	for (int i = 0; i < m_Queues.GetLength(); i++) {
		m_Queues[i].Queue->Clear();
	}
}

/**
 * Plug
 *
 * Plugs the queue (i.e. stops processing items).
 */
void CFloodControl::Plug(void) {
	m_Plugged = true;
}

/**
 * Unplug
 *
 * Unplugs the queue (i.e. enables processing items).
 */
void CFloodControl::Unplug(void) {
	m_BytesSent = 0;
	m_Plugged = false;
}

/**
 * Enable
 *
 * Enabled the flood control object.
 */
void CFloodControl::Enable(void) {
	m_Enabled = true;
}

/**
 * Disable
 *
 * Disabled the flood control object.
 */
void CFloodControl::Disable(void) {
	m_Enabled = false;
}
