#ifndef ZOVUTILS_PLATFORM_PATH_H
#define ZOVUTILS_PLATFORM_PATH_H

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Convert a path to an absolute, lexically normalized path without resolving
   symbolic links.  The caller supplies a PATH_MAX-sized output buffer. */
static inline int zov_path_normalize(const char *path, char *output, size_t cap) {
    char absolute[PATH_MAX * 2];
    if (path[0] == '/') {
        if (strlen(path) >= sizeof(absolute)) { errno = ENAMETOOLONG; return 0; }
        strcpy(absolute, path);
    } else {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) return 0;
        size_t a = strlen(cwd), b = strlen(path);
        if (a + b + 2 > sizeof(absolute)) { errno = ENAMETOOLONG; return 0; }
        memcpy(absolute, cwd, a);
        absolute[a++] = '/';
        memcpy(absolute + a, path, b + 1);
    }
    size_t offsets[PATH_MAX / 2];
    size_t depth = 0, used = 1;
    if (cap < 2) { errno = ENAMETOOLONG; return 0; }
    output[0] = '/'; output[1] = '\0';
    const char *p = absolute;
    while (*p != '\0') {
        while (*p == '/') ++p;
        if (*p == '\0') break;
        const char *start = p;
        while (*p != '\0' && *p != '/') ++p;
        size_t len = (size_t)(p - start);
        if (len == 1 && start[0] == '.') continue;
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (depth > 0) { used = offsets[--depth]; output[used] = '\0'; }
            continue;
        }
        if (depth >= sizeof(offsets) / sizeof(offsets[0]) || used + len + 2 > cap) {
            errno = ENAMETOOLONG; return 0;
        }
        offsets[depth++] = used;
        if (used > 1) output[used++] = '/';
        memcpy(output + used, start, len); used += len; output[used] = '\0';
    }
    return 1;
}

/* mode: 0 all components must exist; 1 final component may be missing;
   2 any suffix may be missing.  logical applies lexical '..' before links. */
static inline int zov_path_resolve(const char *path, int mode, int logical,
                                   int strip_links, char *output, size_t cap) {
    char absolute[PATH_MAX * 2];
    if (logical || strip_links) {
        if (!zov_path_normalize(path, absolute, sizeof(absolute))) return 0;
    } else if (path[0] == '/') {
        if (strlen(path) >= sizeof(absolute)) { errno = ENAMETOOLONG; return 0; }
        strcpy(absolute, path);
    } else {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) return 0;
        size_t a = strlen(cwd), b = strlen(path);
        if (a + b + 2 > sizeof(absolute)) { errno = ENAMETOOLONG; return 0; }
        memcpy(absolute, cwd, a); absolute[a++] = '/'; memcpy(absolute + a, path, b + 1);
    }
    if (strip_links) {
        if (strlen(absolute) + 1 > cap) { errno = ENAMETOOLONG; return 0; }
        strcpy(output, absolute); return 1;
    }
    if (realpath(absolute, output) != NULL) return 1;
    if (mode == 0) return 0;

    size_t len = strlen(absolute);
    int trailing_slash = len > 1 && absolute[len - 1] == '/';
    for (size_t cut = len; cut > 0; --cut) {
        if (absolute[cut] != '/' && absolute[cut] != '\0') continue;
        if (cut == len && trailing_slash) continue;
        char prefix[PATH_MAX * 2];
        size_t plen = cut;
        if (plen == 0) continue;
        if (plen >= sizeof(prefix)) continue;
        memcpy(prefix, absolute, plen); prefix[plen] = '\0';
        if (plen == 0 || prefix[plen - 1] == '\0') continue;
        char resolved[PATH_MAX];
        if (realpath(prefix, resolved) == NULL) continue;
        const char *rest = absolute + cut;
        while (*rest == '/') ++rest;
        if (mode == 1 && (*rest == '\0' || strchr(rest, '/') != NULL || trailing_slash)) {
            errno = ENOENT; return 0;
        }
        size_t a = strlen(resolved), b = strlen(rest);
        char combined[PATH_MAX * 2];
        if (a + b + 2 > sizeof(combined)) { errno = ENAMETOOLONG; return 0; }
        memcpy(combined, resolved, a);
        if (b > 0 && (a == 0 || resolved[a - 1] != '/')) combined[a++] = '/';
        memcpy(combined + a, rest, b + 1);
        return zov_path_normalize(combined, output, cap);
    }
    errno = ENOENT;
    return 0;
}

#endif
