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

CFIFOBuffer::CFIFOBuffer() {
	m_Buffer = NULL;
	m_BufferSize = 0;
	m_Offset = 0;
}

CFIFOBuffer::~CFIFOBuffer() {
	free(m_Buffer);
}

void* CFIFOBuffer::ResizeBuffer(void* Buffer, unsigned int OldSize, unsigned int NewSize) {
	if (OldSize != 0)
		OldSize += BLOCKSIZE - (OldSize % BLOCKSIZE);

	unsigned int CeilNewSize = NewSize + BLOCKSIZE - (NewSize % BLOCKSIZE);

	unsigned int OldBlocks = OldSize / BLOCKSIZE;
	unsigned int NewBlocks = CeilNewSize / BLOCKSIZE;

	if (NewBlocks != OldBlocks)
		return realloc(Buffer, NewBlocks * BLOCKSIZE);
	else
		return Buffer;
}

void CFIFOBuffer::Optimize(void) {
	if (m_Offset <= OPTIMIZEBLOCKS * BLOCKSIZE)
		return;

	char* NewBuffer = (char*)ResizeBuffer(NULL, 0, m_BufferSize - m_Offset);

	if (NewBuffer == NULL)
		return;

	memcpy(NewBuffer, m_Buffer + m_Offset, m_BufferSize - m_Offset);

	free(m_Buffer);
	m_Buffer = NewBuffer;
	m_BufferSize -= m_Offset;
	m_Offset = 0;
}

unsigned int CFIFOBuffer::GetSize(void) {
	return m_BufferSize - m_Offset;
}

char* CFIFOBuffer::Peek(void) {
	return m_Buffer + m_Offset;
}

char* CFIFOBuffer::Read(unsigned int Bytes) {
	char* ReturnValue = m_Buffer + m_Offset;
	m_Offset += Bytes > GetSize() ? GetSize() : Bytes;

	Optimize();

	return ReturnValue;
}

void CFIFOBuffer::Write(const char* Buffer, unsigned int Size) {
	char* tempBuffer = (char*)ResizeBuffer(m_Buffer, m_BufferSize, m_BufferSize + Size);

	if (tempBuffer == NULL) {
		LOGERROR("realloc() failed. Lost %d bytes.", Size);

		return;
	}

	m_Buffer = tempBuffer;
	memcpy(m_Buffer + m_BufferSize, Buffer, Size);
	m_BufferSize += Size;
}

void CFIFOBuffer::WriteLine(const char* Line) {
	unsigned int Len = strlen(Line);

	char* tempBuffer = (char*)ResizeBuffer(m_Buffer, m_BufferSize, m_BufferSize + Len + 2);

	if (tempBuffer == NULL) {
		LOGERROR("realloc failed(). Lost %d bytes.", Len + 2);

		return;
	}

	m_Buffer = tempBuffer;
	memcpy(m_Buffer + m_BufferSize, Line, Len);
	memcpy(m_Buffer + m_BufferSize + Len, "\r\n", 2);
	m_BufferSize += Len + 2;
}

void CFIFOBuffer::Flush(void) {
	Read(GetSize());
}
