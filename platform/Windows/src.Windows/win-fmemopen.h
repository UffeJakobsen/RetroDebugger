#ifndef WIN_FMEMOPEN_H
#define WIN_FMEMOPEN_H

#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

FILE *fmemopen(void *buf, size_t size, const char *mode);

#ifdef __cplusplus
}
#endif

#endif
