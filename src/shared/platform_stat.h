#ifndef ZOVUTILS_PLATFORM_STAT_H
#define ZOVUTILS_PLATFORM_STAT_H

#include <sys/stat.h>
#include <fcntl.h>

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

#endif
