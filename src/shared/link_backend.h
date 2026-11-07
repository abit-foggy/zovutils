#ifndef ZOVUTILS_LINK_BACKEND_H
#define ZOVUTILS_LINK_BACKEND_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "platform_quote.h"

static inline void zov_link_help(void) {
    fputs("Usage: link FILE1 FILE2\n"
          "  or:  link OPTION\n"
          "Call the link function to create a link named FILE2 to an existing FILE1.\n\n"
          "      --help\n"
          "         display this help and exit\n"
          "      --version\n"
          "         output version information and exit\n\n"
          "Report bugs to: bug-coreutils@gnu.org\n"
          "GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n"
          "General help using GNU software: <https://www.gnu.org/gethelp/>\n"
          "Report any translation bugs to <https://translationproject.org/team/>\n"
          "Full documentation <https://www.gnu.org/software/coreutils/link>\n"
          "or available locally via: info '(coreutils) link invocation'\n", stdout);
}

static inline int zov_link_long(const char *arg, const char *name) {
    size_t length = strlen(arg);
    return length > 2 && length <= strlen(name) && strncmp(arg, name, length) == 0;
}

static inline int zov_link_run(int argc, int8_t **argv) {
    const char *operands[2] = {NULL, NULL};
    int count = 0, options_done = 0;
    for (int i = 0; i < argc; ++i) {
        const char *arg = (const char *)argv[i];
        if (!options_done && strcmp(arg, "--") == 0) { options_done = 1; continue; }
        if (!options_done && strncmp(arg, "--", 2) == 0) {
            const char *equal = strchr(arg, '=');
            size_t keylen = equal ? (size_t)(equal - arg) : strlen(arg);
            char key[32];
            if (keylen < sizeof(key)) { memcpy(key, arg, keylen); key[keylen] = '\0'; }
            else key[0] = '\0';
            if (key[0] != '\0' && zov_link_long(key, "--help")) {
                if (equal != NULL) { fputs("link: option '--help' doesn't allow an argument\nTry 'link --help' for more information.\n", stderr); return 1; }
                zov_link_help(); return 0;
            }
            if (key[0] != '\0' && zov_link_long(key, "--version")) {
                if (equal != NULL) { fputs("link: option '--version' doesn't allow an argument\nTry 'link --help' for more information.\n", stderr); return 1; }
                puts("link (zovutils) 0.10.6"); return 0;
            }
            fprintf(stderr, "link: unrecognized option '%s'\nTry 'link --help' for more information.\n", arg);
            return 1;
        }
        if (!options_done && arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "link: invalid option -- '%c'\nTry 'link --help' for more information.\n", arg[1]);
            return 1;
        }
        if (count == 2) {
            fputs("link: extra operand ", stderr); zov_quote_shell(stderr, arg);
            fputs("\nTry 'link --help' for more information.\n", stderr); return 1;
        }
        operands[count++] = arg;
    }
    if (count == 0) {
        fputs("link: missing operand\nTry 'link --help' for more information.\n", stderr); return 1;
    }
    if (count == 1) {
        fputs("link: missing operand after ", stderr); zov_quote_shell(stderr, operands[0]);
        fputs("\nTry 'link --help' for more information.\n", stderr); return 1;
    }
    if (link(operands[0], operands[1]) == 0) return 0;
    int error = errno;
    fputs("link: cannot create link ", stderr); zov_quote_shell(stderr, operands[1]);
    fputs(" to ", stderr); zov_quote_shell(stderr, operands[0]);
    fprintf(stderr, ": %s\n", strerror(error));
    return 1;
}

#endif
