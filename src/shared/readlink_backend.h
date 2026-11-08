#ifndef ZOVUTILS_READLINK_BACKEND_H
#define ZOVUTILS_READLINK_BACKEND_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "platform_path.h"
#include "platform_quote.h"

static inline void zov_readlink_help(void) {
    fputs("Usage: readlink [OPTION]... FILE...\n"
          "Print value of a symbolic link or canonical file name\n\n"
          "  -f, --canonicalize             canonicalize by following every symlink\n"
          "                                 in every component of the given name recursively;\n"
          "                                 all but the last component must exist\n"
          "  -e, --canonicalize-existing    canonicalize by following every symlink\n"
          "                                 in every component of the given name recursively;\n"
          "                                 all components must exist\n"
          "  -m, --canonicalize-missing     canonicalize by following every symlink\n"
          "                                 in every component of the given name recursively,\n"
          "                                 without requirements on components existence\n"
          "  -n, --no-newline               do not output the trailing delimiter\n"
          "  -q, --quiet, -s, --silent      suppress most error messages\n"
          "  -v, --verbose                  report error messages\n"
          "  -z, --zero                     end each output line with NUL\n"
          "      --help                      display this help and exit\n"
          "      --version                   output version information and exit\n\n"
          "Report bugs to: bug-coreutils@gnu.org\n"
          "GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n"
          "General help using GNU software: <https://www.gnu.org/gethelp/>\n"
          "Report any translation bugs to <https://translationproject.org/team/>\n"
          "Full documentation <https://www.gnu.org/software/coreutils/readlink>\n"
          "or available locally via: info '(coreutils) readlink invocation'\n", stdout);
}

static inline int zov_readlink_match(const char *key, const char *name) {
    size_t n = strlen(key);
    return n > 2 && n <= strlen(name) && strncmp(key, name, n) == 0;
}

static inline int zov_readlink_print(const char *path, int mode, int no_newline,
                                     int zero, int quiet, int verbose) {
    char resolved[PATH_MAX];
    if (mode != 0) {
        int resolve_mode = mode == 2 ? 0 : mode == 1 ? 1 : 2;
        if (!zov_path_resolve(path, resolve_mode, 0, 0, resolved, sizeof(resolved))) {
            if (!quiet || verbose) {
                fprintf(stderr, "readlink: %s", path);
                fprintf(stderr, ": %s\n", strerror(errno));
            }
            return 1;
        }
        fputs(resolved, stdout);
    } else {
        size_t cap = 256;
        char *buffer = NULL;
        ssize_t length;
        for (;;) {
            char *next = (char *)realloc(buffer, cap);
            if (next == NULL) { free(buffer); errno = ENOMEM; length = -1; break; }
            buffer = next;
            length = readlink(path, buffer, cap);
            if (length < 0 || (size_t)length < cap) break;
            if (cap > (size_t)-1 / 2) { errno = ENAMETOOLONG; length = -1; break; }
            cap *= 2;
        }
        if (length < 0) {
            int error = errno; free(buffer);
            if (!quiet || verbose) {
                fprintf(stderr, "readlink: %s", path);
                fprintf(stderr, ": %s\n", strerror(error));
            }
            return 1;
        }
        fwrite(buffer, 1, (size_t)length, stdout); free(buffer);
    }
    if (!no_newline) fputc(zero ? '\0' : '\n', stdout);
    return ferror(stdout) ? 1 : 0;
}

static inline int zov_readlink_run(int argc, int8_t **argv) {
    const char *posix = getenv("POSIXLY_CORRECT");
    int mode = 0, no_newline = 0, zero = 0;
    int quiet = posix == NULL || *posix == '\0', verbose = !quiet, options_done = 0;
    const char **paths = (const char **)malloc((size_t)(argc + 1) * sizeof(*paths));
    if (paths == NULL) return 1;
    int count = 0;
    for (int i = 0; i < argc; ++i) {
        const char *arg = (const char *)argv[i];
        if (!options_done && strcmp(arg, "--") == 0) { options_done = 1; continue; }
        if (!options_done && strncmp(arg, "--", 2) == 0) {
            const char *eq = strchr(arg, '='); size_t len = eq ? (size_t)(eq - arg) : strlen(arg);
            char key[64]; if (len < sizeof(key)) { memcpy(key, arg, len); key[len] = '\0'; } else key[0] = '\0';
#define RL_LONG(name) (key[0] && zov_readlink_match(key, name))
            if (RL_LONG("--help") || RL_LONG("--version")) {
                const char *name = RL_LONG("--help") ? "--help" : "--version";
                if (eq) { fprintf(stderr, "readlink: option '%s' doesn't allow an argument\nTry 'readlink --help' for more information.\n", name); free(paths); return 1; }
                if (strcmp(name, "--help") == 0) zov_readlink_help(); else puts("readlink (zovutils) 0.10.51");
                free(paths); return 0;
            }
            if (RL_LONG("--canonicalize")) { mode = 1; continue; }
            if (RL_LONG("--canonicalize-existing")) { mode = 2; continue; }
            if (RL_LONG("--canonicalize-missing")) { mode = 3; continue; }
            if (RL_LONG("--no-newline")) { no_newline = 1; continue; }
            if (RL_LONG("--quiet") || RL_LONG("--silent")) { quiet = 1; continue; }
            if (RL_LONG("--verbose")) { verbose = 1; quiet = 0; continue; }
            if (RL_LONG("--zero")) { zero = 1; continue; }
            fprintf(stderr, "readlink: unrecognized option '%s'\nTry 'readlink --help' for more information.\n", arg);
            free(paths); return 1;
#undef RL_LONG
        }
        if (!options_done && arg[0] == '-' && arg[1] != '\0') {
            for (size_t j = 1; arg[j] != '\0'; ++j) {
                if (arg[j] == 'f') mode = 1;
                else if (arg[j] == 'e') mode = 2;
                else if (arg[j] == 'm') mode = 3;
                else if (arg[j] == 'n') no_newline = 1;
                else if (arg[j] == 'q' || arg[j] == 's') quiet = 1;
                else if (arg[j] == 'v') { verbose = 1; quiet = 0; }
                else if (arg[j] == 'z') zero = 1;
                else { fprintf(stderr, "readlink: invalid option -- '%c'\nTry 'readlink --help' for more information.\n", arg[j]); free(paths); return 1; }
            }
            continue;
        }
        paths[count++] = arg;
        if (getenv("POSIXLY_CORRECT") != NULL && *getenv("POSIXLY_CORRECT") != '\0') options_done = 1;
    }
    if (count == 0) { fputs("readlink: missing operand\nTry 'readlink --help' for more information.\n", stderr); free(paths); return 1; }
    int status = 0;
    for (int i = 0; i < count; ++i) if (zov_readlink_print(paths[i], mode, no_newline, zero, quiet, verbose) != 0) status = 1;
    free(paths);
    if (fflush(stdout) != 0) status = 1;
    return status;
}

#endif
