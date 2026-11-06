#ifndef ZOVUTILS_PLATFORM_ERRNO_H
#define ZOVUTILS_PLATFORM_ERRNO_H

#include <errno.h>

static inline int zov_errno_interrupted(void) {
    return errno == EINTR;
}

static inline int zov_errno_permission_denied(void) {
    return errno == EPERM;
}

#endif
