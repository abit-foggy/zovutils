#ifndef ZOVUTILS_PLATFORM_LS_H
#define ZOVUTILS_PLATFORM_LS_H

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

#if defined(__linux__)
#include <sys/xattr.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    int64_t size;
    int64_t blocks;
    uint64_t nlink;
    int64_t mtime_sec;
    int64_t mtime_nsec;
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    int32_t is_dir;
    int32_t is_link;
    int32_t padding;
} ZovLsStat;

static inline int zov_ls_lstat(const char *path, void *out_ptr) {
    ZovLsStat *out = (ZovLsStat *)out_ptr;
    struct stat st;
    if (lstat(path, &st) != 0) return errno;
    if (out) {
        out->size = (int64_t)st.st_size;
        out->blocks = (int64_t)st.st_blocks;
        out->nlink = (uint64_t)st.st_nlink;
        out->uid = (uint32_t)st.st_uid;
        out->gid = (uint32_t)st.st_gid;
        out->mode = (uint32_t)st.st_mode;
        out->mtime_sec = (int64_t)st.st_mtime;
#if defined(__APPLE__)
        out->mtime_nsec = (int64_t)st.st_mtimespec.tv_nsec;
#elif defined(__linux__)
        out->mtime_nsec = (int64_t)st.st_mtim.tv_nsec;
#else
        out->mtime_nsec = 0;
#endif
        out->is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
        out->is_link = S_ISLNK(st.st_mode) ? 1 : 0;
        out->padding = 0;
    }
    return 0;
}

static inline int zov_ls_has_selinux_xattr(const char *path) {
#if defined(__linux__)
    return lgetxattr(path, "security.selinux", NULL, 0) > 0 ? 1 : 0;
#else
    (void)path;
    return 0;
#endif
}

static inline void zov_ls_format_time(int64_t mtime, char *buf, size_t buflen) {
    time_t now = time(NULL);
    time_t sec = (time_t)mtime;
    struct tm t;
    localtime_r(&sec, &t);
    if (sec > now + 15552000 || sec < now - 15552000) {
        strftime(buf, buflen, "%b %e  %Y", &t);
    } else {
        strftime(buf, buflen, "%b %e %H:%M", &t);
    }
}

static inline int zov_ls_readlink(const char *path, char *buf, size_t bufsiz) {
    ssize_t n = readlink(path, buf, bufsiz - 1);
    if (n < 0) return errno;
    buf[n] = '\0';
    return 0;
}

static inline void zov_ls_print_long(
    const char *mode,
    char marker,
    unsigned long nlink,
    const char *owner,
    const char *group,
    int sw,
    long long size,
    const char *date,
    const char *name,
    const char *link_target,
    int apple_all_long
) {
#if defined(__APPLE__)
    if (apple_all_long && strcmp(name, "..") != 0)
        printf("%s%c %lu %s %s %*lld %s %s", mode, marker, nlink, owner, group, sw, size, date, name);
    else
        printf("%s%c%s%lu %s %s %*lld %s %s", mode, marker, marker == ' ' ? "" : " ", nlink, owner, group, sw, size, date, name);
#else
    (void)apple_all_long;
    printf("%s%c%s%lu %s %s %*lld %s %s", mode, marker, marker == ' ' ? "" : " ", nlink, owner, group, sw, size, date, name);
#endif
    if (link_target && link_target[0]) {
        printf(" -> %s", link_target);
    }
    putchar('\n');
}

#endif
