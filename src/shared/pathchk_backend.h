#ifndef ZOVUTILS_PATHCHK_BACKEND_H
#define ZOVUTILS_PATHCHK_BACKEND_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

static inline int zov_pathchk_run(int argc, int8_t **argv) {
    int portable = 0, strict = 0, options = 1, operands = 0, status = 0;
    for (int i = 0; i < argc; ++i) {
        const char *a = (const char *)argv[i];
        if (options && !strcmp(a, "--")) { options = 0; continue; }
        if (options && !strcmp(a, "--help")) { puts("Usage: pathchk [OPTION]... NAME...\nDiagnose invalid or non-portable file names.\n\n  -p     check for most POSIX systems\n  -P     check for empty names and leading \"-\"\n      --portability  check for all POSIX systems (equivalent to -p -P)\n      --help         display this help and exit\n      --version      output version information and exit\n\nReport bugs to: bug-coreutils@gnu.org\nGNU coreutils home page: <https://www.gnu.org/software/coreutils/>\nGeneral help using GNU software: <https://www.gnu.org/gethelp/>\nReport any translation bugs to <https://translationproject.org/team/>\nFull documentation <https://www.gnu.org/software/coreutils/pathchk>\nor available locally via: info '(coreutils) pathchk invocation'"); return 0; }
        if (options && !strcmp(a, "--version")) { puts("pathchk (zovutils) 0.10.33"); return 0; }
        if (options && (!strcmp(a, "--portability"))) { portable = strict = 1; continue; }
        if (options && a[0] == '-' && a[1] && a[1] != '-') {
            for (size_t j = 1; a[j]; ++j) { if (a[j] == 'p') portable = 1; else if (a[j] == 'P') strict = 1; else { fprintf(stderr, "pathchk: invalid option -- '%c'\nTry 'pathchk --help' for more information.\n", a[j]); return 1; } }
            continue;
        }
        if (options && a[0] == '-' && a[1] == '-') { fprintf(stderr, "pathchk: unrecognized option '%s'\nTry 'pathchk --help' for more information.\n", a); return 1; }
        ++operands;
        size_t len = strlen(a);
        if (!len) { fputs(strict ? "pathchk: empty file name\n" : "pathchk: '': No such file or directory\n", stderr); status = 1; continue; }
        if (strict && a[0] == '-') { fprintf(stderr, "pathchk: leading '-' in file name '%s'\n", a); status = 1; }
        if (portable) {
            size_t component = 0;
            for (size_t j = 0; j <= len; ++j) {
                unsigned char c = (unsigned char)a[j];
                if (c == '/' || c == 0) {
                    if (component > 14) { fprintf(stderr, "pathchk: limit 14 exceeded by length %zu of file name component '%.*s'\n", component, (int)component, a + j - component); status = 1; }
                    component = 0;
                } else {
                    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) { if (c == ' ') fprintf(stderr, "pathchk: non-portable character ' ' in file name '%s'\n", a); else fprintf(stderr, "pathchk: non-portable character %03o in file name '%s'\n", c, a); status = 1; break; }
                    ++component;
                }
            }
            if (len > 255) { fprintf(stderr, "pathchk: limit 255 exceeded by length %zu of file name '%s'\n", len, a); status = 1; }
        } else {
            char *copy = strdup(a); if (!copy) { status = 1; continue; }
            char *part = copy; char *slash;
            while ((slash = strchr(part, '/')) != NULL) { *slash = 0; if (*part) { errno = 0; long lim = pathconf(".", _PC_NAME_MAX); if (lim > 0 && strlen(part) > (size_t)lim) { fprintf(stderr, "pathchk: component is too long: '%s'\n", part); status = 1; } } part = slash + 1; }
            if (*part) { errno = 0; long lim = pathconf(".", _PC_NAME_MAX); if (lim > 0 && strlen(part) > (size_t)lim) { fprintf(stderr, "pathchk: component is too long: '%s'\n", part); status = 1; } }
            free(copy);
        }
    }
    if (!operands) { fputs("pathchk: missing operand\nTry 'pathchk --help' for more information.\n", stderr); return 1; }
    return status;
}
#endif
