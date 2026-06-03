/** \file   types.h
 * \brief   Type definitions for VICE
 *
 * Based on VICE 3.10 types.h. All code uses C99 stdint types directly.
 */

/*
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#ifndef VICE_TYPES_H
#define VICE_TYPES_H

#include "vice.h"

#include <stdbool.h>

#ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
#else
#  ifdef HAVE_STDINT_H
#    include <stdint.h>
#  else
#    include <stdint.h>
#  endif
#endif

/* According to POSIX including <stdio.h> provides off_t, but on Windows we
 * need to include <sys/types.h> */
#ifdef HAVE_OFF_T
# ifdef HAVE_OFF_T_IN_SYS_TYPES
#  include <sys/types.h>
# endif
#else
/* Fallback */
typedef long long off_t;
#endif


/* === CLOCK type: 64-bit in VICE 3.10 (was 32-bit DWORD in 3.1) === */
typedef uint64_t CLOCK;

/* Maximum value of a CLOCK.  */
#undef CLOCK_MAX
#define CLOCK_MAX (~((CLOCK)0))

/* MSVC <2019 doesn't have PRIu64/PRIx64, which we use to print CLOCK */
#ifndef PRIu64
# if SIZEOF_UNSIGNED_LONG == 8
#  define PRIu64 "lu"
# else
#  define PRIu64 "llu"
# endif
#endif
#ifndef PRIx64
# if SIZEOF_UNSIGNED_LONG == 8
#  define PRIx64 "lx"
# else
#  define PRIx64 "llx"
# endif
#endif

/* MSVC doesn't provide ssize_t (older SDKs) */
#if defined(_MSC_VER) && !defined(_SSIZE_T_DEFINED) && !defined(ssize_t)
#define _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#endif

/* === Pointer conversion macros (VICE 3.10 style using intptr_t) === */
#define vice_ptr_to_int(x)  ((int)(intptr_t)(x))
#define vice_ptr_to_uint(x) ((unsigned int)(uintptr_t)(x))
#define vice_int_to_ptr(x)  ((void *)(intptr_t)(x))
#define vice_uint_to_ptr(x) ((void *)(uintptr_t)(x))

/* Backward-compatibility: old macro names used in embedded 3.1 code */
#define int_to_void_ptr(x)  vice_int_to_ptr(x)
#define uint_to_void_ptr(x) vice_uint_to_ptr(x)


#endif /* VICE_TYPES_H */
