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

#if !defined(AFX_QUEUE_H__BE311FBC_F9FB_4649_97B9_6F3726BE9F66__INCLUDED_)
#define AFX_QUEUE_H__BE311FBC_F9FB_4649_97B9_6F3726BE9F66__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

typedef struct queue_item_s {
	bool Valid;
	int Priority;
	char* Line;
} queue_item_t;

class CFloodControl;

class CQueue {
	queue_item_t* m_Items;
	int m_ItemCount;
	CFloodControl* m_Notify;
public:
#ifndef SWIG
	CQueue(void);
	~CQueue(void);
#endif

	virtual char* DequeueItem(void);
	virtual const char* PeekItem(void);
	virtual void QueueItem(const char* Item);
	virtual void QueueItemNext(const char* Item);
	virtual int GetQueueSize(void);
	virtual void FlushQueue(void);

	virtual void SetNotifyObject(CFloodControl* Notify);
};

#endif // !defined(AFX_QUEUE_H__BE311FBC_F9FB_4649_97B9_6F3726BE9F66__INCLUDED_)
