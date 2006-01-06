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

#undef USESSL
#include "StdAfx.h"

extern "C" {
	#include "../md5-c/global.h"
	#include "../md5-c/md5.h"
}

#ifndef _WIN32
	#include <sys/stat.h>

	#define mkdir(X) mkdir(X, 0700)
#else
	#include <direct.h>
	#define mkdir _mkdir
#endif

CBouncerCore* g_Bouncer = NULL;

void string_free(char* string) {
	free(string);
}

const char* md5(const char* String) {
	MD5_CTX context;
	static char Result[33];
	unsigned char digest[16];
	unsigned int len = strlen(String);

	MD5Init (&context);
	MD5Update (&context, (unsigned char*)String, len);
	MD5Final (digest, &context);

#undef sprintf

	for (int i = 0; i < 16; i++) {
		sprintf(Result + i * 2, "%02x", digest[i]);
	}

	return Result;
}

int main(int argc, char* argv[]) {
	puts("shroudBNC" BNCVERSION " - an object-oriented IRC bouncer");
	puts("*** configuration generator");
	puts("");
	puts("This utility will automatically generate a suitable configuration\nfor you once it has asked you some questions.");

	int iPort = 9000;

	printf("1. Which port should the bouncer listen on? [9000] ");
	scanf("%d", &iPort);

	if (iPort <= 1024 || iPort >= 65536) {
		printf("ERROR: You need to specify a valid port between 1025 and 65535.\n");
		return 1;
	}

	char sUser[81], sPassword[81];

	printf("2. What should the first user's name be? ");
	scanf("%s", sUser);

	printf("3. Please enter a password for the first user: ");
	scanf("%s", sPassword);

	char sFile[200];
	sprintf(sFile, "users/%s.conf", sUser);

	rename("sbnc.conf", "sbnc.conf.old");
	mkdir("users");

	CBouncerConfig MainConfig("sbnc.conf");
	CBouncerConfig UserConfig(sFile);

	MainConfig.WriteInteger("system.port", iPort);
	MainConfig.WriteInteger("system.md5", 1);
	MainConfig.WriteString("system.users", sUser);

	UserConfig.WriteString("user.password", md5(sPassword));
	UserConfig.WriteInteger("user.admin", 1);

	puts("Writing config...");

	return 0;
}
