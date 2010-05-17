/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007,2010 Gunnar Beutner                                 *
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

#ifndef SOCKETEVENTS_H
#define SOCKETEVENTS_H

/**
 * CSocketEvents
 *
 * An interface for socket events.
 */
struct CSocketEvents {
public:
	/**
	 * ~CSocketEvents
	 *
	 * Destructor.
	 */
	virtual ~CSocketEvents(void) {}

	/**
	 * Destroy
	 *
	 * Used for destroying the object and the underlying object.
	 */
	virtual void Destroy(void) = 0;

	/**
	 * Read
	 *
	 * Called when the socket is ready for reading.
	 *
	 * @param DontProcess determines whether the function should
	 *					  process the data
	 */
	virtual int Read(bool DontProcess = false) = 0;

	/**
	 * Write
	 *
	 * Called when the socket is ready for writing.
	 */
	virtual int Write(void) = 0;

	/**
	 * Error
	 *
	 * Called when an error occured on the socket.
	 *
	 * @param ErrorCode the error code (errno or winsock error code)
	 */
	virtual void Error(int ErrorCode) = 0;

	/**
	 * HasQueuedData
	 *
	 * Called to determine whether the object wants to write
	 * data for the socket.
	 */
	virtual bool HasQueuedData(void) const = 0;

	/**
	 * ShouldDestroy
	 *
	 * Called to determine whether the event object should be destroyed.
	 */
	virtual bool ShouldDestroy(void) const = 0;

	/**
	 * GetClassName
	 *
	 * Called to get the class' name.
	 */
	virtual const char *GetClassName(void) const = 0;
};

#endif /* SOCKETEVENTS_H */
