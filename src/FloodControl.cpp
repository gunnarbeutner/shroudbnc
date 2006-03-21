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

/**
 * penalty_t
 *
 * A "penalty" for client commands.
 */
typedef struct penalty_s {
	char *Command;
	int Amplifier;
} penalty_t;

/**
 * Penalties
 *
 * Some pre-defined penalties for various client commands.
 */
static penalty_t Penalties [] = {
	{ "MODE", 2 },
	{ "KICK", 2 },
	{ "WHO", 2 },
	{ NULL, 0 }
};

/**
 * CFloodControl
 *
 * Constructs a new flood control object.
 */
CFloodControl::CFloodControl(void) {
	m_Bytes = 0;
	m_Control = true;
	m_FloodTimer = NULL;
	m_LastCommand = 0;
}

/**
 * ~CFloodControl
 *
 * Destructs a CFloodControl object.
 */
CFloodControl::~CFloodControl(void) {
	if (m_FloodTimer) {
		m_FloodTimer->Destroy();
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
 * DequeueItem
 *
 * Removes the next item from the queue and returns it.
 *
 * @param Peek determines whether to actually remove the item
 */
RESULT<char *> CFloodControl::DequeueItem(bool Peek) {
	int LowestPriority = 100;
	irc_queue_t *ThatQueue = NULL;
	RESULT<const char *> PeekItem;
	RESULT<char *> Item;

	if (m_Control && (m_Bytes > FLOODBYTES - 100)) {
		RETURN(char *, NULL);
	}

	if (m_Control && (g_CurrentTime - m_LastCommand < g_Bouncer->GetConfig()->ReadInteger("system.floodwait"))) {
		RETURN(char *, NULL);
	}

	for (unsigned int i = 0; i < m_Queues.GetLength(); i++) {
		if (m_Queues[i].Priority < LowestPriority && m_Queues[i].Queue->GetLength() > 0) {
			LowestPriority = m_Queues[i].Priority;
			ThatQueue = &m_Queues[i];
		}
	}

	if (ThatQueue == NULL) {
		RETURN(char *, NULL);
	}

	PeekItem = ThatQueue->Queue->PeekItem();

	if (IsError(PeekItem)) {
		LOGERROR("PeekItem() failed.");

		THROWRESULT(char *, PeekItem);
	}

	if (m_Control && (strlen(PeekItem) + m_Bytes > FLOODBYTES - 150 && m_Bytes > 1/4 * FLOODBYTES)) {
		RETURN(char *, NULL);
	} else if (Peek) {
		RETURN(char *, const_cast<char*>((const char *)PeekItem));
	}

	Item = ThatQueue->Queue->DequeueItem();

	THROWIFERROR(char *, Item);

	if (m_Control) {
		m_Bytes += strlen(Item) * CalculatePenaltyAmplifier(Item);

		if (m_FloodTimer == NULL) {
			m_FloodTimer = g_Bouncer->CreateTimer(1, true, FloodTimer, this);
		}
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
 * Pulse
 *
 * Called periodically by a timer.
 *
 * @param Now the current time
 */
bool CFloodControl::Pulse(time_t Now) {
	if (m_Bytes > 75) {
		m_Bytes -= 75;
	} else {
		m_Bytes = 0;
	}

	if (GetRealLength() == 0 && m_Bytes == 0) {
		m_FloodTimer = NULL;

		return false;
	} else {
		return true;
	}
}

/**
 * GetBytes
 *
 * Returns the number of bytes which have been recently sent.
 */
int CFloodControl::GetBytes(void) const {
	return m_Bytes;
}

/**
 * GetRealLength
 *
 * Returns the actual size of the queues.
 */
int CFloodControl::GetRealLength(void) const {
	int Count = 0;

	for (unsigned int i = 0; i < m_Queues.GetLength(); i++) {
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
	for (unsigned int i = 0; i < m_Queues.GetLength(); i++) {
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
 * FloodTimer
 *
 * Used for gradually decreasing the m_Bytes member of CFloodControl objects.
 *
 * @param Now the current time
 * @param FloodControl the CFloodControl Object
 */
bool FloodTimer(time_t Now, void *FloodControl) {
	return ((CFloodControl *)FloodControl)->Pulse(Now);
}

/**
 * CalculatePenaltyAmplifier
 *
 * Returns the penalty amplifier for a line.
 *
 * @param Line the line
 */
int CFloodControl::CalculatePenaltyAmplifier(const char *Line) {
	const char *Space = strstr(Line, " ");
	char *Command;
	int i;
	penalty_t Penalty;
	
	if (Space != NULL) {
		Command = (char *)malloc(Space - Line + 1);

		if (Command == NULL) {
			LOGERROR("malloc() failed");

			return 1;
		}

		strncpy(Command, Line, Space - Line);
		Command[Space - Line] = '\0';
	} else {
		Command = const_cast<char *>(Line);
	}

	i = 0;

	while (true) {
		Penalty = Penalties[i++];

		if (Penalty.Command == NULL) {
			break;
		}

		if (strcasecmp(Penalty.Command, Command) == 0) {
			if (Space != NULL) {
				free(Command);
			}

			return Penalty.Amplifier;
		}

	}

	if (Space != NULL) {
		free(Command);
	}

	return 1;
}
