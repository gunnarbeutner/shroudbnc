// md5util.cpp : Defines the entry point for the console application.
//

#include "StdAfx.h"

int main(int argc, char* argv[]) {
	if (argc < 2) {
		fprintf(stderr, "md5util\nsyntax: md5util <string>\n");
		return 1;
	}

	const char* String = argv[1];

	MD5_CTX context;
	static char Result[33];
	unsigned char digest[16];
	unsigned int len = strlen(String);

	MD5Init (&context);
	MD5Update (&context, (unsigned char*)String, len);
	MD5Final (digest, &context);

	for (int i = 0; i < 16; i++) {
		sprintf(Result + i * 2, "%02x", digest[i]);
	}

	printf("%s\n", Result);

	return 0;
}
