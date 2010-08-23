/* MD5.H - header file for MD5C.C
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
rights reserved.

License to copy and use this software is granted provided that it
is identified as the "RSA Data Security, Inc. MD5 Message-Digest
Algorithm" in all material mentioning or referencing this software
or this function.

License is also granted to make and use derivative works provided
that such works are identified as "derived from the RSA Data
Security, Inc. MD5 Message-Digest Algorithm" in all material
mentioning or referencing the derived work.

RSA Data Security, Inc. makes no representations concerning either
the merchantability of this software or the suitability of this
software for any particular purpose. It is provided "as is"
without express or implied warranty of any kind.

These notices must be retained in any copies of any part of this
documentation and/or software.
 */

#ifdef _WIN32
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#endif

/* MD5 context. */
typedef struct {
  uint32_t state[4];                                   /* state (ABCD) */
  uint32_t count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} sMD5_CTX;

void MD5Init(sMD5_CTX *);
void MD5Update(sMD5_CTX *, unsigned char *, unsigned int);
void MD5Final(unsigned char [16], sMD5_CTX *);

typedef struct {
  uint64_t state[4];
  uint64_t count[2];
  unsigned char buffer[64];
} broken_sMD5_CTX;

void broken_MD5Init(broken_sMD5_CTX *);
void broken_MD5Update(broken_sMD5_CTX *, unsigned char *, unsigned int);
void broken_MD5Final(unsigned char [16], broken_sMD5_CTX *);

