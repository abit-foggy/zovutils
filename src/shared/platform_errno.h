#ifndef ZOVUTILS_PLATFORM_ERRNO_H
#define ZOVUTILS_PLATFORM_ERRNO_H

#include <errno.h>
#include <stdio.h>
#include <string.h>

static inline int zov_platform_is_apple(void) {
#ifdef __APPLE__
    return 1;
#else
    return 0;
#endif
}

static inline void zov_report_wc_write_error(void) {
    int error = errno;
    if (error == 0) {
        error = EPIPE;
    }
    fprintf(stderr, "wc: write error");
    if (error != 0) {
        fprintf(stderr, ": %s\n", strerror(error));
    } else {
        fputc('\n', stderr);
    }
}

static inline int zov_finish_wc_output(void) {
    if (fflush(stdout) != 0 || ferror(stdout) != 0) {
        zov_report_wc_write_error();
        return 0;
    }
    return 1;
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

static inline int zov_errno_is_erange(void) {
    return errno == ERANGE;
}

static inline int zov_errno_is_pipe(int err) {
    return err == EPIPE;
}

static inline int zov_errno_is_enoent(int err) {
    return err == ENOENT;
}

static inline int zov_errno_is_enotempty(int err) {
#ifdef EEXIST
    return err == ENOTEMPTY || err == EEXIST;
#else
    return err == ENOTEMPTY;
#endif
}

static inline int zov_errno_is_enotdir(int err) {
    return err == ENOTDIR;
}

static inline int zov_errno_enotdir_val(void) {
    return ENOTDIR;
}

static inline int zov_errno_is_eexist(int err) {
    return err == EEXIST;
}

static inline int zov_errno_enoent(void) {
    return ENOENT;
}

static inline int zov_errno_enotdir(void) {
    return ENOTDIR;
}

static inline int zov_errno_eexist(void) {
    return EEXIST;
}

static inline int zov_errno_eio(void) {
    return EIO;
}

static inline int zov_erange_value(void) {
    return ERANGE;
}

#endif
