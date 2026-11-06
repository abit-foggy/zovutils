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

static inline int zov_write_output(FILE *output, const void *buffer, size_t length) {
#ifdef O_DIRECT
    int flags = fcntl(fileno(output), F_GETFL);
    if (flags >= 0 && (flags & O_DIRECT)) {
        const unsigned char *bytes = (const unsigned char *) buffer;
        size_t written = 0;
        int fallback_fd = fileno(output);
        int close_fallback = 0;
        if (length < 65536) {
            char descriptor_path[64];
            (void) snprintf(descriptor_path, sizeof descriptor_path,
                            "/proc/self/fd/%d", fileno(output));
            fallback_fd = open(descriptor_path, O_WRONLY | (flags & O_APPEND));
            off_t position = lseek(fileno(output), 0, SEEK_CUR);
            if (fallback_fd < 0 || position < 0 ||
                lseek(fallback_fd, position, SEEK_SET) < 0) {
                if (fallback_fd >= 0) {
                    (void) close(fallback_fd);
                }
                return 0;
            }
            close_fallback = 1;
        }
        while (written < length) {
            ssize_t count = write(fallback_fd, bytes + written, length - written);
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (close_fallback) (void) close(fallback_fd);
                return 0;
            }
            if (count == 0) {
                if (close_fallback) (void) close(fallback_fd);
                return 0;
            }
            written += (size_t) count;
        }
        return !close_fallback || close(fallback_fd) == 0;
    }
#endif
    return fwrite(buffer, 1, length, output) == length;
}

static inline void zov_free_read_buffer(void *buffer) {
    free(buffer);
}

#endif
