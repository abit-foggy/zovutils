#ifndef ZOVUTILS_MKTEMP_BACKEND_H
#define ZOVUTILS_MKTEMP_BACKEND_H
#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static inline int zov_mktemp_long(const char *arg, const char *name) {
    size_t n = strlen(arg);
    return n > 2 && n <= strlen(name) && strncmp(arg, name, n) == 0;
}

static inline int zov_mktemp_run(int argc, int8_t **argv) {
    int directory = 0, dry = 0, quiet = 0, tmpdir = 0, tmpdir_arg = 0, single = 0;
    const char *dir = NULL, *suffix = NULL, *templ = NULL;
    const char *bad_option = "--option";
    int options = 1;
    for (int i = 0; i < argc; ++i) {
        const char *a = (const char *)argv[i];
        bad_option = a;
        if (options && strcmp(a, "--") == 0) { options = 0; continue; }
        if (options && strncmp(a, "--", 2) == 0) {
            const char *eq = strchr(a, '='); size_t n = eq ? (size_t)(eq - a) : strlen(a);
            char key[40]; if (n >= sizeof(key)) key[0] = 0; else { memcpy(key, a, n); key[n] = 0; }
#define MT_LONG(x) (key[0] && zov_mktemp_long(key, x))
            if (MT_LONG("--help")) { if (eq) goto badarg; puts("Usage: mktemp [OPTION]... [TEMPLATE]\nCreate a temporary file or directory, safely, and print its name.\nTEMPLATE must contain at least 3 consecutive 'X's in last component.\nIf TEMPLATE is not specified, use tmp.XXXXXXXXXX, and --tmpdir is implied.\nFiles are created u+rw, and directories u+rwx, minus umask restrictions.\n\n  -d, --directory\n         create a directory, not a file\n  -u, --dry-run\n         do not create anything; merely print a name (unsafe)\n  -q, --quiet\n         suppress diagnostics about file/dir-creation failure\n      --suffix=SUFF\n         append SUFF to TEMPLATE; SUFF must not contain a slash.\n         This option is implied if TEMPLATE does not end in X\n  -p DIR, --tmpdir[=DIR]\n         interpret TEMPLATE relative to DIR;\n         if DIR is not specified, use $TMPDIR if set, else /tmp.\n         With this option, TEMPLATE must not be an absolute name;\n         unlike with -t, TEMPLATE may contain slashes,\n         but mktemp creates only the final component\n  -t\n         interpret TEMPLATE as a single file name component,\n         relative to a directory: $TMPDIR, if set;\n         else the directory specified via -p; else /tmp [deprecated]\n      --help\n         display this help and exit\n      --version\n         output version information and exit\n\nReport bugs to: bug-coreutils@gnu.org\nGNU coreutils home page: <https://www.gnu.org/software/coreutils/>\nGeneral help using GNU software: <https://www.gnu.org/gethelp/>\nReport any translation bugs to <https://translationproject.org/team/>\nFull documentation <https://www.gnu.org/software/coreutils/mktemp>\nor available locally via: info '(coreutils) mktemp invocation'"); return 0; }
            if (MT_LONG("--version")) { if (eq) goto badarg; puts("mktemp (zovutils) 0.10.28"); return 0; }
            if (MT_LONG("--directory")) { if (eq) goto badarg; directory = 1; continue; }
            if (MT_LONG("--dry-run")) { if (eq) goto badarg; dry = 1; continue; }
            if (MT_LONG("--quiet")) { if (eq) goto badarg; quiet = 1; continue; }
            if (MT_LONG("--tmpdir")) { tmpdir = 1; if (eq) { dir = eq + 1; tmpdir_arg = 1; } continue; }
            if (MT_LONG("--suffix")) { if (!eq) { if (++i >= argc) { fputs("mktemp: option '--suffix' requires an argument\nTry 'mktemp --help' for more information.\n", stderr); return 1; } suffix = (const char *)argv[i]; } else suffix = eq + 1; continue; }
            fprintf(stderr, "mktemp: unrecognized option '%s'\nTry 'mktemp --help' for more information.\n", a); return 1;
#undef MT_LONG
        }
        if (options && a[0] == '-' && a[1]) {
            for (size_t j = 1; a[j]; ++j) {
                if (a[j] == 'd') directory = 1;
                else if (a[j] == 'u') dry = 1;
                else if (a[j] == 'q') quiet = 1;
                else if (a[j] == 't') single = 1;
                else if (a[j] == 'p') { tmpdir = 1; if (a[j + 1]) { dir = a + j + 1; tmpdir_arg = 1; j = strlen(a) - 1; } else if (i + 1 < argc && ((const char *)argv[i + 1])[0] != '-') { dir = (const char *)argv[++i]; tmpdir_arg = 1; } }
                else { fprintf(stderr, "mktemp: invalid option -- '%c'\nTry 'mktemp --help' for more information.\n", a[j]); return 1; }
            }
            continue;
        }
        if (templ) { fprintf(stderr, "mktemp: too many templates\nTry 'mktemp --help' for more information.\n"); return 1; }
        templ = a;
    }
    if (tmpdir && !tmpdir_arg) { dir = getenv("TMPDIR"); if (!dir || !*dir) dir = "/tmp"; }
    if (single && !dir) { dir = getenv("TMPDIR"); if (!dir || !*dir) dir = "/tmp"; }
    if (!templ) templ = tmpdir || single ? "tmp.XXXXXXXXXX" : "/tmp/tmp.XXXXXXXXXX";
    if (suffix && strchr(suffix, '/')) { fprintf(stderr, "mktemp: invalid suffix '%s', contains directory separator\n", suffix); return 1; }
    if (tmpdir || single) {
        if (templ[0] == '/') { fputs("mktemp: template must not contain directory separators\nTry 'mktemp --help' for more information.\n", stderr); return 1; }
        if (single && strchr(templ, '/')) { fputs("mktemp: template must not contain directory separators\nTry 'mktemp --help' for more information.\n", stderr); return 1; }
    }
    const char *base = templ;
    char path[PATH_MAX];
    if ((tmpdir || single) && dir) { if (snprintf(path, sizeof(path), "%s/%s", dir, templ) >= (int)sizeof(path)) { errno = ENAMETOOLONG; goto fail; } base = path; }
    size_t n = strlen(base), sn = suffix ? strlen(suffix) : 0;
    if (n + sn + 1 > sizeof(path)) { errno = ENAMETOOLONG; goto fail; }
    char work[PATH_MAX]; memcpy(work, base, n + 1);
    char *leaf = strrchr(work, '/'); leaf = leaf ? leaf + 1 : work;
    size_t leaflen = strlen(leaf), xs = 0;
    while (xs < leaflen && leaf[leaflen - xs - 1] == 'X') ++xs;
    if (suffix && xs == 0) { strcat(work, suffix); leaflen += sn; xs = 0; }
    else if (suffix) { leaf[leaflen - xs] = 0; strcat(work, suffix); strcat(work, "XXXXXXXXXX"); xs = 10; }
    if (xs < 3) { fprintf(stderr, "mktemp: too few X's in template '%s'\n", templ); return 1; }
    if (dry) { for (size_t i = 0; work[i]; ++i) if (work[i] == 'X') work[i] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[(unsigned)rand() % 62]; puts(work); return 0; }
    if (directory) { if (!mkdtemp(work)) goto fail; }
    else {
        size_t xleft = xs > 6 ? xs - 6 : 0;
        for (size_t i = 0; work[i] && xleft; ++i) if (work[i] == 'X') { work[i] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[(unsigned)rand() % 62]; --xleft; }
        int fd = mkstemp(work); if (fd < 0) goto fail; close(fd);
    }
    puts(work); return ferror(stdout) ? 1 : 0;
badarg:
    fprintf(stderr, "mktemp: option '%s' doesn't allow an argument\nTry 'mktemp --help' for more information.\n", bad_option); return 1;
fail:
    if (!quiet) fprintf(stderr, "mktemp: failed to create %s '%s': %s\n", directory ? "directory" : "file", templ, strerror(errno));
    return 1;
}
#endif
