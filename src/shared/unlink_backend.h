#ifndef ZOVUTILS_UNLINK_BACKEND_H
#define ZOVUTILS_UNLINK_BACKEND_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "platform_quote.h"

static inline void zov_unlink_help(void) {
    fputs("Usage: unlink FILE\n"
          "  or:  unlink OPTION\n"
          "Call the unlink function to remove the specified FILE.\n\n"
          "      --help\n"
          "         display this help and exit\n"
          "      --version\n"
          "         output version information and exit\n\n"
          "Report bugs to: bug-coreutils@gnu.org\n"
          "GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n"
          "General help using GNU software: <https://www.gnu.org/gethelp/>\n"
          "Report any translation bugs to <https://translationproject.org/team/>\n"
          "Full documentation <https://www.gnu.org/software/coreutils/unlink>\n"
          "or available locally via: info '(coreutils) unlink invocation'\n", stdout);
}

static inline int zov_unlink_long(const char *arg, const char *name) {
    size_t length = strlen(arg);
    return length > 2 && length <= strlen(name) && strncmp(arg, name, length) == 0;
}

static inline int zov_unlink_run(int argc, int8_t **argv) {
    const char *file = NULL;
    int options_done = 0;
    for (int i = 0; i < argc; ++i) {
        const char *arg = (const char *)argv[i];
        if (!options_done && strcmp(arg, "--") == 0) { options_done = 1; continue; }
        if (!options_done && strncmp(arg, "--", 2) == 0) {
            const char *equal = strchr(arg, '=');
            size_t keylen = equal ? (size_t)(equal - arg) : strlen(arg);
            char key[32];
            if (keylen < sizeof(key)) { memcpy(key, arg, keylen); key[keylen] = '\0'; }
            else key[0] = '\0';
            if (key[0] != '\0' && zov_unlink_long(key, "--help")) {
                if (equal != NULL) { fputs("unlink: option '--help' doesn't allow an argument\nTry 'unlink --help' for more information.\n", stderr); return 1; }
                zov_unlink_help(); return 0;
            }
            if (key[0] != '\0' && zov_unlink_long(key, "--version")) {
                if (equal != NULL) { fputs("unlink: option '--version' doesn't allow an argument\nTry 'unlink --help' for more information.\n", stderr); return 1; }
                puts("unlink (zovutils) 0.10.0"); return 0;
            }
            fprintf(stderr, "unlink: unrecognized option '%s'\nTry 'unlink --help' for more information.\n", arg);
            return 1;
        }
        if (!options_done && arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "unlink: invalid option -- '%c'\nTry 'unlink --help' for more information.\n", arg[1]);
            return 1;
        }
        if (file != NULL) {
            fputs("unlink: extra operand ", stderr); zov_quote_shell(stderr, arg);
            fputs("\nTry 'unlink --help' for more information.\n", stderr); return 1;
        }
        file = arg;
    }
    if (file == NULL) {
        fputs("unlink: missing operand\nTry 'unlink --help' for more information.\n", stderr); return 1;
    }
    if (unlink(file) == 0) return 0;
    int error = errno;
    fputs("unlink: cannot unlink ", stderr); zov_quote_shell(stderr, file);
    fprintf(stderr, ": %s\n", strerror(error));
    return 1;
}

#endif
