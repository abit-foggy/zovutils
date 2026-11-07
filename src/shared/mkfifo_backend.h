#ifndef ZOVUTILS_MKFIFO_BACKEND_H
#define ZOVUTILS_MKFIFO_BACKEND_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "platform_quote.h"
#include "mkdir_backend.h"

static inline void zov_mkfifo_help(void) {
    fputs("Usage: mkfifo [OPTION]... NAME...\n"
          "Create named pipes (FIFOs) with the given NAMEs.\n\n"
          "Mandatory arguments to long options are mandatory for short options too.\n"
          "  -m, --mode=MODE\n"
          "         set file permission bits to MODE, not a=rw - umask\n"
          "  -Z\n"
          "         set the SELinux security context to default type\n"
          "      --context[=CTX]\n"
          "         like -Z, or if CTX is specified then set the\n"
          "         SELinux or SMACK security context to CTX\n"
          "      --help\n"
          "         display this help and exit\n"
          "      --version\n"
          "         output version information and exit\n\n"
          "Report bugs to: bug-coreutils@gnu.org\n"
          "GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n"
          "General help using GNU software: <https://www.gnu.org/gethelp/>\n"
          "Report any translation bugs to <https://translationproject.org/team/>\n"
          "Full documentation <https://www.gnu.org/software/coreutils/mkfifo>\n"
          "or available locally via: info '(coreutils) mkfifo invocation'\n", stdout);
}

static inline void zov_mkfifo_error(const char *path, int error) {
    fputs("mkfifo: cannot create fifo ", stderr); zov_quote_shell(stderr, path);
    fprintf(stderr, ": %s\n", strerror(error));
}

static inline int zov_mkfifo_run(int argc, int8_t **argv) {
    const char *mode_text = NULL, *context = NULL;
    int context_option = 0, options_done = 0, path_count = 0;
    const char **paths = (const char **)malloc((size_t)(argc + 1) * sizeof(*paths));
    if (paths == NULL) return 1;
    for (int i = 0; i < argc; ++i) {
        char *arg = (char *)argv[i];
        if (!options_done && strcmp(arg, "--") == 0) { options_done = 1; continue; }
        if (!options_done && strncmp(arg, "--", 2) == 0) {
            char *eq = strchr(arg, '=');
            size_t keylen = eq ? (size_t)(eq - arg) : strlen(arg);
            const char *value = eq ? eq + 1 : NULL;
#define MKFIFO_LONG(name) (keylen > 2 && keylen <= strlen(name) && strncmp(arg, name, keylen) == 0)
            if (MKFIFO_LONG("--help") || MKFIFO_LONG("--version")) {
                const char *name = MKFIFO_LONG("--help") ? "--help" : "--version";
                if (value != NULL) {
                    fprintf(stderr, "mkfifo: option '%s' doesn't allow an argument\nTry 'mkfifo --help' for more information.\n", name);
                    free(paths); return 1;
                }
                if (strcmp(name, "--help") == 0) zov_mkfifo_help();
                else puts("mkfifo (zovutils) 0.10.19");
                free(paths); return 0;
            }
            if (MKFIFO_LONG("--mode")) {
                if (value == NULL && i + 1 < argc) value = (char *)argv[++i];
                if (value == NULL) {
                    fputs("mkfifo: option '--mode' requires an argument\nTry 'mkfifo --help' for more information.\n", stderr);
                    free(paths); return 1;
                }
                mode_text = value; continue;
            }
            if (MKFIFO_LONG("--context")) { context_option = 1; if (value != NULL) context = value; continue; }
            fprintf(stderr, "mkfifo: unrecognized option '%s'\nTry 'mkfifo --help' for more information.\n", arg);
            free(paths); return 1;
#undef MKFIFO_LONG
        }
        if (!options_done && arg[0] == '-' && arg[1] != '\0') {
            for (size_t j = 1; arg[j] != '\0'; ++j) {
                if (arg[j] == 'Z') context_option = 1;
                else if (arg[j] == 'm') {
                    if (arg[j + 1] != '\0') { mode_text = arg + j + 1; j = strlen(arg) - 1; }
                    else if (i + 1 < argc) mode_text = (char *)argv[++i];
                    else {
                        fputs("mkfifo: option requires an argument -- 'm'\nTry 'mkfifo --help' for more information.\n", stderr);
                        free(paths); return 1;
                    }
                    break;
                } else {
                    fprintf(stderr, "mkfifo: invalid option -- '%c'\nTry 'mkfifo --help' for more information.\n", arg[j]);
                    free(paths); return 1;
                }
            }
            continue;
        }
        paths[path_count++] = arg;
    }
    if (path_count == 0) {
        fputs("mkfifo: missing operand\nTry 'mkfifo --help' for more information.\n", stderr);
        free(paths); return 1;
    }
    if (context_option && context != NULL && *context != '\0') {
        fputs("mkfifo: failed to set default file creation context to ", stderr);
        zov_quote_shell(stderr, context); fputs(": Invalid argument\n", stderr);
        free(paths); return 1;
    }
    mode_t mask = umask(0); umask(mask);
    mode_t final_mode = 0666;
    int mode_set = mode_text != NULL;
    if (mode_set && !zov_mkdir_parse_mode(mode_text, &final_mode, mask)) {
        fputs("mkfifo: invalid mode\n", stderr);
        free(paths); return 1;
    }
    mode_t create_mode = mode_set ? final_mode : (0666 & ~mask);
    int status = 0;
    for (int i = 0; i < path_count; ++i) {
        if (mkfifo(paths[i], create_mode) != 0) {
            zov_mkfifo_error(paths[i], errno); status = 1; continue;
        }
        if (mode_set && chmod(paths[i], final_mode) != 0) {
            zov_mkfifo_error(paths[i], errno); status = 1;
        }
    }
    free(paths);
    return status;
}

#endif
