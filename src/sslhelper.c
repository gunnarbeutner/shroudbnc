/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007 Gunnar Beutner                                      *
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

/* ====================================================================
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#include "StdAfx.h"

#if defined(_WIN32) && defined(USESSL)
#include <openssl/applink.c>

#include <openssl/bio.h>

#define get_last_socket_error WSAGetLastError()

#undef shutdown

static int safe_sock_new(BIO *bio);
static int safe_sock_free(BIO *bio);
static int safe_sock_read(BIO *bio, char *Buffer, int Len);
static int safe_sock_write(BIO *bio, const char *Buffer, int Len);
static long safe_sock_ctrl(BIO *bio, int cmd, long num, void *ptr);
static int safe_sock_puts(BIO *bio, const char *String);

static BIO_METHOD methods_safe_sockp = {
	BIO_TYPE_SOCKET,
	"safe_socket",
	safe_sock_write,
	safe_sock_read,
	safe_sock_puts,
	NULL, /* gets */
	safe_sock_ctrl,
	safe_sock_new,
	safe_sock_free,
	NULL
};

BIO_METHOD *BIO_s_safe_sock(void) {
	return &methods_safe_sockp;
}

BIO *BIO_new_safe_socket(SOCKET Socket, int CloseFlag) {
	BIO *Result;

	Result = BIO_new(BIO_s_safe_sock());

	if (Result == NULL) {
		return NULL;
	}

	BIO_set_fd(Result, Socket, CloseFlag);

	return Result;
}

static int safe_sock_new(BIO *bio) {
	bio->init = 0;
	bio->num = 0;
	bio->ptr = NULL;
	bio->flags = 0;

	return 1;
}

static int safe_sock_free(BIO *bio) {
	if (bio == NULL) {
		return 0;
	}

	if (bio->shutdown) {
		if (bio->init) {
			safe_shutdown(bio->num, SD_BOTH);
			safe_closesocket(bio->num);
		}

		bio->init = 0;
		bio->flags = 0;
	}

	return 1;
}

static int safe_sock_read(BIO *bio, char *Buffer, int Len) {
	int Result = 0;

	if (Buffer != NULL) {
		Result = safe_recv(bio->num, Buffer, Len, 0);
		WSASetLastError(safe_errno());
		BIO_clear_retry_flags(bio);

		if (Result <= 0) {
			if (BIO_sock_should_retry(Result)) {
				BIO_set_retry_read(bio);
			}
		}
	}

	return Result;
}

static int safe_sock_write(BIO *bio, const char *Buffer, int Len) {
	int Result;

	Result = safe_send(bio->num, Buffer, Len, 0);
	WSASetLastError(safe_errno());
	BIO_clear_retry_flags(bio);

	if (Result <= 0) {
		if (BIO_sock_should_retry(Result)) {
			BIO_set_retry_write(bio);
		}
	}

	return Result;
}

static long safe_sock_ctrl(BIO *bio, int cmd, long num, void *ptr) {
	long Result=1;
	int *ip;

	switch (cmd) {
		case BIO_CTRL_RESET:
			num = 0;
		case BIO_C_FILE_SEEK:
			Result=0;
			break;
		case BIO_C_FILE_TELL:
		case BIO_CTRL_INFO:
			Result = 0;
			break;
		case BIO_C_SET_FD:
			safe_sock_free(bio);
			bio->num = *((int *)ptr);
			bio->shutdown = (int)num;
			bio->init = 1;
			break;
		case BIO_C_GET_FD:
			if (bio->init) {
				ip = (int *)ptr;
				if (ip != NULL) {
					*ip = bio->num;
				}
				Result = bio->num;
			} else {
				Result = -1;
			}
			break;
		case BIO_CTRL_GET_CLOSE:
			Result = bio->shutdown;
			break;
		case BIO_CTRL_SET_CLOSE:
			bio->shutdown = (int)num;
			break;
		case BIO_CTRL_PENDING:
		case BIO_CTRL_WPENDING:
			Result = 0;
			break;
		case BIO_CTRL_DUP:
		case BIO_CTRL_FLUSH:
			Result = 1;
			break;
		default:
			Result = 0;
			break;
	}

	return Result;
}

static int safe_sock_puts(BIO *bio, const char *String) {
	int Len, Result;

	Len = strlen(String);
	Result = safe_sock_write(bio, String, Len);

	return Len;
}
#endif
