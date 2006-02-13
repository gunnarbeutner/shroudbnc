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

typedef struct penalty_s {
	char *Command;
	int Amplifier;
} penalty_t;

static penalty_t penalties [] = {
	{ "MODE", 2 },
	{ "KICK", 2 },
	{ "WHO", 2 },
	{ NULL, 0 }
};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CFloodControl::CFloodControl(void) {
	m_Bytes = 0;
	m_Control = true;
	m_FloodTimer = NULL;
	m_LastCommand = 0;
}

CFloodControl::~CFloodControl() {
	if (m_FloodTimer) {
		m_FloodTimer->Destroy();
	}
}

void CFloodControl::AttachInputQueue(CQueue *Queue, int Priority) {
	irc_queue_t IrcQueue;

	IrcQueue.Queue = Queue;
	IrcQueue.Priority = Priority;

	m_Queues.Insert(IrcQueue);
}

char *CFloodControl::DequeueItem(bool Peek) {
	int LowestPriority = 100;
	irc_queue_t *ThatQueue = NULL;

	if (m_Control && (m_Bytes > FLOODBYTES - 100)) {
		return NULL;
	}

	if (m_Control && (time(NULL) - m_LastCommand < g_Bouncer->GetConfig()->ReadInteger("system.floodwait"))) {
		return NULL;
	}

	for (unsigned int i = 0; i < m_Queues.GetLength(); i++) {
		if (m_Queues[i].Priority < LowestPriority && m_Queues[i].Queue->GetLength() > 0) {
			LowestPriority = m_Queues[i].Priority;
			ThatQueue = &m_Queues[i];
		}
	}

	if (ThatQueue) {
		const char *PItem = ThatQueue->Queue->PeekItem();

		if (PItem == NULL) {
			LOGERROR("PeekItem() failed.");

			return NULL;
		}

		if (m_Control && (strlen(PItem) + m_Bytes > FLOODBYTES - 150 && m_Bytes > 1/4 * FLOODBYTES))
			return NULL;
		else if (Peek)
			return const_cast<char*>(PItem);

		char *Item = ThatQueue->Queue->DequeueItem();
		
		if (Item && m_Control) {
			m_Bytes += strlen(Item) * CalculatePenaltyAmplifier(Item);

			if (m_FloodTimer == NULL)
				m_FloodTimer = g_Bouncer->CreateTimer(1, true, FloodTimer, this);
		}

		m_LastCommand = time(NULL);

		return Item;
	} else
		return NULL;
}

bool CFloodControl::QueueItem(const char *Item) {
	throw "Not supported.";
}

bool CFloodControl::QueueItemNext(const char *Item) {
	throw "Not supported.";
}

int CFloodControl::GetQueueSize(void) {
	if (DequeueItem(true) != NULL) {
		return 1;
	} else {
		return 0;
	}
}

bool CFloodControl::Pulse(time_t Now) {
	m_Bytes -= 75 > m_Bytes ? m_Bytes : 75;

	if (GetRealLength() == 0 && m_Bytes == 0) {
		m_FloodTimer = NULL;

		return false;
	} else {
		return true;
	}
}

int CFloodControl::GetBytes(void) {
	return m_Bytes;
}

int CFloodControl::GetRealLength(void) {
	int Count = 0;

	for (unsigned int i = 0; i < m_Queues.GetLength(); i++) {
		Count += m_Queues[i].Queue->GetLength();
	}

	return Count;
}

void CFloodControl::Clear(void) {
	for (unsigned int i = 0; i < m_Queues.GetLength(); i++) {
		m_Queues[i].Queue->Clear();
	}
}

void CFloodControl::Enable(void) {
	m_Control = true;
}

void CFloodControl::Disable(void) {
	m_Control = false;
}

bool FloodTimer(time_t Now, void *FloodControl) {
	return ((CFloodControl*)FloodControl)->Pulse(Now);
}

int CFloodControl::CalculatePenaltyAmplifier(const char *Line) {
	const char *Space = strstr(Line, " ");
	char *Command;
	
	if (Space) {
		Command = (char *)malloc(Space - Line + 1);

		if (Command == NULL) {
			LOGERROR("malloc() failed");

			return 1;
		}

		strncpy(Command, Line, Space - Line);
		Command[Space - Line] = '\0';
	} else {
		Command = const_cast<char*>(Line);
	}

	int i = 0;

	while (true) {
		penalty_t penalty = penalties[i++];

		if (penalty.Command == NULL) {
			break;
		}

		if (strcasecmp(penalty.Command, Command) == 0) {
			if (Space != NULL) {
				free(Command);
			}

			return penalty.Amplifier;
		}

	}

	if (Space != NULL) {
		free(Command);
	}

	return 1;
}
