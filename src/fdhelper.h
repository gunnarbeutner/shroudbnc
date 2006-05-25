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

#ifndef _FDHELPER_H
#define _FDHELPER_H

#undef SFD_SETSIZE
#define SFD_SETSIZE 16384

#ifndef _WIN32
typedef long sfd_mask;
#undef SNFDBITS
#define SNFDBITS (8 * sizeof (sfd_mask)) /* bits per mask */
typedef struct _types_sfd_set {
        sfd_mask sfds_bits[SFD_SETSIZE / SNFDBITS];
} _types_sfd_set;

#define SFDS_BITS(set) ((set)->sfds_bits)

#define sfd_set _types_sfd_set

#undef SFD_ZERO
#define SFD_ZERO(set) \
  do { \
    unsigned int __i; \
    sfd_set *__arr = (set); \
    for (__i = 0; __i < sizeof (sfd_set) / sizeof (sfd_mask); ++__i) \
      SFDS_BITS (__arr)[__i] = 0; \
  } while (0)

#define     SFDELT(d)      ((d) / SNFDBITS)
#define     SFDMASK(d)     ((sfd_mask) 1 << ((d) % SNFDBITS))

#undef SFD_SET
#define SFD_SET(d, set)    (SFDS_BITS (set)[SFDELT (d)] |= SFDMASK (d))

#undef SFD_CLR
#define SFD_CLR(d, set)    (SFDS_BITS (set)[SFDELT (d)] &= ~SFDMASK (d))

#undef SFD_ISSET
#define SFD_ISSET(d, set)  (SFDS_BITS (set)[SFDELT (d)] & SFDMASK (d))

#else /* !_WIN32 */
#undef FD_SETSIZE
#define FD_SETSIZE SFD_SETSIZE

#define sfd_set fd_set
#define SFD_ZERO FD_ZERO
#define SFD_SET FD_SET
#define SFD_CLR FD_CLR
#define SFD_ISSET FD_ISSET
#endif

#endif /* _FDHELPER_H */
