#ifndef ZOVUTILS_NPROC_BACKEND_H
#define ZOVUTILS_NPROC_BACKEND_H
#define _GNU_SOURCE 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static inline int zov_nproc_long(const char *a, const char *name) { size_t n = strlen(a); return n > 2 && n <= strlen(name) && strncmp(a, name, n) == 0; }
static inline int zov_nproc_run(int argc, int8_t **argv) {
    int all = 0; unsigned long ignore = 0;
    for (int i = 0; i < argc; ++i) {
        const char *a = (const char *)argv[i];
        if (!strcmp(a, "--help")) { puts("Usage: nproc [OPTION]...\nPrint the number of processing units available.\n\n      --all       print the number of installed processors\n      --ignore=N  if possible, exclude N processing units\n      --help      display this help and exit\n      --version   output version information and exit\n\nReport bugs to: bug-coreutils@gnu.org\nGNU coreutils home page: <https://www.gnu.org/software/coreutils/>"); return 0; }
        if (!strcmp(a, "--version")) { puts("nproc (zovutils) 0.10.44"); return 0; }
        if (!strcmp(a, "--all")) { all = 1; continue; }
        if (!strncmp(a, "--ignore=", 9) || !strcmp(a, "--ignore")) {
            const char *v = a[8] == '=' ? a + 9 : (i + 1 < argc ? (const char *)argv[++i] : NULL);
            if (!v || !*v) { fputs("nproc: option '--ignore' requires an argument\nTry 'nproc --help' for more information.\n", stderr); return 1; }
            char *end; ignore = strtoul(v, &end, 10); if (*end || end == v) { fprintf(stderr, "nproc: invalid number: '%s'\n", v); return 1; } continue;
        }
        fprintf(stderr, "nproc: unrecognized option '%s'\nTry 'nproc --help' for more information.\n", a); return 1;
    }
    long count = 0;
#ifdef __linux__
    if (!all) {
        FILE *status = fopen("/proc/self/status", "r"); char line[512];
        if (status) {
            while (fgets(line, sizeof(line), status)) if (strncmp(line, "Cpus_allowed_list:", 18) == 0) {
                char *p = line + 18; long total = 0;
                while (*p && *p != '\n') { char *end; long first = strtol(p, &end, 10); if (end == p) break; p = end; long last = first; if (*p == '-') { last = strtol(p + 1, &end, 10); p = end; } if (last >= first) total += last - first + 1; if (*p == ',') ++p; else break; }
                if (total > 0) count = total; break;
            }
            fclose(status);
        }
    }
#endif
    if (count <= 0) { count = sysconf(_SC_NPROCESSORS_ONLN); if (count <= 0) count = 1; }
    if (all) { long installed = sysconf(_SC_NPROCESSORS_CONF); if (installed > 0) count = installed; }
    const char *omp = getenv("OMP_NUM_THREADS");
    if (!all && omp && *omp) { char *end; unsigned long n = strtoul(omp, &end, 10); if (end != omp && (*end == 0 || *end == ',')) if (n > 0 && n < (unsigned long)count) count = (long)n; }
    if (ignore >= (unsigned long)count) count = 1; else count -= (long)ignore;
    printf("%ld\n", count); return ferror(stdout) ? 1 : 0;
}
#endif
