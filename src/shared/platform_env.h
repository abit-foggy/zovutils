#ifndef ZOVUTILS_PLATFORM_ENV_H
#define ZOVUTILS_PLATFORM_ENV_H

#include <stdlib.h>

static inline int zov_posixly_correct(void) {
    return getenv("POSIXLY_CORRECT") != NULL;
}

#endif
