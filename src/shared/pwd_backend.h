#ifndef ZOVUTILS_PWD_BACKEND_H
#define ZOVUTILS_PWD_BACKEND_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static inline char *zov_pwd_get(int physical) {
    if (!physical) {
        const char *logical = getenv("PWD");
        if (logical != NULL && logical[0] == '/') {
            struct stat current;
            struct stat candidate;
            if (stat(".", &current) == 0 && stat(logical, &candidate) == 0 &&
                current.st_dev == candidate.st_dev && current.st_ino == candidate.st_ino) {
                size_t length = strlen(logical);
                char *copy = (char *)malloc(length + 1);
                if (copy != NULL) memcpy(copy, logical, length + 1);
                return copy;
            }
        }
    }

    size_t size = 256;
    while (size <= (size_t)-1 / 2) {
        char *buffer = (char *)malloc(size);
        if (buffer == NULL) return NULL;
        if (getcwd(buffer, size) != NULL) return buffer;
        int error = errno;
        free(buffer);
        if (error != ERANGE) {
            errno = error;
            return NULL;
        }
        size *= 2;
    }
    errno = ERANGE;
    return NULL;
}

static inline const char *zov_pwd_error(void) {
    return strerror(errno);
}

#endif
