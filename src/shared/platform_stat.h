#ifndef ZOVUTILS_PLATFORM_STAT_H
#define ZOVUTILS_PLATFORM_STAT_H

#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <pwd.h>
#include <grp.h>
#if defined(__linux__)
#include <sys/sysmacros.h>
#endif

#if defined(__APPLE__) || defined(__linux__)
extern time_t timegm(struct tm *);
#endif

#ifndef AT_FDCWD
#define AT_FDCWD -100
#endif
#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW 0x100
#endif
#ifndef AT_SYMLINK_FOLLOW
#define AT_SYMLINK_FOLLOW 0x400
#endif
#ifndef UTIME_NOW
#define UTIME_NOW ((1l << 30) - 1l)
#endif
#ifndef UTIME_OMIT
#define UTIME_OMIT ((1l << 30) - 2l)
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

static inline int64_t zov_utime_now(void) { return (int64_t)UTIME_NOW; }
static inline int64_t zov_utime_omit(void) { return (int64_t)UTIME_OMIT; }

static inline int zov_stat_get_times(const char *path, int64_t *atime_sec, int64_t *atime_nsec, int64_t *mtime_sec, int64_t *mtime_nsec) {
    struct stat st;
    if (stat(path, &st) != 0) return errno;
#if defined(__APPLE__)
    *atime_sec = (int64_t)st.st_atimespec.tv_sec;
    *atime_nsec = (int64_t)st.st_atimespec.tv_nsec;
    *mtime_sec = (int64_t)st.st_mtimespec.tv_sec;
    *mtime_nsec = (int64_t)st.st_mtimespec.tv_nsec;
#else
    *atime_sec = (int64_t)st.st_atim.tv_sec;
    *atime_nsec = (int64_t)st.st_atim.tv_nsec;
    *mtime_sec = (int64_t)st.st_mtim.tv_sec;
    *mtime_nsec = (int64_t)st.st_mtim.tv_nsec;
#endif
    return 0;
}

static inline int zov_touch_create_if_needed(const char *path, int no_create, int no_dereference) {
    if (no_create || no_dereference) return 0;
    struct stat info;
    if (stat(path, &info) != 0 && errno == ENOENT) {
        int fd = open(path, O_WRONLY | O_CREAT, 0666);
        if (fd >= 0) close(fd);
    }
    return 0;
}

static inline int zov_touch_apply(const char *path, int64_t atime_sec, int64_t atime_nsec, int64_t mtime_sec, int64_t mtime_nsec, int no_dereference) {
    struct timespec times[2];
    times[0].tv_sec = (time_t)atime_sec;
    times[0].tv_nsec = (long)atime_nsec;
    times[1].tv_sec = (time_t)mtime_sec;
    times[1].tv_nsec = (long)mtime_nsec;
    if (strcmp(path, "-") == 0) {
        if (futimens(STDOUT_FILENO, times) != 0) {
            int err = errno;
#if !defined(__APPLE__)
            if (err == EINVAL) return 0;
#endif
            if (err != EBADF) return err;
        }
        return 0;
    }
    int flags = no_dereference ? AT_SYMLINK_NOFOLLOW : 0;
    if (utimensat(AT_FDCWD, path, times, flags) != 0) {
        return errno;
    }
    return 0;
}

static inline int64_t zov_time_now_sec(void) {
    return (int64_t)time(NULL);
}

static inline int zov_time_get_local_parts(int64_t sec, int32_t *year, int32_t *mon, int32_t *mday, int32_t *hour, int32_t *min, int32_t *sec_out) {
    time_t t = (time_t)sec;
    struct tm *tm = localtime(&t);
    if (!tm) return 0;
    *year = tm->tm_year + 1900;
    *mon = tm->tm_mon + 1;
    *mday = tm->tm_mday;
    *hour = tm->tm_hour;
    *min = tm->tm_min;
    *sec_out = tm->tm_sec;
    return 1;
}

static inline int zov_time_make_tm(int32_t year, int32_t mon, int32_t mday, int32_t hour, int32_t min, int32_t sec, int is_utc, int64_t *sec_out) {
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = year - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = mday;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;
    tm.tm_isdst = -1;
    time_t result;
    if (is_utc) {
#if defined(__APPLE__) || defined(__linux__)
        result = timegm(&tm);
        if (result == (time_t)-1) return 0;
        struct tm *rt = gmtime(&result);
        if (!rt || rt->tm_year != tm.tm_year || rt->tm_mon != tm.tm_mon ||
            rt->tm_mday != tm.tm_mday || rt->tm_hour != tm.tm_hour ||
            rt->tm_min != tm.tm_min || rt->tm_sec != tm.tm_sec) return 0;
#else
        result = mktime(&tm);
        if (result == (time_t)-1) return 0;
#endif
    } else {
        result = mktime(&tm);
        if (result == (time_t)-1) return 0;
        struct tm *rt = localtime(&result);
        if (!rt || rt->tm_year != tm.tm_year || rt->tm_mon != tm.tm_mon ||
            rt->tm_mday != tm.tm_mday || rt->tm_hour != tm.tm_hour ||
            rt->tm_min != tm.tm_min || rt->tm_sec != tm.tm_sec) return 0;
    }
    *sec_out = (int64_t)result;
    return 1;
}

struct ZovStatInfo {
    int64_t size;
    int64_t blocks;
    int64_t blksize;
    uint64_t ino;
    uint64_t nlink;
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    uint64_t dev;
    int64_t atime;
    int64_t mtime;
    int64_t ctime;
};

static inline int zov_stat_info(const char *path, int follow, void *out_ptr) {
    struct ZovStatInfo *out = (struct ZovStatInfo *)out_ptr;
    struct stat st;
    int rc = follow ? stat(path, &st) : lstat(path, &st);
    if (rc != 0) return errno;
    out->size = (int64_t)st.st_size;
    out->blocks = (int64_t)st.st_blocks;
    out->blksize = (int64_t)st.st_blksize;
    out->ino = (uint64_t)st.st_ino;
    out->nlink = (uint64_t)st.st_nlink;
    out->uid = (uint32_t)st.st_uid;
    out->gid = (uint32_t)st.st_gid;
    out->mode = (uint32_t)st.st_mode;
    out->dev = (uint64_t)st.st_dev;
    out->atime = (int64_t)st.st_atime;
    out->mtime = (int64_t)st.st_mtime;
    out->ctime = (int64_t)st.st_ctime;
    return 0;
}

static inline const char *zov_get_username(uint32_t uid) {
    struct passwd *pw = getpwuid((uid_t)uid);
    return pw ? pw->pw_name : NULL;
}

static inline const char *zov_get_groupname(uint32_t gid) {
    struct group *gr = getgrgid((gid_t)gid);
    return gr ? gr->gr_name : NULL;
}

static inline void zov_format_time(int64_t t, char *buf, size_t buflen) {
    time_t sec = (time_t)t;
    struct tm tmv;
    localtime_r(&sec, &tmv);
    char b[64];
    strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &tmv);
    snprintf(buf, buflen, "%s.000000000 +0000", b);
}

static inline int zov_linkat(const char *source, const char *destination, int follow) {
    if (linkat(AT_FDCWD, source, AT_FDCWD, destination, follow ? AT_SYMLINK_FOLLOW : 0) != 0) {
        return errno;
    }
    return 0;
}

static inline int zov_symlink(const char *target, const char *linkpath) {
    if (symlink(target, linkpath) != 0) {
        return errno;
    }
    return 0;
}

static inline int zov_unlink(const char *pathname) {
    if (unlink(pathname) != 0) {
        return errno;
    }
    return 0;
}

static inline int zov_rename(const char *oldpath, const char *newpath) {
    if (rename(oldpath, newpath) != 0) {
        return errno;
    }
    return 0;
}

static inline int zov_path_lstat_exists(const char *path) {
    struct stat st;
    return lstat(path, &st) == 0;
}

static inline int zov_path_stat_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static inline int zov_path_is_dir(const char *path, int follow) {
    struct stat st;
    int rc = follow ? stat(path, &st) : lstat(path, &st);
    if (rc != 0) return 0;
    return S_ISDIR(st.st_mode);
}

#endif
