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

struct CDnsEvents {
	virtual void AsyncDnsFinished(adns_query *query, adns_answer *response) = 0;
	virtual void Destroy(void) = 0;
};

#define IMPL_DNSEVENTCLASS(ClassName, EventClassName, Function) \
class ClassName : public CDnsEvents { \
private: \
	EventClassName *m_EventClass; \
\
	virtual void AsyncDnsFinished(adns_query *query, adns_answer *response) { \
		m_EventClass->Function(query, response); \
	} \
public: \
	ClassName(EventClassName* EventClass) { \
		m_EventClass = EventClass; \
	} \
\
	void Destroy(void) { \
		delete this; \
	} \
};
