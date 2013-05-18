/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2011 Gunnar Beutner                                      *
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
 * CFIFOBuffer
 *
 * Constructs a new fifo buffer.
 */
CFIFOBuffer::CFIFOBuffer() {
	m_Buffer = NULL;
	m_BufferSize = 0;
	m_Offset = 0;
}

/**
 * ~CFIFOBuffer
 *
 * Destructs a fifo buffer.
 */
CFIFOBuffer::~CFIFOBuffer() {
	free(m_Buffer);
}

/**
 * ResizeBuffer
 *
 * Resizes a buffer. The new size of the buffer will be a multiple
 * of BLOCKSIZE. NULL is returned if the buffer could not be successfully
 * resized.
 *
 * @param Buffer the buffer which is to be resized
 * @param OldSize the old size of the buffer
 * @param NewSize the new size of the buffer
 */
void *CFIFOBuffer::ResizeBuffer(void *Buffer, size_t OldSize, size_t NewSize) {
	if (OldSize != 0) {
		OldSize += BLOCKSIZE - (OldSize % BLOCKSIZE);
	}

	size_t CeilNewSize = NewSize + BLOCKSIZE - (NewSize % BLOCKSIZE);

	size_t OldBlocks = OldSize / BLOCKSIZE;
	size_t NewBlocks = CeilNewSize / BLOCKSIZE;

	if (NewBlocks != OldBlocks) {
		if (NewSize == 0) {
			free(Buffer);

			return NULL;
		} else {
			return realloc(Buffer, NewBlocks * BLOCKSIZE);
		}
	} else {
		return Buffer;
	}
}

/**
 * Optimize
 *
 * Optimizes the memory usage of a buffer.
 */
void CFIFOBuffer::Optimize(void) {
	char *NewBuffer;

	if (m_Offset == 0 || m_Offset < m_BufferSize / 5) {
		return;
	}

	if (m_BufferSize - m_Offset <= 0) {
		free(m_Buffer);

		m_Buffer = NULL;
		m_BufferSize = 0;
		m_Offset = 0;

		return;
	}

	NewBuffer = (char *)ResizeBuffer(NULL, 0, m_BufferSize - m_Offset);

	if (AllocFailed(NewBuffer)) {
		return;
	}

	memcpy(NewBuffer, m_Buffer + m_Offset, m_BufferSize - m_Offset);

	free(m_Buffer);
	m_Buffer = NewBuffer;
	m_BufferSize -= m_Offset;
	m_Offset = 0;
}

/**
 * GetSize
 *
 * Returns the size of the buffer.
 */
size_t CFIFOBuffer::GetSize(void) const {
	return m_BufferSize - m_Offset;
}

/**
 * Peek
 *
 * Returns a pointer to the buffer's data without advancing the read pointer (or
 * NULL if there is no data left in the buffer).
 */
char *CFIFOBuffer::Peek(void) const {
	if (GetSize() == 0) {
		return NULL;
	} else {
		return m_Buffer + m_Offset;
	}
}

/**
 * Reads and returns the specified amount of bytes from the buffer.
 *
 * @param Bytes the number of bytes which should be read from the buffer.
 *              If this value is greater than the size of the buffer,
 *              GetSize() bytes are read instead.
 */
char *CFIFOBuffer::Read(size_t Bytes) {
	char *ReturnValue;

	Optimize();

	ReturnValue = m_Buffer + m_Offset;

	if (Bytes > GetSize()) {
		m_Offset += GetSize();
	} else {
		m_Offset += Bytes;
	}

	return ReturnValue;
}

/**
 * Write
 *
 * Saves data in the buffer.
 *
 * @param Data a pointer to the data
 * @param Size the number of bytes which should be written
 */
RESULT<bool> CFIFOBuffer::Write(const char *Data, size_t Size) {
	char *tempBuffer;

	tempBuffer = (char *)ResizeBuffer(m_Buffer, m_BufferSize,
		m_BufferSize + Size);

	if (AllocFailed(tempBuffer)) {
		THROW(bool, Generic_OutOfMemory, "ResizeBuffer() failed.");
	}

	m_Buffer = tempBuffer;
	memcpy(m_Buffer + m_Offset + m_BufferSize, Data, Size);
	m_BufferSize += Size;

	RETURN(bool, true);
}

/**
 * WriteUnformattedLine
 *
 * Writes a line into the buffer.
 *
 * @param Line the line
 */
RESULT<bool> CFIFOBuffer::WriteUnformattedLine(const char *Line) {
	size_t Length = strlen(Line);

	char *tempBuffer = (char *)ResizeBuffer(m_Buffer, m_BufferSize,
		m_BufferSize + Length + 2);

	if (AllocFailed(tempBuffer)) {
		THROW(bool, Generic_OutOfMemory, "ResizeBuffer() failed.");
	}

	m_Buffer = tempBuffer;
	memcpy(m_Buffer + m_Offset + m_BufferSize, Line, Length);
	memcpy(m_Buffer + m_Offset + m_BufferSize + Length, "\r\n", 2);
	m_BufferSize += Length + 2;

	RETURN(bool, true);
}

/**
 * Flush
 *
 * Removes all data which is currently stored in the buffer.
 */
void CFIFOBuffer::Flush(void) {
	Read(GetSize());
}
