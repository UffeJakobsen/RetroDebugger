/*
 * vicetypes.h - Backward-compatibility redirect to types.h
 *
 * The embedded VICE 3.1 code includes "vicetypes.h" everywhere.
 * VICE 3.10 renamed this to "types.h". This file redirects so that
 * existing code continues to compile during the transition.
 *
 * This file will be removed in Step 11 cleanup.
 */

#ifndef VICE_TYPES_H
/* types.h uses the same include guard, so this redirect is safe */
#include "types.h"
#endif
