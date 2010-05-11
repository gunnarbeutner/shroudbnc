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

#define BLOCKSIZE 4096

/**
 * CFIFOBuffer
 *
 * A fifo buffer.
 */
class SBNCAPI CFIFOBuffer {
	char *m_Buffer; /**< the fifo buffer's data */
	size_t m_BufferSize; /**< the size of the buffer */
	size_t m_Offset; /**< the number of unused bytes at the
								beginning of the buffer */

	void *ResizeBuffer(void *Buffer, size_t OldSize, size_t NewSize);
	inline void Optimize(void);
public:
#ifndef SWIG
	CFIFOBuffer();
	virtual ~CFIFOBuffer();
#endif /* SWIG */

	size_t GetSize(void) const;

	char *Peek(void) const;
	char *Read(size_t Bytes);
	void Flush(void);

	RESULT<bool> Write(const char *Data, size_t Size);
	RESULT<bool> WriteUnformattedLine(const char *Line);
};
