#ifndef ZOVUTILS_RMDIR_BACKEND_H
#define ZOVUTILS_RMDIR_BACKEND_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "platform_quote.h"

static inline void zov_rmdir_help(void) {
    fputs("Usage: rmdir [OPTION]... DIRECTORY...\n"
          "Remove the DIRECTORY(ies), if they are empty.\n\n"
          "      --ignore-fail-on-non-empty\n"
          "         ignore each failure to remove a non-empty directory\n"
          "  -p, --parents\n"
          "         remove DIRECTORY and its ancestors;\n"
          "         e.g., 'rmdir -p a/b' is similar to 'rmdir a/b a'\n"
          "  -v, --verbose\n"
          "         output a diagnostic for every directory processed\n"
          "      --help\n"
          "         display this help and exit\n"
          "      --version\n"
          "         output version information and exit\n\n"
          "Report bugs to: bug-coreutils@gnu.org\n"
          "GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n"
          "General help using GNU software: <https://www.gnu.org/gethelp/>\n"
          "Report any translation bugs to <https://translationproject.org/team/>\n"
          "Full documentation <https://www.gnu.org/software/coreutils/rmdir>\n"
          "or available locally via: info '(coreutils) rmdir invocation'\n", stdout);
}

static inline int zov_rmdir_long(const char *arg, const char *option, size_t keylen) {
    return keylen > 2 && keylen <= strlen(option) && strncmp(arg, option, keylen) == 0;
}

static inline void zov_rmdir_error(const char *path, int error, int ancestor) {
    int ancestor_error = ancestor && error != ENOTDIR && error != ENOENT;
    fputs(ancestor_error ? "rmdir: failed to remove directory " : "rmdir: failed to remove ", stderr);
    zov_quote_shell(stderr, path);
    fprintf(stderr, ": %s\n", strerror(error));
}

static inline int zov_rmdir_remove_one(const char *path, int verbose,
                                       int ignore_nonempty, int ancestor) {
    if (verbose) {
        fputs("rmdir: removing directory, ", stdout);
        zov_quote_shell(stdout, path);
        fputc('\n', stdout);
    }
    if (rmdir(path) == 0) return 0;
    int error = errno;
    if (ignore_nonempty && (error == ENOTEMPTY || error == EEXIST)) return 0;
    zov_rmdir_error(path, error, ancestor);
    return 1;
}

static inline int zov_rmdir_path(const char *path, int parents, int verbose,
                                 int ignore_nonempty) {
    size_t length = strlen(path);
    char *copy = (char *)malloc(length + 1);
    if (copy == NULL) { zov_rmdir_error(path, ENOMEM, 0); return 1; }
    memcpy(copy, path, length + 1);
    while (length > 1 && copy[length - 1] == '/') copy[--length] = '\0';
    int status = 0;
    int ancestor = 0;
    for (;;) {
        if (zov_rmdir_remove_one(copy, verbose, ignore_nonempty, ancestor)) {
            status = 1;
            break;
        }
        if (!parents) break;
        while (length > 1 && copy[length - 1] == '/') copy[--length] = '\0';
        size_t slash = length;
        while (slash > 0 && copy[slash - 1] != '/') --slash;
        if (slash == 0) break;
        if (slash == 1) {
            copy[1] = '\0';
            length = 1;
        } else {
            length = slash - 1;
            copy[length] = '\0';
            while (length > 1 && copy[length - 1] == '/') copy[--length] = '\0';
        }
        ancestor = 1;
    }
    free(copy);
    return status;
}

static inline int zov_rmdir_run(int argc, int8_t **argv) {
    int parents = 0, verbose = 0, ignore_nonempty = 0, options_done = 0;
    char **paths = (char **)malloc((size_t)(argc + 1) * sizeof(*paths));
    if (paths == NULL) return 1;
    int count = 0;

    for (int i = 0; i < argc; ++i) {
        char *arg = (char *)argv[i];
        if (!options_done && strcmp(arg, "--") == 0) { options_done = 1; continue; }
        if (!options_done && strncmp(arg, "--", 2) == 0) {
            char *equal = strchr(arg, '=');
            size_t keylen = equal ? (size_t)(equal - arg) : strlen(arg);
            const char *value = equal ? equal + 1 : NULL;
            if (zov_rmdir_long(arg, "--help", keylen) ||
                zov_rmdir_long(arg, "--version", keylen) ||
                zov_rmdir_long(arg, "--parents", keylen) ||
                zov_rmdir_long(arg, "--verbose", keylen) ||
                zov_rmdir_long(arg, "--ignore-fail-on-non-empty", keylen)) {
                const char *name = zov_rmdir_long(arg, "--help", keylen) ? "--help" :
                                   zov_rmdir_long(arg, "--version", keylen) ? "--version" :
                                   zov_rmdir_long(arg, "--parents", keylen) ? "--parents" :
                                   zov_rmdir_long(arg, "--verbose", keylen) ? "--verbose" :
                                   "--ignore-fail-on-non-empty";
                if (value != NULL) {
                    fprintf(stderr, "rmdir: option '%s' doesn't allow an argument\nTry 'rmdir --help' for more information.\n", name);
                    free(paths); return 1;
                }
                if (strcmp(name, "--help") == 0) { zov_rmdir_help(); free(paths); return 0; }
                if (strcmp(name, "--version") == 0) { puts("rmdir (zovutils) 0.10.33"); free(paths); return 0; }
                if (strcmp(name, "--parents") == 0) parents = 1;
                else if (strcmp(name, "--verbose") == 0) verbose = 1;
                else ignore_nonempty = 1;
                continue;
            }
            fprintf(stderr, "rmdir: unrecognized option '%s'\nTry 'rmdir --help' for more information.\n", arg);
            free(paths); return 1;
        }
        if (!options_done && arg[0] == '-' && arg[1] != '\0') {
            for (size_t j = 1; arg[j] != '\0'; ++j) {
                if (arg[j] == 'p') parents = 1;
                else if (arg[j] == 'v') verbose = 1;
                else {
                    fprintf(stderr, "rmdir: invalid option -- '%c'\nTry 'rmdir --help' for more information.\n", arg[j]);
                    free(paths); return 1;
                }
            }
            continue;
        }
        paths[count++] = arg;
        const char *posixly_correct = getenv("POSIXLY_CORRECT");
        if (posixly_correct != NULL && *posixly_correct != '\0') options_done = 1;
    }

    if (count == 0) {
        fprintf(stderr, "rmdir: missing operand\nTry 'rmdir --help' for more information.\n");
        free(paths); return 1;
    }
    int status = 0;
    for (int i = 0; i < count; ++i)
        if (zov_rmdir_path(paths[i], parents, verbose, ignore_nonempty)) status = 1;
    free(paths);
    if (fflush(stdout) != 0) status = 1;
    return status;
}

#endif
