#ifndef ZOVUTILS_REALPATH_BACKEND_H
#define ZOVUTILS_REALPATH_BACKEND_H
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform_path.h"

static inline int zov_realpath_long(const char *a, const char *name) {
    size_t n = strlen(a); return n > 2 && n <= strlen(name) && strncmp(a, name, n) == 0;
}

static inline void zov_realpath_error(const char *path, int quiet) {
    if (!quiet) fprintf(stderr, "realpath: %s: %s\n", path, strerror(errno));
}

static inline int zov_realpath_run(int argc, int8_t **argv) {
    int mode = 1, logical = 0, strip = 0, quiet = 0, zero = 0, options = 1;
    const char *relative_to = NULL, *relative_base = NULL;
    const char *bad_option = "--option";
    const char *paths[4096]; int npaths = 0;
    for (int i = 0; i < argc; ++i) {
        const char *a = (const char *)argv[i];
        bad_option = a;
        if (options && !strcmp(a, "--")) { options = 0; continue; }
        if (options && !strncmp(a, "--", 2)) {
            const char *eq = strchr(a, '='); size_t n = eq ? (size_t)(eq - a) : strlen(a); char key[64];
            if (n >= sizeof(key)) key[0] = 0; else { memcpy(key, a, n); key[n] = 0; }
#define RP_LONG(x) (key[0] && zov_realpath_long(key, x))
            if (RP_LONG("--help")) { if (eq) goto badarg; puts("Usage: realpath [OPTION]... FILE...\nPrint the resolved absolute file name.\n  -E, --canonicalize             all but the last component must exist (default)\n  -e, --canonicalize-existing    all components of the path must exist\n  -m, --canonicalize-missing     no path components need exist or be a directory\n  -L, --logical                  resolve '..' components before symlinks\n  -P, --physical                 resolve symlinks as encountered (default)\n  -q, --quiet                    suppress most error messages\n      --relative-to=DIR          print the resolved path relative to DIR\n      --relative-base=DIR        print absolute paths unless below DIR\n  -s, --strip, --no-symlinks     don't expand symlinks\n  -z, --zero                     end each output line with NUL\n      --help                     display this help and exit\n      --version                  output version information and exit\n\nReport bugs to: bug-coreutils@gnu.org\nGNU coreutils home page: <https://www.gnu.org/software/coreutils/>\nGeneral help using GNU software: <https://www.gnu.org/gethelp/>\nReport any translation bugs to <https://translationproject.org/team/>\nFull documentation <https://www.gnu.org/software/coreutils/realpath>\nor available locally via: info '(coreutils) realpath invocation'"); return 0; }
            if (RP_LONG("--version")) { if (eq) goto badarg; puts("realpath (zovutils) 0.10.15"); return 0; }
            if (RP_LONG("--canonicalize")) { mode = 1; continue; }
            if (RP_LONG("--canonicalize-existing")) { mode = 0; continue; }
            if (RP_LONG("--canonicalize-missing")) { mode = 2; continue; }
            if (RP_LONG("--logical")) { logical = 1; continue; }
            if (RP_LONG("--physical")) { logical = 0; continue; }
            if (RP_LONG("--quiet")) { quiet = 1; continue; }
            if (RP_LONG("--strip") || RP_LONG("--no-symlinks")) { strip = 1; mode = 2; continue; }
            if (RP_LONG("--zero")) { zero = 1; continue; }
            if (RP_LONG("--relative-to") || RP_LONG("--relative-base")) {
                const char *v = eq ? eq + 1 : (i + 1 < argc ? (const char *)argv[++i] : NULL);
                if (!v) { fprintf(stderr, "realpath: option '%s' requires an argument\nTry 'realpath --help' for more information.\n", key); return 1; }
                if (RP_LONG("--relative-to")) relative_to = v; else relative_base = v; continue;
            }
            fprintf(stderr, "realpath: unrecognized option '%s'\nTry 'realpath --help' for more information.\n", a); return 1;
#undef RP_LONG
        }
        if (options && a[0] == '-' && a[1]) {
            for (size_t j = 1; a[j]; ++j) {
                if (a[j] == 'E') mode = 1; else if (a[j] == 'e') mode = 0; else if (a[j] == 'm') mode = 2;
                else if (a[j] == 'L') logical = 1; else if (a[j] == 'P') logical = 0;
                else if (a[j] == 'q') quiet = 1; else if (a[j] == 's') { strip = 1; mode = 2; }
                else if (a[j] == 'z') zero = 1;
                else { fprintf(stderr, "realpath: invalid option -- '%c'\nTry 'realpath --help' for more information.\n", a[j]); return 1; }
            }
            continue;
        }
        if (npaths >= 4096) { fputs("realpath: too many operands\n", stderr); return 1; }
        paths[npaths++] = a;
    }
    if (!npaths) { fputs("realpath: missing operand\nTry 'realpath --help' for more information.\n", stderr); return 1; }
    char base[PATH_MAX], relbase[PATH_MAX];
    if (relative_to && !zov_path_resolve(relative_to, 0, logical, strip, base, sizeof(base))) { zov_realpath_error(relative_to, quiet); return 1; }
    if (relative_base && !zov_path_resolve(relative_base, 0, logical, strip, relbase, sizeof(relbase))) { zov_realpath_error(relative_base, quiet); return 1; }
    int status = 0;
    for (int i = 0; i < npaths; ++i) {
        char result[PATH_MAX];
        if (!zov_path_resolve(paths[i], mode, logical, strip, result, sizeof(result))) { zov_realpath_error(paths[i], quiet); status = 1; continue; }
        const char *out = result; char relative[PATH_MAX];
        const char *against = relative_to ? base : relative_base ? relbase : NULL;
        if (against && strncmp(result, against, strlen(against)) == 0 && (result[strlen(against)] == '/' || result[strlen(against)] == 0)) {
            const char *tail = result + strlen(against); while (*tail == '/') ++tail;
            if (relative_to) out = tail[0] ? tail : ".";
            else out = tail;
        }
        fputs(out, stdout); fputc(zero ? '\0' : '\n', stdout);
        (void)relative;
    }
    return fflush(stdout) == 0 ? status : 1;
badarg:
    fprintf(stderr, "realpath: option '%s' doesn't allow an argument\nTry 'realpath --help' for more information.\n", bad_option); return 1;
}
#endif
