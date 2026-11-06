#ifndef ZOVUTILS_SLEEP_BACKEND_H
#define ZOVUTILS_SLEEP_BACKEND_H

#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

/* Parse one coreutils sleep interval and wait for it. */
static inline int zov_sleep_interval(const char *text) {
    char *end = NULL;
    errno = 0;
    double amount = strtod(text, &end);
    if (end == text || !isfinite(amount) || amount < 0.0) return 1;

    double multiplier = 1.0;
    if (*end != '\0') {
        if (end[1] != '\0') return 1;
        switch (*end) {
        case 's': multiplier = 1.0; break;
        case 'm': multiplier = 60.0; break;
        case 'h': multiplier = 3600.0; break;
        case 'd': multiplier = 86400.0; break;
        default: return 1;
        }
    }

    double seconds = amount * multiplier;
    if (!isfinite(seconds) || seconds > 9.0e15) return 2;
    time_t whole = (time_t)seconds;
    double fraction = seconds - (double)whole;
    struct timespec request;
    request.tv_sec = whole;
    request.tv_nsec = (long)(fraction * 1000000000.0);
    if (request.tv_nsec >= 1000000000L) {
        request.tv_sec += 1;
        request.tv_nsec -= 1000000000L;
    }

    while (nanosleep(&request, &request) != 0) {
        if (errno != EINTR) return 2;
    }
    return 0;
}

#endif
