#ifndef ZOV_PLATFORM_IO_H
#define ZOV_PLATFORM_IO_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static inline void *zov_alloc_read_buffer(void) {
    void *buffer = NULL;
    /* Direct-I/O descriptors can require both the address and size aligned. */
    int error = posix_memalign(&buffer, 65536, 65536);
    if (error != 0) {
        errno = error;
        return NULL;
    }
    return buffer;
}

static inline void *zov_alloc_write_buffer(void) {
    return zov_alloc_read_buffer();
}

static inline int zov_prepare_direct_output(FILE *output) {
#ifdef O_DIRECT
    int flags = fcntl(fileno(output), F_GETFL);
    if (flags >= 0 && (flags & O_DIRECT)) {
        if (fflush(output) != 0) {
            return 0;
        }
        return setvbuf(output, NULL, _IONBF, 0) == 0;
    }
#else
    (void) output;
#endif
    return 1;
}

static inline void zov_free_read_buffer(void *buffer) {
    free(buffer);
}

#endif
