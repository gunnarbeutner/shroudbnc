/**
 * Based on code from the GNU C library
 */

/* Copyright (C) 1997, 1998, 1999, 2001 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _WIN32
#ifndef FD_SETSIZE
#define FD_SETSIZE 16384
#endif

#define NBBY sizeof(char) /* number of bits in a byte */
typedef long sfd_mask;
#define NFDBITS (sizeof (sfd_mask) * NBBY) /* bits per mask */
#define howmany(x,y) (((x)+((y)-1))/(y))
typedef struct _types_sfd_set {
        sfd_mask fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} _types_sfd_set;

# define FDS_BITS(set) ((set)->fds_bits)

#define sfd_set _types_sfd_set

#undef FD_ZERO
#define FD_ZERO(set)  \
  do {                                                                        \
    unsigned int __i;                                                         \
    fd_set *__arr = (set);                                                    \
    for (__i = 0; __i < sizeof (sfd_set) / sizeof (sfd_mask); ++__i)          \
      FDS_BITS (__arr)[__i] = 0;                                              \
  } while (0)

#define     FDELT(d)      ((d) / NFDBITS)
#define     FDMASK(d)     ((sfd_mask) 1 << ((d) % NFDBITS))

#undef FD_SET
#define FD_SET(d, set)    (FDS_BITS (set)[FDELT (d)] |= FDMASK (d))

#undef FD_CLR
#define FD_CLR(d, set)    (FDS_BITS (set)[FDELT (d)] &= ~FDMASK (d))

#undef FD_ISSET
#define FD_ISSET(d, set)  (FDS_BITS (set)[FDELT (d)] & FDMASK (d))

#define fd_set sfd_set
#endif
