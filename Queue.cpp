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

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CQueue::CQueue() {
	m_Items = NULL;
	m_ItemCount = 0;
}

CQueue::~CQueue() {
	for (int i = 0; i < m_ItemCount; i++) {
		if (m_Items[i].Valid)
			free(m_Items[i].Line);
	}

	free(m_Items);
}

const char* CQueue::PeekItem(void) {
	int LowestPriority = 100;
	queue_item_t* ThatItem = NULL;

	for (int i = 0; i < m_ItemCount; i++) {
		if (m_Items[i].Valid && m_Items[i].Priority < LowestPriority) {
			LowestPriority = m_Items[i].Priority;
			ThatItem = &m_Items[i];
		}
	}

	if (ThatItem)
		return ThatItem->Line;
	else
		return NULL;
}

char* CQueue::DequeueItem(void) {
	int LowestPriority = 100;
	queue_item_t* ThatItem = NULL;

	for (int i = 0; i < m_ItemCount; i++) {
		if (m_Items[i].Valid && m_Items[i].Priority < LowestPriority) {
			LowestPriority = m_Items[i].Priority;
			ThatItem = &m_Items[i];
		}
	}

	if (ThatItem) {
		// Caller is now the owner of the object
		ThatItem->Valid = false;

		return ThatItem->Line;
	} else
		return NULL;
}

void CQueue::QueueItem(const char* Item) {
	for (int i = 0; i < m_ItemCount; i++) {
		if (!m_Items[i].Valid) {
			m_Items[i].Priority = 0;
			m_Items[i].Line = strdup(Item);
			m_Items[i].Valid = true;

			return;
		}
	}

	m_Items = (queue_item_t*)realloc(m_Items, ++m_ItemCount * sizeof(queue_item_t));

	m_Items[m_ItemCount - 1].Priority = 0;
	m_Items[m_ItemCount - 1].Line = strdup(Item);
	m_Items[m_ItemCount - 1].Valid = true;
}

void CQueue::QueueItemNext(const char* Item) {
	for (int i = 0; i < m_ItemCount; i++) {
		if (m_Items[i].Valid)
			m_Items[i].Priority++;
	}

	QueueItem(Item);
}

int CQueue::GetQueueSize(void) {
	int a = 0;
	
	for (int i = 0; i < m_ItemCount; i++) {
		if (m_Items[i].Valid)
			a++;
	}

	return a;
}

void CQueue::FlushQueue(void) {
	for (int i = 0; i < m_ItemCount; i++) {
		if (m_Items[i].Valid) {
			free(m_Items[i].Line);
			m_Items[i].Valid = false;
		}
	}
}
