#ifndef _WIN_GETTIMEOFDAY_
#define _WIN_GETTIMEOFDAY_

#ifdef _WIN32
#include <winsock2.h>
#endif

struct timezone 
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};
 
#ifdef __cplusplus
extern "C" {
#endif
int gettimeofday(struct timeval *tv, struct timezone *tz);
#ifdef __cplusplus
}
#endif

#endif