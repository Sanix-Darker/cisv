#ifndef WIN_SYS_TIME_H
#define WIN_SYS_TIME_H

#ifdef _WIN32
#include <windows.h>
#include <stdint.h>

// Define timezone structure (dummy implementation)
struct timezone {
    int tz_minuteswest; // minutes west of Greenwich
    int tz_dsttime;     // type of DST correction
};

// Get current time of day (Windows implementation)
static inline int gettimeofday(struct timeval* tv, struct timezone* tz) {
    if (tv) {
        FILETIME ft;
        uint64_t tmpres = 0;

        // Get current time as file time
        GetSystemTimeAsFileTime(&ft);

        // Convert to 64-bit integer
        tmpres |= ft.dwHighDateTime;
        tmpres <<= 32;
        tmpres |= ft.dwLowDateTime;

        // Convert to microseconds (1601-01-01 to 1970-01-01 is 11644473600 seconds)
        tmpres /= 10;  // Convert to microseconds
        tmpres -= 11644473600000000ULL;  // Offset to Unix epoch

        tv->tv_sec = (long)(tmpres / 1000000UL);
        tv->tv_usec = (long)(tmpres % 1000000UL);
    }

    // We don't support timezone on Windows
    if (tz) {
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = 0;
    }

    return 0;
}

#else
// On non-Windows platforms, use the standard header
#include <sys/time.h>
#endif  // _WIN32

#endif  // WIN_SYS_TIME_H
