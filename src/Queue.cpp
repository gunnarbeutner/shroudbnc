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
 * CQueue
 *
 * Constructs an empty queue
 */
CQueue::CQueue() {
	m_Items = NULL;
	m_ItemCount = 0;
}

/**
 * ~CQueue
 *
 * Destroy a queue
 */
CQueue::~CQueue() {
	FlushQueue();
}

/**
 * PeekItems
 *
 * Retrieves the next item from the queue without removing it
 */
const char *CQueue::PeekItem(void) {
	int LowestPriority = 99999;
	queue_item_t *ThatItem = NULL;

	for (int i = 0; i < m_ItemCount; i++) {
		if (m_Items[i].Valid && m_Items[i].Priority < LowestPriority) {
			LowestPriority = m_Items[i].Priority;
			ThatItem = &m_Items[i];
		}
	}

	if (ThatItem != NULL) {
		return ThatItem->Line;
	} else {
		return NULL;
	}
}

/**
 * DequeueItem
 *
 * Retrieves the next item from the queue and removes it
 */
char *CQueue::DequeueItem(void) {
	int LowestPriority = 99999;
	queue_item_t *ThatItem = NULL;

	for (int i = 0; i < m_ItemCount; i++) {
		if (m_Items[i].Valid && m_Items[i].Priority < LowestPriority) {
			LowestPriority = m_Items[i].Priority;
			ThatItem = &m_Items[i];
		}
	}

	if (ThatItem != NULL) {
		// Caller is now the owner of the object
		ThatItem->Valid = false;

		return ThatItem->Line;
	} else {
		return NULL;
	}
}

/**
 * QueueItem
 *
 * Inserts a new item at the end of the queue
 *
 * @param Item the item which is to be inserted
 */
bool CQueue::QueueItem(const char *Item) {
	queue_item_t *Items;
	char *dupItem;

	if (Item == NULL) {
		return false;
	}

	dupItem = strdup(Item);

	if (dupItem == NULL) {
		LOGERROR("strdup() failed.");

		return false;
	}

	bool doneInsert = false;

	for (int i = 0; i < m_ItemCount; i++) {
		if (!m_Items[i].Valid && !doneInsert) {
			m_Items[i].Priority = 0;
			m_Items[i].Line = dupItem;
			m_Items[i].Valid = true;

			doneInsert = true;
		} else {
			m_Items[i].Priority--;
		}
	}

	if (doneInsert) {
		return true;
	}

	Items = (queue_item_t *)realloc(m_Items,
		++m_ItemCount * sizeof(queue_item_t));

	if (Items == NULL) {
		LOGERROR("realloc() failed. An item was lost (%s).", Item);

		free(dupItem);

		m_ItemCount--;

		return false;
	}

	m_Items = Items;
	m_Items[m_ItemCount - 1].Priority = 0;
	m_Items[m_ItemCount - 1].Line = dupItem;
	m_Items[m_ItemCount - 1].Valid = true;

	return true;
}

/**
 * QueueItemNext
 *
 * Inserts a new item at the front of the queue
 *
 * @param Item the item which is to be inserted
 */
bool CQueue::QueueItemNext(const char *Item) {
	for (int i = 0; i < m_ItemCount; i++) {
		if (m_Items[i].Valid) {
			m_Items[i].Priority++;
		}
	}

	return QueueItem(Item);
}

/**
 * GetQueueSize
 *
 * Returns the number of items which are in the queue
 */
int CQueue::GetQueueSize(void) {
	int Count = 0;
	
	for (int i = 0; i < m_ItemCount; i++) {
		if (m_Items[i].Valid) {
			Count++;
		}
	}

	return Count;
}

/**
 * FlushQueue
 *
 * Removes all items from the queue
 */
void CQueue::FlushQueue(void) {
	for (int i = 0; i < m_ItemCount; i++) {
		if (m_Items[i].Valid) {
			free(m_Items[i].Line);
		}
	}

	free(m_Items);

	m_Items = NULL;
	m_ItemCount = 0;
}

bool CQueue::Freeze(CAssocArray *Box) {
	unsigned int i = 0;
	char *Line, *Index;

	while ((Line = DequeueItem()) != NULL) {
		asprintf(&Index, "%d", i);
		Box->AddString(Index, Line);
		free(Line);
		free(Index);
	}

	delete this;

	return true;
}

CQueue *CQueue::Unfreeze(CAssocArray *Box) {
	unsigned int i = 0;
	char *Index;
	const char *Line;
	CQueue *Queue;

	if (Box == NULL) {
		return NULL;
	}

	Queue = new CQueue();

	while (true) {
		asprintf(&Index, "%d", i);
		Line = Box->ReadString(Index);

		if (Line == NULL) {
			break;
		}

		Queue->QueueItem(Line);

		free(Index);

		i++;
	}

	return Queue;
}
