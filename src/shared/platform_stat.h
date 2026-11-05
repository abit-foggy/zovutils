#ifndef ZOVUTILS_PLATFORM_STAT_H
#define ZOVUTILS_PLATFORM_STAT_H

#include <sys/stat.h>

static inline int zov_file_is_regular(int descriptor) {
    struct stat info;
    return fstat(descriptor, &info) == 0 && S_ISREG(info.st_mode);
}

static inline int zov_file_is_character(int descriptor) {
    struct stat info;
    return fstat(descriptor, &info) == 0 && S_ISCHR(info.st_mode);
}

#endif
