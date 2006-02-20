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
void *CFIFOBuffer::ResizeBuffer(void *Buffer, unsigned int OldSize,
								unsigned int NewSize) {
	if (OldSize != 0) {
		OldSize += BLOCKSIZE - (OldSize % BLOCKSIZE);
	}

	unsigned int CeilNewSize = NewSize + BLOCKSIZE - (NewSize % BLOCKSIZE);

	unsigned int OldBlocks = OldSize / BLOCKSIZE;
	unsigned int NewBlocks = CeilNewSize / BLOCKSIZE;

	if (NewBlocks != OldBlocks) {
		return realloc(Buffer, NewBlocks * BLOCKSIZE);
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

	if (m_Offset <= OPTIMIZEBLOCKS * BLOCKSIZE) {
		return;
	}

	NewBuffer = (char *)ResizeBuffer(NULL, 0, m_BufferSize - m_Offset);

	if (NewBuffer == NULL) {
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
unsigned int CFIFOBuffer::GetSize(void) {
	return m_BufferSize - m_Offset;
}

/**
 * Peek
 *
 * Returns a pointer to the buffer's data without advancing the read pointer.
 */
char *CFIFOBuffer::Peek(void) {
	return m_Buffer + m_Offset;
}

/**
 * Reads and returns the specified amount of bytes from the buffer.
 *
 * @param Bytes the number of bytes which should be read from the buffer.
 *              If this value is greater than the size of the buffer,
 *              GetSize() bytes are read instead.
 */
char *CFIFOBuffer::Read(unsigned int Bytes) {
	char* ReturnValue = m_Buffer + m_Offset;

	if (Bytes > GetSize()) {
		m_Offset += GetSize();
	} else {
		m_Offset += Bytes;
	}

	Optimize();

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
void CFIFOBuffer::Write(const char *Data, unsigned int Size) {
	char *tempBuffer;

	tempBuffer = (char *)ResizeBuffer(m_Buffer, m_BufferSize,
		m_BufferSize + Size);

	if (tempBuffer == NULL) {
		LOGERROR("realloc() failed. Lost %d bytes.", Size);

		return;
	}

	m_Buffer = tempBuffer;
	memcpy(m_Buffer + m_BufferSize, Data, Size);
	m_BufferSize += Size;
}

/**
 * WriteUnformattedLine
 *
 * Writes a line into the buffer.
 *
 * @param Line the line
 */
void CFIFOBuffer::WriteUnformattedLine(const char *Line) {
	unsigned int Len = strlen(Line);

	char *tempBuffer = (char *)ResizeBuffer(m_Buffer, m_BufferSize,
		m_BufferSize + Len + 2);

	CHECK_ALLOC_RESULT(tempBuffer, ResizeBuffer) {
		return;
	} CHECK_ALLOC_RESULT_END;

	m_Buffer = tempBuffer;
	memcpy(m_Buffer + m_BufferSize, Line, Len);
	memcpy(m_Buffer + m_BufferSize + Len, "\r\n", 2);
	m_BufferSize += Len + 2;
}

/**
 * Flush
 *
 * Removes all data which is currently stored in the buffer.
 */
void CFIFOBuffer::Flush(void) {
	Read(GetSize());
}
