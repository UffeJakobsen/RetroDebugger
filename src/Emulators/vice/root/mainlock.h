/** \file   mainlock.h
 * \brief   VICE mutex used to synchronise access to the VICE api - header
 *
 * Embedded version: USE_VICE_THREAD is never defined, so all mainlock
 * operations are no-ops. This avoids the archdep.h / tick_t dependency
 * chain that the upstream mainlock.h pulls in.
 */

#ifndef VICE_MAIN_LOCK_H
#define VICE_MAIN_LOCK_H

#include "vice.h"

#ifdef USE_VICE_THREAD
#error "USE_VICE_THREAD is not supported in the embedded VICE build"
#endif

#define mainlock_yield()
#define mainlock_yield_begin()
#define mainlock_yield_end()
#define mainlock_yield_and_sleep(ticks)

#define mainlock_obtain()
#define mainlock_release()

#define mainlock_is_vice_thread() (1)

#define mainlock_assert_is_not_vice_thread()
#define mainlock_assert_is_vice_thread()

#endif /* #ifndef VICE_MAIN_LOCK_H */
