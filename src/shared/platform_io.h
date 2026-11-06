#ifndef ZOV_PLATFORM_IO_H
#define ZOV_PLATFORM_IO_H

#include <stdlib.h>

static inline void *zov_alloc_read_buffer(void) {
    void *buffer = NULL;
    /* Direct-I/O descriptors can require both the address and size aligned. */
    if (posix_memalign(&buffer, 65536, 65536) != 0) {
        return NULL;
    }
    return buffer;
}

static inline void zov_free_read_buffer(void *buffer) {
    free(buffer);
}

#endif
