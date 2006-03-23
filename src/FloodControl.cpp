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

int CFloodControl::m_FloodWaitCache;
time_t g_NextCommand;

/**
 * CFloodControl
 *
 * Constructs a new flood control object.
 */
CFloodControl::CFloodControl(void) {
	m_Bytes = 0;
	m_Control = true;
	m_LastCommand = 0;

	m_FloodWaitCache = g_Bouncer->GetConfig()->ReadInteger("system.floodwait");
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

#define ScheduleCommand() \
	int Bytes = min(CurrentBytes, FLOODBYTES - 100); \
	if (Bytes > 0) { \
		Delay = (Bytes / 75) + 1; \
	} else { \
		Delay = 0; \
	} \
	\
	if ((g_CurrentTime + Delay < g_NextCommand || g_NextCommand < g_CurrentTime) && GetRealLength() > 0) { \
		g_NextCommand = g_CurrentTime + Delay; \
	}

/**
 * DequeueItem
 *
 * Removes the next item from the queue and returns it.
 *
 * @param Peek determines whether to actually remove the item
 */
RESULT<char *> CFloodControl::DequeueItem(bool Peek) {
	size_t CurrentBytes;
	unsigned int Delay;
	int LowestPriority = 100;
	irc_queue_t *ThatQueue = NULL;

	CurrentBytes = GetBytes();

	if (m_Control && (CurrentBytes > FLOODBYTES - 100)) {
		ScheduleCommand();

		RETURN(char *, NULL);
	}

	if (m_Control && (g_CurrentTime - m_LastCommand < m_FloodWaitCache)) {
		ScheduleCommand();

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

	RESULT<const char *> PeekItem = ThatQueue->Queue->PeekItem();

	if (IsError(PeekItem)) {
		LOGERROR("PeekItem() failed.");

		THROWRESULT(char *, PeekItem);
	}

	if (m_Control && (strlen(PeekItem) + CurrentBytes > FLOODBYTES - 150 && CurrentBytes > 1/4 * FLOODBYTES)) {
		RETURN(char *, NULL);
	} else if (Peek) {
		RETURN(char *, const_cast<char*>((const char *)PeekItem));
	}

	RESULT<char *> Item = ThatQueue->Queue->DequeueItem();

	THROWIFERROR(char *, Item);

	if (m_Control) {
		CurrentBytes += strlen(Item) * CalculatePenaltyAmplifier(Item);
		m_Bytes = CurrentBytes;

		ScheduleCommand();
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
	if ((g_CurrentTime - m_LastCommand) * 75 > m_Bytes) {
		return 0;
	} else {
		return m_Bytes - (g_CurrentTime - m_LastCommand) * 75;
	}
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
