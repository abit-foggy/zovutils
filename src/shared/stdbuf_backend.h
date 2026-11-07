#ifndef ZOVUTILS_STDBUF_BACKEND_H
#define ZOVUTILS_STDBUF_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
extern char **environ;
static inline int zov_stdbuf_run(int argc, char **argv) {
    const char *mode[3] = {"-", "-", "-"};
    int i = 0;
    for (; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--help")) { puts("Usage: stdbuf OPTION... COMMAND\nRun COMMAND, with modified buffering operations for its standard streams.\n  -i, --input=MODE   adjust standard input stream buffering\n  -o, --output=MODE  adjust standard output stream buffering\n  -e, --error=MODE   adjust standard error stream buffering\n      --help         display this help and exit\n      --version      output version information and exit\nIf MODE is 'L' the stream is line buffered, '0' unbuffered, or a number optionally followed by K or M."); return 0; }
        if (!strcmp(a, "--version")) { puts("stdbuf (zovutils) 0.10.0"); return 0; }
        if (!strcmp(a, "--")) { i++; break; }
        if (a[0] != '-' || !a[1]) break;
        int which = -1; const char *value = NULL;
        if ((!strcmp(a, "-i") || !strcmp(a, "-o") || !strcmp(a, "-e")) && i + 1 < argc) { which = a[1] == 'i' ? 0 : a[1] == 'o' ? 1 : 2; value = argv[++i]; }
        else if (a[0] == '-' && a[1] && (a[1] == 'i' || a[1] == 'o' || a[1] == 'e') && a[2]) { which = a[1] == 'i' ? 0 : a[1] == 'o' ? 1 : 2; value = a + 2; }
        else if (!strncmp(a, "--input=", 8)) { which = 0; value = a + 8; }
        else if (!strncmp(a, "--output=", 9)) { which = 1; value = a + 9; }
        else if (!strncmp(a, "--error=", 8)) { which = 2; value = a + 8; }
        else if ((!strcmp(a, "--input") || !strcmp(a, "--output") || !strcmp(a, "--error")) && i + 1 < argc) { which = !strcmp(a, "--input") ? 0 : !strcmp(a, "--output") ? 1 : 2; value = argv[++i]; }
        else { fprintf(stderr, "stdbuf: invalid option -- '%s'\nTry 'stdbuf --help' for more information.\n", a); return 1; }
        if (which >= 0) mode[which] = value;
    }
    if (i >= argc) { fprintf(stderr, "stdbuf: missing operand\nTry 'stdbuf --help' for more information.\n"); return 125; }
    for (int m = 0; m < 3; m++) if (strcmp(mode[m], "-")) {
        const char *key = m == 0 ? "_STDBUF_I" : m == 1 ? "_STDBUF_O" : "_STDBUF_E";
        if (setenv(key, mode[m], 1) != 0) { fprintf(stderr, "stdbuf: %s\n", strerror(errno)); return 125; }
    }
    char exe[PATH_MAX];
#if defined(__APPLE__)
    uint32_t size = (uint32_t)sizeof exe;
    if (_NSGetExecutablePath(exe, &size) != 0 || realpath(exe, exe) == NULL) { fprintf(stderr, "stdbuf: cannot locate preload library\n"); return 125; }
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
