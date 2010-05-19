/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007,2010 Gunnar Beutner                                 *
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

bool FloodTimer(time_t Now, void *Null);

/**
 * penalty_t
 *
 * A "penalty" for client commands.
 */
typedef struct penalty_s {
	const char *Command;
	int Amplifier;
} penalty_t;

/**
 * Penalties
 *
 * Some pre-defined penalties for various client commands.
 */
static penalty_t Penalties[] = {
	{ "MODE", 2 },
	{ "KICK", 2 },
	{ "WHO", 2 },
	{ NULL, 0 }
};

CTimer *g_FloodTimer = NULL;

/**
 * CFloodControl
 *
 * Constructs a new flood control object.
 */
CFloodControl::CFloodControl(void) {
	m_Bytes = 0;
	m_Control = true;
	m_LastCommand = 0;

	if (g_FloodTimer == NULL) {
		g_FloodTimer = new CTimer(300, true, FloodTimer, NULL);
	}
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
 * ScheduleItem
 *
 * Re-schedules when the next item can be retrieved using DequeueItem().
 */
void CFloodControl::ScheduleItem(void) {
	size_t Delay, Bytes;

	Bytes = GetBytes() - FLOODBYTES;

	if (Bytes > 0) {
		Delay = (Bytes / FLOODFADEOUT) + 1;
	} else {
		Delay = 0;
	}

	if (Delay > 0 && GetRealLength() > 0) {
		g_FloodTimer->Reschedule(g_CurrentTime + Delay);
	}
}

/**
 * DequeueItem
 *
 * Removes the next item from the queue and returns it.
 *
 * @param Peek determines whether to actually remove the item
 */
RESULT<char *> CFloodControl::DequeueItem(bool Peek) {
	unsigned int Delay;
	int LowestPriority = 100;
	irc_queue_t *ThatQueue = NULL;

	if (m_Control && (GetBytes() > FLOODBYTES)) {
		ScheduleItem();

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

	RESULT<char *> Item = ThatQueue->Queue->DequeueItem();

	THROWIFERROR(char *, Item);

	if (m_Control) {
		m_Bytes = GetBytes() + max(130, strlen(Item) * CalculatePenaltyAmplifier(Item));

		ScheduleItem();
	}

	m_LastCommand = g_CurrentTime;

	RETURN(char *, Item);
}

/**
 * GetQueueSize
 *
 * Returns 1 if there is at least one item in the queue, which
 * could immediately retrieved using DequeueItem().
 */
int CFloodControl::GetQueueSize(void) {
	if (DequeueItem(true) != NULL) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * GetBytes
 *
 * Returns the number of bytes which have been recently sent.
 */
int CFloodControl::GetBytes(void) const {
	if ((size_t)((g_CurrentTime - m_LastCommand) * FLOODFADEOUT) > m_Bytes) {
		return 0;
	} else {
		return m_Bytes - (g_CurrentTime - m_LastCommand) * FLOODFADEOUT;
	}
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
 * Enable
 *
 * Enabled the flood control object.
 */
void CFloodControl::Enable(void) {
	m_Control = true;
}

/**
 * Disable
 *
 * Disabled the flood control object.
 */
void CFloodControl::Disable(void) {
	m_Control = false;
}

/**
 * CalculatePenaltyAmplifier
 *
 * Returns the penalty amplifier for a line.
 *
 * @param Line the line
 */
int CFloodControl::CalculatePenaltyAmplifier(const char *Line) {
	const char *Space = strchr(Line, ' ');
	char Command[32];
	int i;
	penalty_t Penalty;

	if (Space != NULL) {
		strmcpy(Command, Line, min(sizeof(Command), (size_t)(Space - Line + 1)));
	} else {
		strmcpy(Command, Line, sizeof(Command));
	}

	i = 0;

	while (true) {
		Penalty = Penalties[i++];

		if (Penalty.Command == NULL) {
			break;
		}

		if (strcasecmp(Penalty.Command, Command) == 0) {
			return Penalty.Amplifier;
		}

	}

	return 1;
}

/**
 * FloodTimer
 *
 * Controls the output of the CFloodControl class.
 *
 * @param Now the them when the timer should have been executed
 * @param Null a NULL pointer
 */
bool FloodTimer(time_t Now, void *Null) {
	if (Now < g_CurrentTime) {
		// the timer was executed too late, force re-evaluation
		g_FloodTimer->Reschedule(g_CurrentTime + 1);
	}

	return true;
}
