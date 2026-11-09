#ifndef ZOVUTILS_PLATFORM_PATH_H
#define ZOVUTILS_PLATFORM_PATH_H

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#define zov_getcwd _getcwd
#define zov_realpath(path, out) _fullpath(out, path, (out) ? PATH_MAX : 0)
#define zov_strtok_r strtok_s
#define IS_PATH_SEP(c) ((c) == '/' || (c) == '\\')
#else
#include <unistd.h>
#define zov_getcwd getcwd
#define zov_realpath realpath
#define zov_strtok_r strtok_r
#define IS_PATH_SEP(c) ((c) == '/')
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static inline int zov_is_absolute_path(const char *path) {
    if (!path || !path[0]) return 0;
    if (path[0] == '/') return 1;
#if defined(_WIN32)
    if (path[0] == '\\') return 1;
    if (((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z')) &&
        path[1] == ':' && (path[2] == '/' || path[2] == '\\')) {
        return 1;
    }
#endif
    return 0;
}

/* Convert a path to an absolute, lexically normalized path without resolving
   symbolic links.  The caller supplies a PATH_MAX-sized output buffer. */
static inline int zov_path_normalize(const char *path, char *output, size_t cap) {
    char absolute[PATH_MAX * 2];
    if (zov_is_absolute_path(path)) {
        if (strlen(path) >= sizeof(absolute)) { errno = ENAMETOOLONG; return 0; }
        strcpy(absolute, path);
    } else {
        char cwd[PATH_MAX];
        if (zov_getcwd(cwd, sizeof(cwd)) == NULL) return 0;
        size_t a = strlen(cwd), b = strlen(path);
        if (a + b + 2 > sizeof(absolute)) { errno = ENAMETOOLONG; return 0; }
        memcpy(absolute, cwd, a);
        absolute[a++] = '/';
        memcpy(absolute + a, path, b + 1);
    }

    size_t offsets[PATH_MAX / 2];
    size_t depth = 0, used = 0;
    const char *p = absolute;

#if defined(_WIN32)
    if (((p[0] >= 'a' && p[0] <= 'z') || (p[0] >= 'A' && p[0] <= 'Z')) && p[1] == ':' && IS_PATH_SEP(p[2])) {
        if (cap < 4) { errno = ENAMETOOLONG; return 0; }
        output[0] = p[0];
        output[1] = ':';
        output[2] = '/';
        output[3] = '\0';
        used = 3;
        p += 3;
    } else
#endif
    {
        if (cap < 2) { errno = ENAMETOOLONG; return 0; }
        output[0] = '/';
        output[1] = '\0';
        used = 1;
        while (IS_PATH_SEP(*p)) ++p;
    }

    while (*p != '\0') {
        while (IS_PATH_SEP(*p)) ++p;
        if (*p == '\0') break;
        const char *start = p;
        while (*p != '\0' && !IS_PATH_SEP(*p)) ++p;
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
        if (used > 0 && output[used - 1] != '/') output[used++] = '/';
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
    } else if (zov_is_absolute_path(path)) {
        if (strlen(path) >= sizeof(absolute)) { errno = ENAMETOOLONG; return 0; }
        strcpy(absolute, path);
    } else {
        char cwd[PATH_MAX];
        if (zov_getcwd(cwd, sizeof(cwd)) == NULL) return 0;
        size_t a = strlen(cwd), b = strlen(path);
        if (a + b + 2 > sizeof(absolute)) { errno = ENAMETOOLONG; return 0; }
        memcpy(absolute, cwd, a); absolute[a++] = '/'; memcpy(absolute + a, path, b + 1);
    }
    if (strip_links) {
        if (strlen(absolute) + 1 > cap) { errno = ENAMETOOLONG; return 0; }
        strcpy(output, absolute); return 1;
    }
    if (zov_realpath(absolute, output) != NULL) return 1;
    if (mode == 0) return 0;

    size_t len = strlen(absolute);
    int trailing_slash = len > 1 && IS_PATH_SEP(absolute[len - 1]);
    for (size_t cut = len; cut > 0; --cut) {
        if (!IS_PATH_SEP(absolute[cut]) && absolute[cut] != '\0') continue;
        if (cut == len && trailing_slash) continue;
        char prefix[PATH_MAX * 2];
        size_t plen = cut;
        if (plen == 0) continue;
        if (plen >= sizeof(prefix)) continue;
        memcpy(prefix, absolute, plen); prefix[plen] = '\0';
        if (plen == 0 || prefix[plen - 1] == '\0') continue;
        char resolved[PATH_MAX];
        if (zov_realpath(prefix, resolved) == NULL) continue;
        const char *rest = absolute + cut;
        while (IS_PATH_SEP(*rest)) ++rest;
        if (mode == 1 && (*rest == '\0' || strchr(rest, '/') != NULL || strchr(rest, '\\') != NULL || trailing_slash)) {
            errno = ENOENT; return 0;
        }
        size_t a = strlen(resolved), b = strlen(rest);
        char combined[PATH_MAX * 2];
        if (a + b + 2 > sizeof(combined)) { errno = ENAMETOOLONG; return 0; }
        memcpy(combined, resolved, a);
        if (b > 0 && (a == 0 || !IS_PATH_SEP(resolved[a - 1]))) combined[a++] = '/';
        memcpy(combined + a, rest, b + 1);
        return zov_path_normalize(combined, output, cap);
    }
    errno = ENOENT;
    return 0;
}

static inline long zov_path_name_max(const char *path) {
#if defined(_WIN32)
    (void)path;
    return 255;
#else
    errno = 0;
    return pathconf(path, _PC_NAME_MAX);
#endif
}

static inline int zov_path_relative_target(const char *source, const char *destination, char *output, size_t cap) {
    char source_path[PATH_MAX];
    if (zov_realpath(source, source_path) == NULL) {
        if (!zov_path_normalize(source, source_path, sizeof(source_path))) return 0;
    }
    char parent[PATH_MAX];
    if (strlen(destination) >= sizeof(parent)) { errno = ENAMETOOLONG; return 0; }
    strcpy(parent, destination);
    char *slash = strrchr(parent, '/');
    char *bslash = strrchr(parent, '\\');
    if (bslash > slash) slash = bslash;
    if (slash == NULL) strcpy(parent, ".");
    else if (slash == parent) slash[1] = '\0';
    else *slash = '\0';

    char dest_path[PATH_MAX];
    if (zov_realpath(parent, dest_path) == NULL) {
        if (!zov_path_normalize(parent, dest_path, sizeof(dest_path))) return 0;
    }

    char source_copy[PATH_MAX], dest_copy[PATH_MAX];
    strcpy(source_copy, source_path);
    strcpy(dest_copy, dest_path);

    char *source_parts[PATH_MAX / 2];
    char *dest_parts[PATH_MAX / 2];
    size_t source_count = 0, dest_count = 0;
    char *save_source = NULL, *save_dest = NULL;

    for (char *part = zov_strtok_r(source_copy, "/\\", &save_source); part != NULL; part = zov_strtok_r(NULL, "/\\", &save_source)) {
        if (source_count < sizeof(source_parts) / sizeof(source_parts[0]))
            source_parts[source_count++] = part;
    }
    for (char *part = zov_strtok_r(dest_copy, "/\\", &save_dest); part != NULL; part = zov_strtok_r(NULL, "/\\", &save_dest)) {
        if (dest_count < sizeof(dest_parts) / sizeof(dest_parts[0]))
            dest_parts[dest_count++] = part;
    }

    size_t common = 0;
    while (common < source_count && common < dest_count && strcmp(source_parts[common], dest_parts[common]) == 0) {
        ++common;
    }

    size_t out = 0;
    for (size_t i = common; i < dest_count; ++i) {
        if (out != 0) {
            if (out + 1 >= cap) { errno = ENAMETOOLONG; return 0; }
            output[out++] = '/';
        }
        if (out + 2 >= cap) { errno = ENAMETOOLONG; return 0; }
        output[out++] = '.'; output[out++] = '.';
    }
    for (size_t i = common; i < source_count; ++i) {
        if (out != 0) {
            if (out + 1 >= cap) { errno = ENAMETOOLONG; return 0; }
            output[out++] = '/';
        }
        size_t part_len = strlen(source_parts[i]);
        if (out + part_len >= cap) { errno = ENAMETOOLONG; return 0; }
        memcpy(output + out, source_parts[i], part_len);
        out += part_len;
    }
    if (out == 0) {
        if (cap < 2) { errno = ENAMETOOLONG; return 0; }
        output[out++] = '.';
    }
    output[out] = '\0';
    return 1;
}

#endif
