#ifndef ZOVUTILS_PLATFORM_TERMINAL_H
#define ZOVUTILS_PLATFORM_TERMINAL_H

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#ifndef ENOTTY
#define ENOTTY 25
#endif
static inline int zov_tcgetattr(int fd, void *termios_buf, size_t *termios_sz, unsigned long *speed_out) {
    (void)fd; (void)termios_buf; (void)termios_sz; (void)speed_out;
    return ENOTTY;
}
#else
#include <termios.h>
#include <unistd.h>

static inline int zov_tcgetattr(int fd, void *termios_buf, size_t *termios_sz, unsigned long *speed_out) {
    struct termios t;
    if (tcgetattr(fd, &t) < 0) {
        return errno;
    }
    if (speed_out) {
        *speed_out = (unsigned long)cfgetospeed(&t);
    }
    if (termios_buf && termios_sz) {
        size_t sz = sizeof(t) < *termios_sz ? sizeof(t) : *termios_sz;
        memcpy(termios_buf, &t, sz);
        *termios_sz = sizeof(t);
    }
    return 0;
}
#endif

#endif
