#ifndef ZOVUTILS_PLATFORM_ERRNO_H
#define ZOVUTILS_PLATFORM_ERRNO_H

#include <errno.h>
#include <stdio.h>
#include <string.h>

static inline void zov_report_wc_write_error(void) {
    int error = errno;
#ifdef __APPLE__
    if (error == 0) {
        error = EPIPE;
    }
#endif
    fprintf(stderr, "wc: write error");
    if (error != 0) {
        fprintf(stderr, ": %s\n", strerror(error));
    } else {
        fputc('\n', stderr);
    }
}

static inline int zov_errno_value(void) {
    return errno;
}

static inline void zov_errno_set(int value) {
    errno = value;
}

static inline int zov_errno_interrupted(void) {
    return errno == EINTR;
}

static inline int zov_errno_permission_denied(void) {
    return errno == EPERM;
}

static inline int zov_errno_would_block(void) {
    return errno == EAGAIN || errno == EWOULDBLOCK;
}

#endif
