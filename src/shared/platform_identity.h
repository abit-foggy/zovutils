#ifndef ZOVUTILS_PLATFORM_IDENTITY_H
#define ZOVUTILS_PLATFORM_IDENTITY_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <grp.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

#if defined(__linux__)
#include <sys/xattr.h>
#endif

#if defined(_WIN32)
#define ZOV_UNAME_OS "Windows_NT"

static inline char *zov_uname_processor(void) {
    char *arch = getenv("PROCESSOR_ARCHITECTURE");
    return arch ? arch : "x86_64";
}

static inline char *zov_uname_os(void) {
    return (char *)ZOV_UNAME_OS;
}

static inline uint32_t zov_getuid(void) { return 1000; }
static inline uint32_t zov_geteuid(void) { return 1000; }
static inline uint32_t zov_getgid(void) { return 1000; }
static inline uint32_t zov_getegid(void) { return 1000; }

static inline char *zov_user_name(uint32_t uid) {
    (void)uid;
    char *u = getenv("USERNAME");
    return u ? u : "user";
}

static inline char *zov_group_name(uint32_t gid) {
    (void)gid;
    return (char *)"Users";
}

static inline int zov_get_uname_fields(
    char *sysname, size_t sysname_sz,
    char *nodename, size_t nodename_sz,
    char *release, size_t release_sz,
    char *version, size_t version_sz,
    char *machine, size_t machine_sz
) {
    snprintf(sysname, sysname_sz, "Windows_NT");
    const char *comp = getenv("COMPUTERNAME");
    snprintf(nodename, nodename_sz, "%s", comp ? comp : "localhost");
    snprintf(release, release_sz, "10.0");
    snprintf(version, version_sz, "Windows");
    const char *arch = getenv("PROCESSOR_ARCHITECTURE");
    snprintf(machine, machine_sz, "%s", arch ? arch : "x86_64");
    return 0;
}

static inline int zov_get_user_primary_gid(const char *username, uint32_t *gid_out, char *canonical_name, size_t name_sz) {
    if (gid_out) *gid_out = 1000;
    if (canonical_name) {
        const char *u = username ? username : getenv("USERNAME");
        snprintf(canonical_name, name_sz, "%s", u ? u : "user");
    }
    return 1;
}

static inline int zov_get_user_groups(const char *username, uint32_t primary_gid, uint32_t *groups_out, int max_groups) {
    (void)username; (void)primary_gid;
    if (max_groups > 0 && groups_out) groups_out[0] = 1000;
    return 1;
}

static inline int zov_get_current_groups(uint32_t *groups_out, int max_groups) {
    if (max_groups > 0 && groups_out) groups_out[0] = 1000;
    return 1;
}
#elif defined(__APPLE__)
#include <mach-o/arch.h>
#include <sys/sysctl.h>

static inline char *zov_uname_processor(void) {
    cpu_type_t cputype;
    size_t size = sizeof(cputype);
    if (sysctlbyname("hw.cputype", &cputype, &size, NULL, 0) == 0) {
        const NXArchInfo *ai = NXGetArchInfoFromCpuType(cputype, CPU_SUBTYPE_MULTIPLE);
        if (ai && ai->name) {
            if (strncmp(ai->name, "arm", 3) == 0) return (char *)"arm";
            if (strncmp(ai->name, "x86", 3) == 0 || strncmp(ai->name, "i386", 4) == 0) return (char *)"i386";
            return (char *)ai->name;
        }
    }
#if defined(__aarch64__) || defined(__arm64__) || defined(__arm__)
    return (char *)"arm";
#elif defined(__x86_64__) || defined(__i386__)
    return (char *)"i386";
#else
    return (char *)"unknown";
#endif
}
#define ZOV_UNAME_OS "Darwin"

static inline char *zov_uname_os(void) {
    return (char *)ZOV_UNAME_OS;
}

static inline uint32_t zov_getuid(void) { return (uint32_t)getuid(); }
static inline uint32_t zov_geteuid(void) { return (uint32_t)geteuid(); }
static inline uint32_t zov_getgid(void) { return (uint32_t)getgid(); }
static inline uint32_t zov_getegid(void) { return (uint32_t)getegid(); }

static inline char *zov_user_name(uint32_t uid) {
    struct passwd *p = getpwuid((uid_t)uid);
    return p ? p->pw_name : NULL;
}

static inline char *zov_group_name(uint32_t gid) {
    struct group *p = getgrgid((gid_t)gid);
    return p ? p->gr_name : NULL;
}

static inline int zov_get_uname_fields(
    char *sysname, size_t sysname_sz,
    char *nodename, size_t nodename_sz,
    char *release, size_t release_sz,
    char *version, size_t version_sz,
    char *machine, size_t machine_sz
) {
    struct utsname u;
    if (uname(&u) != 0) return errno;
    snprintf(sysname, sysname_sz, "%s", u.sysname);
    snprintf(nodename, nodename_sz, "%s", u.nodename);
    snprintf(release, release_sz, "%s", u.release);
    snprintf(version, version_sz, "%s", u.version);
    snprintf(machine, machine_sz, "%s", u.machine);
    return 0;
}

static inline int zov_get_user_primary_gid(const char *username, uint32_t *gid_out, char *canonical_name, size_t name_sz) {
    struct passwd *pw = username ? getpwnam(username) : getpwuid(getuid());
    if (!pw) return 0;
    if (gid_out) *gid_out = (uint32_t)pw->pw_gid;
    if (canonical_name) snprintf(canonical_name, name_sz, "%s", pw->pw_name);
    return 1;
}

static inline int zov_get_user_groups(const char *username, uint32_t primary_gid, uint32_t *groups_out, int max_groups) {
    int n = max_groups;
    gid_t *list = (gid_t *)malloc((size_t)max_groups * sizeof(gid_t));
    if (!list) return -1;
    if (getgrouplist(username, (gid_t)primary_gid, list, &n) < 0) {
        free(list);
        return -n;
    }
    for (int i = 0; i < n; i++) groups_out[i] = (uint32_t)list[i];
    free(list);
    return n;
}

static inline int zov_get_current_groups(uint32_t *groups_out, int max_groups) {
    int n = getgroups(0, NULL);
    if (n < 0) return -1;
    if (max_groups == 0) return n;
    gid_t *list = (gid_t *)malloc((size_t)(n > 0 ? n : 1) * sizeof(gid_t));
    if (!list) return -1;
    if (n > 0 && getgroups(n, list) < 0) {
        free(list);
        return -1;
    }
#if defined(__APPLE__)
    if (n > 16) n = 16;
#endif
    int count = n < max_groups ? n : max_groups;
    for (int i = 0; i < count; i++) groups_out[i] = (uint32_t)list[i];
    free(list);
    return count;
}
#else
static inline char *zov_uname_processor(void) { return (char *)"unknown"; }
#define ZOV_UNAME_OS "GNU/Linux"

static inline char *zov_uname_os(void) {
    return (char *)ZOV_UNAME_OS;
}

static inline uint32_t zov_getuid(void) { return (uint32_t)getuid(); }
static inline uint32_t zov_geteuid(void) { return (uint32_t)geteuid(); }
static inline uint32_t zov_getgid(void) { return (uint32_t)getgid(); }
static inline uint32_t zov_getegid(void) { return (uint32_t)getegid(); }

static inline char *zov_user_name(uint32_t uid) {
    struct passwd *p = getpwuid((uid_t)uid);
    return p ? p->pw_name : NULL;
}

static inline char *zov_group_name(uint32_t gid) {
    struct group *p = getgrgid((gid_t)gid);
    return p ? p->gr_name : NULL;
}

static inline int zov_get_uname_fields(
    char *sysname, size_t sysname_sz,
    char *nodename, size_t nodename_sz,
    char *release, size_t release_sz,
    char *version, size_t version_sz,
    char *machine, size_t machine_sz
) {
    struct utsname u;
    if (uname(&u) != 0) return errno;
    snprintf(sysname, sysname_sz, "%s", u.sysname);
    snprintf(nodename, nodename_sz, "%s", u.nodename);
    snprintf(release, release_sz, "%s", u.release);
    snprintf(version, version_sz, "%s", u.version);
    snprintf(machine, machine_sz, "%s", u.machine);
    return 0;
}

static inline int zov_get_user_primary_gid(const char *username, uint32_t *gid_out, char *canonical_name, size_t name_sz) {
    struct passwd *pw = username ? getpwnam(username) : getpwuid(getuid());
    if (!pw) return 0;
    if (gid_out) *gid_out = (uint32_t)pw->pw_gid;
    if (canonical_name) snprintf(canonical_name, name_sz, "%s", pw->pw_name);
    return 1;
}

static inline int zov_get_user_groups(const char *username, uint32_t primary_gid, uint32_t *groups_out, int max_groups) {
    int n = max_groups;
    gid_t *list = (gid_t *)malloc((size_t)max_groups * sizeof(gid_t));
    if (!list) return -1;
    if (getgrouplist(username, (gid_t)primary_gid, list, &n) < 0) {
        free(list);
        return -n;
    }
    for (int i = 0; i < n; i++) groups_out[i] = (uint32_t)list[i];
    free(list);
    return n;
}

static inline int zov_get_current_groups(uint32_t *groups_out, int max_groups) {
    int n = getgroups(0, NULL);
    if (n < 0) return -1;
    if (max_groups == 0) return n;
    gid_t *list = (gid_t *)malloc((size_t)(n > 0 ? n : 1) * sizeof(gid_t));
    if (!list) return -1;
    if (n > 0 && getgroups(n, list) < 0) {
        free(list);
        return -1;
    }
#if defined(__APPLE__)
    if (n > 16) n = 16;
#endif
    int count = n < max_groups ? n : max_groups;
    for (int i = 0; i < count; i++) groups_out[i] = (uint32_t)list[i];
    free(list);
    return count;
}
#endif

static inline int zov_get_security_context(char *buf, size_t buf_sz) {
    FILE *f = fopen("/proc/self/attr/current", "r");
    if (!f) return errno;
    if (!fgets(buf, buf_sz, f)) {
        int err = errno;
        fclose(f);
        return err ? err : EIO;
    }
    fclose(f);
    buf[strcspn(buf, "\r\n")] = '\0';
    return 0;
}

static inline int zov_selinux_active(void) {
    return access("/sys/fs/selinux", F_OK) == 0;
}

static inline int zov_chcon_set_context(const char *context, const char *path, int follow) {
#if defined(__linux__)
    int rc = follow ? setxattr(path, "security.selinux", context, strlen(context) + 1, 0)
                    : lsetxattr(path, "security.selinux", context, strlen(context) + 1, 0);
    return rc == 0 ? 0 : errno;
#else
    (void)context; (void)path; (void)follow;
    return ENOTSUP;
#endif
}

static inline int zov_runcon_set_exec_context(const char *context) {
#if defined(__linux__)
    if (access("/sys/fs/selinux", F_OK) < 0) {
        return -1;
    }
    FILE *f = fopen("/proc/self/attr/exec", "w");
    if (!f) return errno;
    if (fprintf(f, "%s", context) < 0 || fclose(f) < 0) {
        return -2;
    }
    return 0;
#else
    (void)context;
    return -3;
#endif
}

#endif

