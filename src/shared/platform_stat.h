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

#endif
