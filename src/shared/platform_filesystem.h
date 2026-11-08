#ifndef ZOVUTILS_PLATFORM_FILESYSTEM_H
#define ZOVUTILS_PLATFORM_FILESYSTEM_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__linux__)
#include <mntent.h>
#endif
#if defined(__APPLE__)
#include <sys/mount.h>
#endif

typedef struct {
    uint64_t f_bsize;
    uint64_t f_frsize;
    uint64_t f_blocks;
    uint64_t f_bfree;
    uint64_t f_bavail;
    uint64_t f_files;
    uint64_t f_ffree;
} ZovStatvfs;

static inline int zov_statvfs(const char *path, void *out_ptr) {
    struct statvfs v;
    if (statvfs(path, &v) < 0) {
        return errno;
    }
    if (out_ptr) {
        ZovStatvfs *out = (ZovStatvfs *)out_ptr;
        out->f_bsize = (uint64_t)v.f_bsize;
        out->f_frsize = (uint64_t)(v.f_frsize ? v.f_frsize : v.f_bsize);
        out->f_blocks = (uint64_t)v.f_blocks;
        out->f_bfree = (uint64_t)v.f_bfree;
        out->f_bavail = (uint64_t)v.f_bavail;
        out->f_files = (uint64_t)v.f_files;
        out->f_ffree = (uint64_t)v.f_ffree;
    }
    return 0;
}

static inline void zov_df_mount(const char *path, char *mount_buf, size_t mount_sz) {
    if (mount_sz == 0) return;
    strcpy(mount_buf, "/");
#if defined(__linux__)
    FILE *f = setmntent("/proc/self/mounts", "r");
    if (!f) return;
    struct mntent *m;
    size_t best = 0;
    while ((m = getmntent(f))) {
        size_t z = strlen(m->mnt_dir);
        if (z >= best && !strncmp(path, m->mnt_dir, z) && (path[z] == 0 || path[z] == '/' || z == 1)) {
            best = z;
            snprintf(mount_buf, mount_sz, "%s", m->mnt_dir);
        }
    }
    endmntent(f);
#elif defined(__APPLE__)
    struct statfs *m;
    int nfs = getmntinfo(&m, MNT_NOWAIT);
    size_t best = 0;
    for (int i = 0; i < nfs; i++) {
        size_t z = strlen(m[i].f_mntonname);
        if (z >= best && !strncmp(path, m[i].f_mntonname, z)) {
            best = z;
            snprintf(mount_buf, mount_sz, "%s", m[i].f_mntonname);
        }
    }
#else
    (void)path;
#endif
}

static inline void zov_df_source_type(const char *target, char *fs, size_t fsn, char *type, size_t typen) {
    if (fsn > 0) snprintf(fs, fsn, "%s", target);
    if (typen > 0) type[0] = '\0';
#if defined(__linux__)
    FILE *f = setmntent("/proc/self/mounts", "r");
    if (f) {
        struct mntent *m;
        while ((m = getmntent(f))) {
            if (!strcmp(m->mnt_dir, target)) {
                if (fsn > 0) snprintf(fs, fsn, "%s", m->mnt_fsname);
                if (typen > 0) snprintf(type, typen, "%s", m->mnt_type);
                break;
            }
        }
        endmntent(f);
    }
#elif defined(__APPLE__)
    struct statfs *m;
    int n = getmntinfo(&m, MNT_NOWAIT);
    for (int i = 0; i < n; i++) {
        if (!strcmp(m[i].f_mntonname, target)) {
            if (fsn > 0) snprintf(fs, fsn, "%s", m[i].f_mntfromname);
            if (typen > 0) snprintf(type, typen, "%s", m[i].f_fstypename);
            break;
        }
    }
#endif
}

static inline void zov_df_human(uint64_t bytes, int base, char *out, size_t cap) {
    static const char units[] = "BKMGTPE";
    uint64_t divisor = 1;
    int unit = 0;
    while (unit < 6 && bytes / divisor >= (uint64_t)base) {
        if (divisor > (~0ULL) / (uint64_t)base) break;
        divisor *= (uint64_t)base;
        unit++;
    }
    uint64_t value = (bytes + divisor - 1) / divisor;
    if (unit && value >= (uint64_t)base && unit < 6) {
        divisor *= (uint64_t)base;
        value = (bytes + divisor - 1) / divisor;
        unit++;
    }
    if (unit) snprintf(out, cap, "%llu%c", (unsigned long long)value, units[unit]);
    else snprintf(out, cap, "%llu", (unsigned long long)value);
}

#endif
