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

/** Defines how many items can be stored in a single queue */
#define MAX_QUEUE_SIZE 500

/**
 * queue_item_t
 *
 * An item from a queue.
 */
typedef struct queue_item_s {
	int Priority; /**< the priority of this item; 0 is the highest priority */
	char *Line; /**< the string which is associated with this item */
} queue_item_t;

/**
 * CQueue
 *
 * A queue which can be used for storing strings.
 */
class CQueue : public CZoneObject<CQueue, 64> {
	CVector<queue_item_t> m_Items; /**< the items which are in the queue */
public:
#ifndef SWIG
	RESULT<bool> Freeze(CAssocArray *Box);
	static RESULT<CQueue *> Thaw(CAssocArray *Box);
#endif

	virtual RESULT<char *> DequeueItem(void);
	virtual RESULT<const char *> PeekItem(void) const;
	virtual RESULT<bool> QueueItem(const char *Line);
	virtual RESULT<bool> QueueItemNext(const char *Line);
	virtual unsigned int GetLength(void) const;
	virtual void Clear(void);
};
