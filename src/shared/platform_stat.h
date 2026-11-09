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
#include <dirent.h>
#include <limits.h>
#if defined(__linux__)
#include <sys/sysmacros.h>
#include <sys/vfs.h>
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

static inline int zov_file_is_seekable_regular(int descriptor) {
    struct stat info;
    if (fstat(descriptor, &info) != 0 || !S_ISREG(info.st_mode)) return 0;
#if defined(__linux__)
    struct statfs sfs;
    if (fstatfs(descriptor, &sfs) == 0) {
        if (sfs.f_type == 0x62656572 || sfs.f_type == 0x9fa0) {
            return 0;
        }
    }
#endif
    return 1;
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

static inline int zov_set_file_times_sec(const char *path, int64_t atime, int64_t mtime) {
    struct timespec times[2];
    times[0].tv_sec = (time_t)atime;
    times[0].tv_nsec = 0;
    times[1].tv_sec = (time_t)mtime;
    times[1].tv_nsec = 0;
    return utimensat(AT_FDCWD, path, times, 0) == 0 ? 0 : errno;
}

static inline int zov_stat_file_times_sec(const char *path, int64_t *atime_out, int64_t *mtime_out) {
    struct stat st;
    if (stat(path, &st) != 0) return errno;
    if (atime_out) *atime_out = (int64_t)st.st_atime;
    if (mtime_out) *mtime_out = (int64_t)st.st_mtime;
    return 0;
}

static inline int zov_mkdirs(const char *p, uint32_t mode) {
    if (!p || !*p) return 0;
    char *b = strdup(p);
    if (!b) return 0;
    for (char *q = b + 1; *q; q++) {
        if (*q == '/') {
            *q = '\0';
            if (*b && strcmp(b, ".") != 0 && strcmp(b, "..") != 0) {
                if (mkdir(b, (mode_t)mode) != 0) {
                    struct stat st;
                    if (errno != EEXIST || stat(b, &st) != 0 || !S_ISDIR(st.st_mode)) {
                        free(b);
                        return 0;
                    }
                }
            }
            *q = '/';
        }
    }
    if (mkdir(b, (mode_t)mode) != 0) {
        struct stat st;
        if (errno != EEXIST || stat(b, &st) != 0 || !S_ISDIR(st.st_mode)) {
            free(b);
            return 0;
        }
    }
    free(b);
    return 1;
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

static inline int zov_du_stat(const char *path, int follow, uint64_t *size_out, uint64_t *blocks_out, uint32_t *mode_out) {
    struct stat st;
    int rc = follow ? stat(path, &st) : lstat(path, &st);
    if (rc != 0) return errno;
    if (size_out) *size_out = (uint64_t)st.st_size;
    if (blocks_out) *blocks_out = (uint64_t)(st.st_blocks > 0 ? (st.st_blocks + 1) / 2 : 0);
    if (mode_out) *mode_out = (uint32_t)st.st_mode;
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

static inline int zov_format_strftime(int64_t sec, int utc, const char *format, char *buf, size_t buflen) {
    time_t t = (time_t)sec;
    struct tm tmv;
    struct tm *pt = utc ? gmtime_r(&t, &tmv) : localtime_r(&t, &tmv);
    if (!pt) return -1;
    size_t n = strftime(buf, buflen, format, pt);
    if (!n && *format) return -2;
    return 0;
}

static inline int zov_stat_mtime(const char *path, int64_t *mtime_out) {
    struct stat st;
    if (stat(path, &st) != 0) return errno;
    if (mtime_out) *mtime_out = (int64_t)st.st_mtime;
    return 0;
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

static inline void *zov_opendir(const char *path) {
    return (void *)opendir(path);
}

static inline const char *zov_readdir(void *dir) {
    struct dirent *ent = readdir((DIR *)dir);
    if (!ent) return NULL;
    return ent->d_name;
}

static inline int zov_closedir(void *dir) {
    return closedir((DIR *)dir);
}

static inline int zov_realpath_is_root(const char *path) {
    char buf[PATH_MAX];
    if (realpath(path, buf) == NULL) return 0;
    return strcmp(buf, "/") == 0;
}

static inline int zov_lchmod(const char *path, uint32_t mode) {
#if defined(__APPLE__)
    return lchmod(path, (mode_t)mode) == 0 ? 0 : errno;
#else
    (void)path; (void)mode;
    return 0;
#endif
}

static inline int zov_stat_mode(const char *path, int follow, uint32_t *mode_out) {
    struct stat st;
    int rc = follow ? stat(path, &st) : lstat(path, &st);
    if (rc != 0) return errno;
    *mode_out = (uint32_t)st.st_mode;
    return 0;
}

static inline int zov_chown(const char *path, uint32_t uid, uint32_t gid) {
    return chown(path, (uid_t)uid, (gid_t)gid) == 0 ? 0 : errno;
}

static inline int zov_lchown(const char *path, uint32_t uid, uint32_t gid) {
    return lchown(path, (uid_t)uid, (gid_t)gid) == 0 ? 0 : errno;
}

static inline int zov_parse_user(const char *text, uint32_t *uid) {
    if (!text || !*text) return 0;
    char *end = NULL;
    unsigned long val = strtoul(text, &end, 10);
    if (end && !*end) {
        *uid = (uint32_t)val;
        return 1;
    }
    struct passwd *pw = getpwnam(text);
    if (!pw) return 0;
    *uid = (uint32_t)pw->pw_uid;
    return 1;
}

static inline int zov_parse_group(const char *text, uint32_t *gid) {
    if (!text || !*text) return 0;
    char *end = NULL;
    unsigned long val = strtoul(text, &end, 10);
    if (end && !*end) {
        *gid = (uint32_t)val;
        return 1;
    }
    struct group *gr = getgrnam(text);
    if (!gr) return 0;
    *gid = (uint32_t)gr->gr_gid;
    return 1;
}

static inline int zov_parse_user_slice(const char *text, size_t len, uint32_t *uid) {
    char buf[128];
    if (len >= sizeof(buf)) return 0;
    memcpy(buf, text, len);
    buf[len] = '\0';
    return zov_parse_user(buf, uid);
}

static inline int zov_parse_group_slice(const char *text, size_t len, uint32_t *gid) {
    char buf[128];
    if (len >= sizeof(buf)) return 0;
    memcpy(buf, text, len);
    buf[len] = '\0';
    return zov_parse_group(buf, gid);
}

static inline int zov_stat_owner(const char *path, int follow, uint32_t *uid_out, uint32_t *gid_out, uint32_t *mode_out) {
    struct stat st;
    int rc = follow ? stat(path, &st) : lstat(path, &st);
    if (rc != 0) return errno;
    if (uid_out) *uid_out = (uint32_t)st.st_uid;
    if (gid_out) *gid_out = (uint32_t)st.st_gid;
    if (mode_out) *mode_out = (uint32_t)st.st_mode;
    return 0;
}

static inline int zov_rmdir(const char *pathname) {
    if (rmdir(pathname) != 0) {
        return errno;
    }
    return 0;
}

static inline int zov_stat_dev_mode(const char *path, uint64_t *dev_out, uint32_t *mode_out) {
    struct stat st;
    if (lstat(path, &st) != 0) return errno;
    if (dev_out) *dev_out = (uint64_t)st.st_dev;
    if (mode_out) *mode_out = (uint32_t)st.st_mode;
    return 0;
}

static inline int zov_test_file(const char *op, const char *path, const char *extra) {
    struct stat st;
    int rc = stat(path, &st);
    if (strcmp(op, "-L") == 0 || strcmp(op, "-h") == 0) {
        return lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
    }
    if (strcmp(op, "-e") == 0 || strcmp(op, "-a") == 0) return rc == 0;
    if (strcmp(op, "-f") == 0) return rc == 0 && S_ISREG(st.st_mode);
    if (strcmp(op, "-d") == 0) return rc == 0 && S_ISDIR(st.st_mode);
    if (strcmp(op, "-b") == 0) return rc == 0 && S_ISBLK(st.st_mode);
    if (strcmp(op, "-c") == 0) return rc == 0 && S_ISCHR(st.st_mode);
    if (strcmp(op, "-p") == 0) return rc == 0 && S_ISFIFO(st.st_mode);
    if (strcmp(op, "-S") == 0) return rc == 0 && S_ISSOCK(st.st_mode);
    if (strcmp(op, "-r") == 0) return access(path, R_OK) == 0;
    if (strcmp(op, "-w") == 0) return access(path, W_OK) == 0;
    if (strcmp(op, "-x") == 0) return access(path, X_OK) == 0;
    if (strcmp(op, "-s") == 0) return rc == 0 && st.st_size > 0;
    if (strcmp(op, "-O") == 0) return rc == 0 && st.st_uid == getuid();
    if (strcmp(op, "-G") == 0) return rc == 0 && st.st_gid == getgid();
    if (strcmp(op, "-t") == 0) return isatty(atoi(path));
    if (strcmp(op, "-N") == 0) return rc == 0 && st.st_mtime > st.st_atime;
    if (strcmp(op, "-ef") == 0 || strcmp(op, "-nt") == 0 || strcmp(op, "-ot") == 0) {
        struct stat other;
        if (rc != 0 || stat(extra, &other) != 0) return 0;
        if (strcmp(op, "-ef") == 0)
            return st.st_dev == other.st_dev && st.st_ino == other.st_ino;
        if (strcmp(op, "-nt") == 0) return st.st_mtime > other.st_mtime;
        return st.st_mtime < other.st_mtime;
    }
    return 0;
}

#endif

