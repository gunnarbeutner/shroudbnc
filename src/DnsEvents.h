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

#ifndef SWIG
/**
 * DnsEventFunction
 *
 * A function typedef for DNS event callbacks.
 */
typedef void (*DnsEventFunction)(void *Object, hostent *Response);

class CTimer;

/**
 * CDnsQuery
 *
 * A class for dns queries.
 */
class SBNCAPI CDnsQuery {
	friend void GenericDnsQueryCallback(void *Cookie, int Status, hostent *HostEntity);
	friend bool DestroyDnsChannelTimer(time_t Now, void *Cookie);

	void *m_EventObject; /**< the object used for callbacks */
	DnsEventFunction m_EventFunction; /**< the function used for callbacks */
	ares_channel m_Channel; /**< the ares channel object */
	time_t m_Timeout; /**< timeout for the dns query (in seconds) */
	unsigned int m_PendingQueries; /**< number of pending queries */

	void InitChannel(void);
	void DestroyChannel(void);
	void AsyncDnsEvent(int Status, hostent *Response);
public:
	CDnsQuery(void *EventInterface, DnsEventFunction EventFunction, int Timeout = 5);
	~CDnsQuery(void);
	void GetHostByName(const char *Host, int Family = AF_INET);
	void GetHostByAddr(sockaddr *Address);
	ares_channel GetChannel(void);
};

/**
 * IMPL_DNSEVENTPROXY
 *
 * Implements a DNS event proxy.
 *
 * @param ClassName the name of the class
 * @param Function the name of the function in that class
 */
#define IMPL_DNSEVENTPROXY(ClassName, Function) \
	void DnsEventProxy##ClassName##Function(void *Object, hostent *Response) { \
		((ClassName *)Object)->Function(Response); \
	}

/**
 * USE_DNSEVENTPROXY
 *
 * Returns the name of a DNS event proxy function.
 *
 * @param ClassName the name of the class
 * @param Function the name of the function in that class
 */
#define USE_DNSEVENTPROXY(ClassName, Function) DnsEventProxy##ClassName##Function

#else
class CDnsQuery;
#endif
