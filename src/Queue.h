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

typedef struct queue_item_s {
	int Priority;
	char *Line;
} queue_item_t;

#define MAX_QUEUE_SIZE 500

class CQueue : public CZoneObject<CQueue, 64> {
	CVector<queue_item_t> m_Items; /**< the items which are in the queue */
public:
#ifndef SWIG
	CQueue(void);
#endif
	virtual ~CQueue(void);

#ifndef SWIG
	bool CQueue::Freeze(CAssocArray *Box);
	static CQueue *CQueue::Thaw(CAssocArray *Box);
#endif

	virtual char *DequeueItem(void);
	virtual const char *PeekItem(void);
	virtual bool QueueItem(const char *Line);
	virtual bool QueueItemNext(const char *Line);
	virtual unsigned int GetLength(void);
	virtual void Clear(void);
};
