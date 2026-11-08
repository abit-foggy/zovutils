#ifndef ZOV_PLATFORM_IO_H
#define ZOV_PLATFORM_IO_H

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static inline void zov_set_unbuffered(FILE *fp) {
    setvbuf(fp, NULL, _IONBF, 0);
}

static inline size_t zov_wc_count_newlines(const unsigned char *buffer, size_t length) {
    const unsigned char *cursor = buffer;
    size_t remaining = length;
    size_t count = 0;
    while (remaining != 0) {
        const unsigned char *found = (const unsigned char *)memchr(cursor, '\n', remaining);
        if (found == NULL) break;
        ++count;
        remaining -= (size_t)(found - cursor) + 1;
        cursor = found + 1;
    }
    return count;
}

static inline int zov_wc_newline_backend(void) {
#if defined(__linux__) && defined(__aarch64__)
    return 3;
#endif
#if defined(__x86_64__)
    unsigned int eax, ebx, ecx, edx;
    unsigned int leaf1_ecx;
    unsigned long long xcr0;
    __asm__ __volatile__("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1u), "c"(0u));
    leaf1_ecx = ecx;
    if ((leaf1_ecx & (1u << 27)) == 0 || (leaf1_ecx & (1u << 28)) == 0) return 0;
    __asm__ __volatile__(".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) : "c"(0u));
    xcr0 = ((unsigned long long)edx << 32) | eax;
    __asm__ __volatile__("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(7u), "c"(0u));
    if ((xcr0 & 0xe6u) == 0xe6u && (ebx & (1u << 16)) != 0 && (ebx & (1u << 30)) != 0) return 2;
    if ((xcr0 & 0x6u) == 0x6u && (ebx & (1u << 5)) != 0) return 1;
#endif
    return 0;
}

static inline int zov_fseeko(FILE *fp, int64_t offset, int whence) {
    return fseeko(fp, (off_t)offset, whence);
}

#endif
