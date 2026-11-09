#ifndef ZOVUTILS_PLATFORM_COPY_H
#define ZOVUTILS_PLATFORM_COPY_H

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <direct.h>
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISLNK
#define S_ISLNK(m) 0
#endif
#define lstat(p, s) stat(p, s)
#define zov_realpath_copy(p, r) _fullpath(r, p, (r) ? PATH_MAX : 0)
#else
#include <unistd.h>
#define zov_realpath_copy realpath
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    uint64_t dev;
    uint64_t ino;
    uint32_t mode;
    int32_t is_dir;
    int64_t size;
    int64_t mtime;
    int32_t is_link;
    int32_t is_reg;
} ZovCpStat;

static inline int zov_cp_stat(const char *path, int follow, void *out_ptr) {
    ZovCpStat *out = (ZovCpStat *)out_ptr;
    struct stat st;
    int rc = follow ? stat(path, &st) : lstat(path, &st);
    if (rc != 0) return errno;
    if (out) {
        out->dev = (uint64_t)st.st_dev;
        out->ino = (uint64_t)st.st_ino;
        out->mode = (uint32_t)st.st_mode;
        out->is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
        out->size = (int64_t)st.st_size;
        out->mtime = (int64_t)st.st_mtime;
        out->is_link = S_ISLNK(st.st_mode) ? 1 : 0;
        out->is_reg = S_ISREG(st.st_mode) ? 1 : 0;
    }
    return 0;
}

static inline int zov_cp_fstat(int fd, void *out_ptr) {
    ZovCpStat *out = (ZovCpStat *)out_ptr;
    struct stat st;
    int rc = fstat(fd, &st);
    if (rc != 0) return errno;
    if (out) {
        out->dev = (uint64_t)st.st_dev;
        out->ino = (uint64_t)st.st_ino;
        out->mode = (uint32_t)st.st_mode;
        out->is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
        out->size = (int64_t)st.st_size;
        out->mtime = (int64_t)st.st_mtime;
        out->is_link = S_ISLNK(st.st_mode) ? 1 : 0;
        out->is_reg = S_ISREG(st.st_mode) ? 1 : 0;
    }
    return 0;
}

static inline int zov_cp_destination_inside(const char *source, const char *dest) {
    char *source_real = zov_realpath_copy(source, NULL);
    if (source_real == NULL) return 0;
    char *copy = strdup(dest);
    if (copy == NULL) { free(source_real); return 0; }
    char *slash = strrchr(copy, '/');
    char *bslash = strrchr(copy, '\\');
    if (bslash > slash) slash = bslash;
    if (slash == NULL) strcpy(copy, ".");
    else if (slash == copy) slash[1] = '\0';
    else *slash = '\0';
    char *parent_real = zov_realpath_copy(copy, NULL);
    free(copy);
    if (parent_real == NULL) { free(source_real); return 0; }
    size_t n = strlen(source_real);
    int inside = strncmp(parent_real, source_real, n) == 0
        && (parent_real[n] == '/' || parent_real[n] == '\\' || parent_real[n] == '\0');
    free(source_real); free(parent_real); return inside;
}

static inline int zov_cp_path_contains(const char *parent, const char *child) {
    char *a = zov_realpath_copy(parent, NULL), *b = zov_realpath_copy(child, NULL);
    if (a == NULL || b == NULL) { free(a); free(b); return 0; }
    size_t n = strlen(a);
    int result = strncmp(b, a, n) == 0 && (b[n] == '/' || b[n] == '\\' || b[n] == '\0');
    free(a); free(b); return result;
}

static inline uint32_t zov_cp_create_mode(uint32_t src_mode) {
    mode_t old_mask = umask(0);
    umask(old_mask);
    return (uint32_t)(((src_mode & 0777) & ~old_mask));
}

static inline int zov_cp_copy_data(int in_fd, int out_fd) {
    char buffer[65536];
    for (;;) {
        ssize_t n = read(in_fd, buffer, sizeof(buffer));
        if (n == 0) break;
        if (n < 0) {
            if (errno == EINTR) continue;
            return errno;
        }
        ssize_t offset = 0;
        while (offset < n) {
            ssize_t written = write(out_fd, buffer + offset, (size_t)(n - offset));
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) return errno ? errno : EIO;
            offset += written;
        }
    }
    return 0;
}

static inline int zov_cp_preserve_fd(int fd, uint32_t mode, int64_t atime_sec, int64_t atime_nsec, int64_t mtime_sec, int64_t mtime_nsec) {
#if defined(_WIN32)
    (void)fd; (void)mode; (void)atime_sec; (void)atime_nsec; (void)mtime_sec; (void)mtime_nsec;
    return 0;
#else
    int status = 0;
    if (fchmod(fd, (mode_t)(mode & 07777)) != 0) status = errno;
    struct timespec times[2];
    times[0].tv_sec = (time_t)atime_sec;
    times[0].tv_nsec = (long)atime_nsec;
    times[1].tv_sec = (time_t)mtime_sec;
    times[1].tv_nsec = (long)mtime_nsec;
    if (futimens(fd, times) != 0) {
        if (!status) status = errno;
    }
    return status;
#endif
}

static inline int zov_cp_preserve_path(const char *path, uint32_t mode, int64_t atime_sec, int64_t atime_nsec, int64_t mtime_sec, int64_t mtime_nsec) {
#if defined(_WIN32)
    (void)path; (void)mode; (void)atime_sec; (void)atime_nsec; (void)mtime_sec; (void)mtime_nsec;
    return 0;
#else
    int status = 0;
    if (chmod(path, (mode_t)(mode & 07777)) != 0) status = errno;
    struct timespec times[2];
    times[0].tv_sec = (time_t)atime_sec;
    times[0].tv_nsec = (long)atime_nsec;
    times[1].tv_sec = (time_t)mtime_sec;
    times[1].tv_nsec = (long)mtime_nsec;
    if (utimensat(AT_FDCWD, path, times, 0) != 0) {
        if (!status) status = errno;
    }
    return status;
#endif
}

static inline int zov_cp_open_read(const char *path) {
    return open(path, O_RDONLY | O_BINARY);
}

static inline int zov_cp_open_write(const char *path, uint32_t mode) {
    return open(path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, (mode_t)mode);
}

static inline int zov_cp_close(int fd) {
    return close(fd) == 0 ? 0 : errno;
}

static inline int zov_cp_is_same(uint64_t dev_a, uint64_t ino_a, uint64_t dev_b, uint64_t ino_b) {
    return dev_a == dev_b && ino_a == ino_b;
}

static inline int zov_cp_readlink(const char *path, char *buf, size_t cap) {
#if defined(_WIN32)
    (void)path; (void)buf; (void)cap;
    return EINVAL;
#else
    ssize_t n = readlink(path, buf, cap - 1);
    if (n < 0) return errno;
    buf[n] = '\0';
    return 0;
#endif
}

static inline int zov_cp_is_exdev(int err) {
    return err == EXDEV;
}

static inline int zov_cp_is_enoent(int err) {
    return err == ENOENT;
}

static inline int zov_cp_is_enotdir(int err) {
    return err == ENOTDIR;
}

static inline int zov_cp_is_eexist(int err) {
    return err == EEXIST;
}

#endif
