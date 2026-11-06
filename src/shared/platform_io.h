#ifndef ZOV_PLATFORM_IO_H
#define ZOV_PLATFORM_IO_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* libc hides O_DIRECT under strict POSIX feature-test macros. */
#if defined(__linux__) && !defined(O_DIRECT)
#if defined(__aarch64__)
#define O_DIRECT 0200000
#else
#define O_DIRECT 040000
#endif
#endif

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

static inline int zov_needs_seekdescriptor_sync(void) {
#ifdef __APPLE__
    return 0;
#else
    return 1;
#endif
}

static inline int zov_write_output(FILE *output, const void *buffer, size_t length) {
    int flags = fcntl(fileno(output), F_GETFL);
#ifdef O_DIRECT
    if (flags >= 0 && (flags & O_DIRECT)) {
        const unsigned char *bytes = (const unsigned char *) buffer;
        size_t written = 0;
        int fallback_fd = fileno(output);
        int close_fallback = 0;
        off_t position = -1;
        if (length < 65536) {
            int set_result = fcntl(fileno(output), F_SETFL, flags & ~O_DIRECT);
            if (set_result == 0) {
                int updated_flags = fcntl(fileno(output), F_GETFL);
                if (updated_flags >= 0 && (updated_flags & O_DIRECT) == 0) {
                    while (written < length) {
                        ssize_t count = write(fileno(output), bytes + written,
                                              length - written);
                        if (count < 0 && errno == EINTR) {
                            continue;
                        }
                        if (count <= 0) {
                            (void) fcntl(fileno(output), F_SETFL, flags);
                            return 0;
                        }
                        written += (size_t) count;
                    }
                    if (fcntl(fileno(output), F_SETFL, flags) != 0) {
                        return 0;
                    }
                    return 1;
                }
                (void) fcntl(fileno(output), F_SETFL, flags);
            }
            char descriptor_path[64];
            (void) snprintf(descriptor_path, sizeof descriptor_path,
                            "/proc/self/fd/%d", fileno(output));
            fallback_fd = open(descriptor_path, O_WRONLY | (flags & O_APPEND));
            position = lseek(fileno(output), 0, SEEK_CUR);
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
