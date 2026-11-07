#ifndef ZOVUTILS_STDBUF_BACKEND_H
#define ZOVUTILS_STDBUF_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

static inline int zov_stdbuf_size(const char *mode) {
    if (!mode || !*mode) return 0;
    if (!strcmp(mode, "0") || !strcmp(mode, "L")) return 1;
    if (*mode == '-') return 0;
    errno = 0;
    char *end = NULL;
    unsigned long long n = strtoull(mode, &end, 10);
    if (errno || end == mode) return 0;
    unsigned long long multiplier = 1;
    if (!*end) multiplier = 1;
    else if (!strcmp(end, "k") || !strcmp(end, "K") || !strcmp(end, "KiB")) multiplier = 1024ULL;
    else if (!strcmp(end, "KB")) multiplier = 1000ULL;
    else if (!strcmp(end, "M") || !strcmp(end, "MiB")) multiplier = 1024ULL * 1024ULL;
    else if (!strcmp(end, "MB")) multiplier = 1000ULL * 1000ULL;
    else if (!strcmp(end, "G") || !strcmp(end, "GiB")) multiplier = 1024ULL * 1024ULL * 1024ULL;
    else if (!strcmp(end, "GB")) multiplier = 1000ULL * 1000ULL * 1000ULL;
    else if (!strcmp(end, "T") || !strcmp(end, "TiB")) multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
    else if (!strcmp(end, "TB")) multiplier = 1000ULL * 1000ULL * 1000ULL * 1000ULL;
    else return 0;
    return n <= (unsigned long long)SIZE_MAX / multiplier;
}

static inline int zov_stdbuf_run(int argc, char **argv) {
    const char *mode[3] = {NULL, NULL, NULL};
    int i = 0;
    for (; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--help")) {
            puts("Usage: stdbuf OPTION... COMMAND\nRun COMMAND, with modified buffering operations for its standard streams.\n  -i, --input=MODE   adjust standard input stream buffering\n  -o, --output=MODE  adjust standard output stream buffering\n  -e, --error=MODE   adjust standard error stream buffering\n      --help         display this help and exit\n      --version      output version information and exit\nIf MODE is 'L' the stream is line buffered, '0' unbuffered, or a number optionally followed by a size suffix.");
            return 0;
        }
        if (!strcmp(a, "--version")) { puts("stdbuf (zovutils) 0.10.28"); return 0; }
        if (!strcmp(a, "--")) { i++; break; }
        if (a[0] != '-' || !a[1]) break;
        int which = -1;
        const char *value = NULL;
        if (!strcmp(a, "-i") || !strcmp(a, "-o") || !strcmp(a, "-e")) {
            which = a[1] == 'i' ? 0 : a[1] == 'o' ? 1 : 2;
            if (i + 1 >= argc) {
                fprintf(stderr, "stdbuf: option requires an argument -- '%c'\nTry 'stdbuf --help' for more information.\n", a[1]);
                return 125;
            }
            value = argv[++i];
        } else if (a[0] == '-' && (a[1] == 'i' || a[1] == 'o' || a[1] == 'e') && a[2]) {
            which = a[1] == 'i' ? 0 : a[1] == 'o' ? 1 : 2;
            value = a + 2;
        } else if (!strncmp(a, "--input=", 8)) { which = 0; value = a + 8; }
        else if (!strncmp(a, "--output=", 9)) { which = 1; value = a + 9; }
        else if (!strncmp(a, "--error=", 8)) { which = 2; value = a + 8; }
        else if (!strcmp(a, "--input") || !strcmp(a, "--output") || !strcmp(a, "--error")) {
            which = !strcmp(a, "--input") ? 0 : !strcmp(a, "--output") ? 1 : 2;
            if (i + 1 >= argc) {
                fprintf(stderr, "stdbuf: option '%s' requires an argument\nTry 'stdbuf --help' for more information.\n", a);
                return 125;
            }
            value = argv[++i];
        } else {
            if (!strncmp(a, "--help=", 7) || !strncmp(a, "--version=", 10)) {
                const char *opt = a[2] == 'h' ? "--help" : "--version";
                fprintf(stderr, "stdbuf: option '%s' doesn't allow an argument\nTry 'stdbuf --help' for more information.\n", opt);
            } else if (a[1] == '-') {
                fprintf(stderr, "stdbuf: unrecognized option '%s'\nTry 'stdbuf --help' for more information.\n", a);
            } else {
                fprintf(stderr, "stdbuf: invalid option -- '%c'\nTry 'stdbuf --help' for more information.\n", a[1]);
            }
            return 125;
        }
        if (!zov_stdbuf_size(value)) {
#if defined(__APPLE__)
            if (value[0] == '-') fprintf(stderr, "stdbuf: invalid mode '%s'\n", value);
            else fprintf(stderr, "stdbuf: invalid mode '%s': %s\n", value, strerror(EINVAL));
#else
            fprintf(stderr, "stdbuf: invalid mode '%s'\n", value);
#endif
            return 125;
        }
        if (which == 0 && !strcmp(value, "L")) {
            fprintf(stderr, "stdbuf: line buffering standard input is meaningless\nTry 'stdbuf --help' for more information.\n");
            return 125;
        }
        mode[which] = value;
    }
    if (i >= argc) { fprintf(stderr, "stdbuf: missing operand\nTry 'stdbuf --help' for more information.\n"); return 125; }
    for (int m = 0; m < 3; m++) if (mode[m]) {
        const char *key = m == 0 ? "_STDBUF_I" : m == 1 ? "_STDBUF_O" : "_STDBUF_E";
        if (setenv(key, mode[m], 1) != 0) { fprintf(stderr, "stdbuf: %s\n", strerror(errno)); return 125; }
    }
    char exe[PATH_MAX];
#if defined(__APPLE__)
    uint32_t size = (uint32_t)sizeof exe;
    char unresolved[PATH_MAX];
    if (_NSGetExecutablePath(unresolved, &size) != 0 || realpath(unresolved, exe) == NULL) { fprintf(stderr, "stdbuf: cannot locate preload library\n"); return 125; }
#else
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n < 0) { fprintf(stderr, "stdbuf: cannot locate preload library: %s\n", strerror(errno)); return 125; }
    exe[n] = 0;
#endif
    char *slash = strrchr(exe, '/');
    if (!slash) { fprintf(stderr, "stdbuf: cannot locate preload library\n"); return 125; }
    strcpy(slash + 1, "libzovstdbuf.dylib");
#if defined(__APPLE__)
    if (setenv("DYLD_INSERT_LIBRARIES", exe, 1) != 0) return 125;
    setenv("DYLD_FORCE_FLAT_NAMESPACE", "1", 1);
#else
    const char *old = getenv("LD_PRELOAD");
    char preload[PATH_MAX * 2];
    if (old && *old) snprintf(preload, sizeof preload, "%s:%s", exe, old);
    else snprintf(preload, sizeof preload, "%s", exe);
    if (setenv("LD_PRELOAD", preload, 1) != 0) return 125;
#endif
    execvp(argv[i], argv + i);
    fprintf(stderr, "stdbuf: failed to run command '%s': %s\n", argv[i], strerror(errno));
    return errno == ENOENT ? 127 : 126;
}
#endif
