#include <windows.h>

static HANDLE usleep_timer = NULL;

void usleep(__int64 usec)
{
    LARGE_INTEGER ft;

    ft.QuadPart = -(10*usec); // Convert to 100 nanosecond interval, negative value indicates relative time

    if (usleep_timer == NULL) {
        usleep_timer = CreateWaitableTimer(NULL, TRUE, NULL);
    }
    SetWaitableTimer(usleep_timer, &ft, 0, NULL, NULL, 0);
    WaitForSingleObject(usleep_timer, INFINITE);
}
