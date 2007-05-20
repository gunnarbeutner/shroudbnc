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

#include "StdAfx.h"

#if defined(_WIN32) && defined(USESSL)
#include <openssl/applink.c>
#endif

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
