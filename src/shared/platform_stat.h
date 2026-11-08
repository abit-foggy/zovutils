#ifndef ZOVUTILS_PLATFORM_STAT_H
#define ZOVUTILS_PLATFORM_STAT_H

#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/sysmacros.h>
#endif

static inline int zov_file_is_regular(int descriptor) {
    struct stat info;
    return fstat(descriptor, &info) == 0 && S_ISREG(info.st_mode);
}

static inline int zov_file_is_character(int descriptor) {
    struct stat info;
    return fstat(descriptor, &info) == 0 && S_ISCHR(info.st_mode);
}

static inline int zov_file_is_append(int descriptor) {
    int flags = fcntl(descriptor, F_GETFL);
    return flags >= 0 && (flags & O_APPEND) != 0;
}

static inline int zov_same_file(int descriptor, const char *path) {
    struct stat descriptor_info;
    struct stat path_info;
    return fstat(descriptor, &descriptor_info) == 0
        && stat(path, &path_info) == 0
        && descriptor_info.st_dev == path_info.st_dev
        && descriptor_info.st_ino == path_info.st_ino;
}

static inline int zov_same_path(const char *first, const char *second) {
    struct stat first_info;
    struct stat second_info;
    return stat(first, &first_info) == 0
        && stat(second, &second_info) == 0
        && first_info.st_dev == second_info.st_dev
        && first_info.st_ino == second_info.st_ino;
}

static inline int zov_same_open_file(int first, int second) {
    struct stat first_info;
    struct stat second_info;
    return fstat(first, &first_info) == 0
        && fstat(second, &second_info) == 0
        && first_info.st_dev == second_info.st_dev
        && first_info.st_ino == second_info.st_ino;
}

static inline long long zov_regular_bytes_remaining(int descriptor) {
    struct stat info;
    if (fstat(descriptor, &info) != 0 || !S_ISREG(info.st_mode)
        || info.st_size <= info.st_blksize)
        return -1;
    off_t position = lseek(descriptor, 0, SEEK_CUR);
    if (position < 0)
        return -1;
    if (position >= info.st_size)
        return 0;
    off_t end = lseek(descriptor, 0, SEEK_END);
    if (end < 0)
        return -1;
    if (end < position) {
        if (lseek(descriptor, position, SEEK_SET) < 0)
            return -1;
        return 0;
    }
    return (long long) (end - position);
}

static inline int zov_stat_size(const char *path, int64_t *size_out) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    *size_out = (int64_t)st.st_size;
    return 0;
}

static inline int zov_open_for_truncate(const char *path, int create) {
    int flags = O_WRONLY | (create ? O_CREAT : 0);
    return open(path, flags, 0666);
}

static inline int zov_file_truncate(int fd, int64_t target) {
    return ftruncate(fd, (off_t)target);
}

static inline int zov_close_fd(int fd) {
    return close(fd);
}

static inline int zov_mkfifo(const char *path, uint32_t mode) {
    return mkfifo(path, ((mode_t)mode) & 07777);
}

static inline int zov_mknod_device(const char *path, char type, uint32_t mode, uint32_t maj, uint32_t min) {
#if defined(__linux__) || defined(__APPLE__)
    mode_t kind = (type == 'b' ? S_IFBLK : S_IFCHR);
    return mknod(path, kind | ((mode_t)mode & 07777), makedev(maj, min));
#else
    errno = ENOSYS;
    return -1;
#endif
}

static inline uint32_t zov_get_umask(void) {
    mode_t mask = umask(0);
    umask(mask);
    return (uint32_t)mask;
}

static inline int zov_mkdir(const char *path, uint32_t mode) {
    return mkdir(path, (mode_t)mode);
}

static inline int zov_chmod(const char *path, uint32_t mode) {
    return chmod(path, (mode_t)mode);
}

static inline int zov_is_directory(const char *path) {
    struct stat info;
    if (stat(path, &info) != 0) return 0;
    return S_ISDIR(info.st_mode);
}

static inline int zov_context_enabled(void) {
#if defined(__linux__)
    return access("/sys/fs/selinux/enforce", F_OK) == 0 ||
           access("/sys/fs/smackfs", F_OK) == 0 ||
           access("/smack", F_OK) == 0;
#else
    return 0;
#endif
}

static inline int zov_open_wronly(const char *path) {
    return open(path, O_WRONLY);
}

static inline int zov_fsync(int fd) {
    return fsync(fd);
}

#endif
