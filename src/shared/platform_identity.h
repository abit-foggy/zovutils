#ifndef ZOVUTILS_PLATFORM_IDENTITY_H
#define ZOVUTILS_PLATFORM_IDENTITY_H

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/arch.h>
#include <sys/sysctl.h>

static inline const char *zov_uname_processor(void) {
    cpu_type_t cputype;
    size_t size = sizeof(cputype);
    if (sysctlbyname("hw.cputype", &cputype, &size, NULL, 0) == 0) {
        const NXArchInfo *ai = NXGetArchInfoFromCpuType(cputype, CPU_SUBTYPE_MULTIPLE);
        if (ai && ai->name) {
            if (strncmp(ai->name, "arm", 3) == 0) return "arm";
            if (strncmp(ai->name, "x86", 3) == 0 || strncmp(ai->name, "i386", 4) == 0) return "i386";
            return ai->name;
        }
    }
#if defined(__aarch64__) || defined(__arm64__) || defined(__arm__)
    return "arm";
#elif defined(__x86_64__) || defined(__i386__)
    return "i386";
#else
    return "unknown";
#endif
}
#define ZOV_UNAME_OS "Darwin"
#else
static inline const char *zov_uname_processor(void) { return "unknown"; }
#define ZOV_UNAME_OS "GNU/Linux"
#endif

static inline const char *zov_uname_os(void) {
    return ZOV_UNAME_OS;
}

static inline uint32_t zov_getuid(void) { return (uint32_t)getuid(); }
static inline uint32_t zov_geteuid(void) { return (uint32_t)geteuid(); }
static inline uint32_t zov_getgid(void) { return (uint32_t)getgid(); }
static inline uint32_t zov_getegid(void) { return (uint32_t)getegid(); }

static inline const char *zov_user_name(uint32_t uid) {
    struct passwd *p = getpwuid((uid_t)uid);
    return p ? p->pw_name : NULL;
}

static inline const char *zov_group_name(uint32_t gid) {
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
