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
    return (long long) (info.st_size - position);
}

#endif
