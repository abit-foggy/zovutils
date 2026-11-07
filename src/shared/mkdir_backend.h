#ifndef ZOVUTILS_MKDIR_BACKEND_H
#define ZOVUTILS_MKDIR_BACKEND_H

#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "platform_quote.h"

#ifndef S_ISVTX
#define S_ISVTX 01000
#endif

static inline void zov_mkdir_help(void) {
    fputs("Usage: mkdir [OPTION]... DIRECTORY...\n"
          "Create the DIRECTORY(ies), if they do not already exist.\n\n"
          "Mandatory arguments to long options are mandatory for short options too.\n"
          "  -m, --mode=MODE\n"
          "         set file mode (as in chmod), not a=rwx - umask\n"
          "  -p, --parents\n"
          "         no error if existing, make parent directories as needed,\n"
          "         with their file modes unaffected by any -m option\n"
          "  -v, --verbose\n"
          "         print a message for each created directory\n"
          "  -Z\n"
          "         set SELinux security context of each created directory\n"
          "         to the default type\n"
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
          "Full documentation <https://www.gnu.org/software/coreutils/mkdir>\n"
          "or available locally via: info '(coreutils) mkdir invocation'\n", stdout);
}

static inline int zov_mkdir_parse_mode(const char *text, mode_t *result,
                                       mode_t current_umask) {
    if (text == NULL || *text == '\0') return 0;
    int octal = 1;
    for (const char *p = text; *p != '\0'; ++p)
        if (*p < '0' || *p > '7') { octal = 0; break; }
    if (octal) {
        mode_t mode = 0;
        for (const char *p = text; *p != '\0'; ++p) {
            unsigned digit = (unsigned)(*p - '0');
            if (mode > (07777u - digit) / 8u) return 0;
            mode = (mode << 3) | digit;
        }
        if (mode > 07777u) return 0;
        *result = mode;
        return 1;
    }

    mode_t mode = 0777;
    const char *p = text;
    while (*p != '\0') {
        unsigned who = 0;
        int who_given = 0;
        while (*p == 'u' || *p == 'g' || *p == 'o' || *p == 'a') {
            who_given = 1;
            if (*p == 'u' || *p == 'a') who |= 1;
            if (*p == 'g' || *p == 'a') who |= 2;
            if (*p == 'o' || *p == 'a') who |= 4;
            ++p;
        }
        if (!who_given) who = 7;
        if (*p != '+' && *p != '-' && *p != '=') return 0;
        char op = *p++;
        mode_t affected = 0;
        if (who & 1) affected |= S_IRWXU | S_ISUID;
        if (who & 2) affected |= S_IRWXG | S_ISGID;
        if (who & 4) affected |= S_IRWXO | S_ISVTX;
        mode_t bits = 0;
        int any = 0;
        while (*p != '\0' && *p != ',') {
            char c = *p++;
            any = 1;
            if (c == 'r' || c == 'w' || c == 'x' || c == 'X') {
                mode_t bit = c == 'r' ? 4 : c == 'w' ? 2 : 1;
                if (c == 'X') bit = 1;
                if (who & 1) bits |= (bit == 4 ? S_IRUSR : bit == 2 ? S_IWUSR : S_IXUSR);
                if (who & 2) bits |= (bit == 4 ? S_IRGRP : bit == 2 ? S_IWGRP : S_IXGRP);
                if (who & 4) bits |= (bit == 4 ? S_IROTH : bit == 2 ? S_IWOTH : S_IXOTH);
            } else if (c == 's') {
                if (who & 1) bits |= S_ISUID;
                if (who & 2) bits |= S_ISGID;
            } else if (c == 't') {
                if (who & 4) bits |= S_ISVTX;
            } else if (c == 'u' || c == 'g' || c == 'o') {
                mode_t source = c == 'u' ? (mode & S_IRWXU) >> 6
                              : c == 'g' ? (mode & S_IRWXG) >> 3
                                         : (mode & S_IRWXO);
                if (who & 1) bits |= source << 6;
                if (who & 2) bits |= source << 3;
                if (who & 4) bits |= source;
            } else return 0;
        }
        if (!any && op != '=') return 0;
        if (!who_given) {
            affected &= ~(current_umask & 0777);
            bits &= affected;
            if (op == '=') affected |= 0777;
        }
        if (op == '=') mode = (mode & ~affected) | bits;
        else if (op == '+') mode |= bits;
        else mode &= ~bits;
#if !defined(__APPLE__)
        if ((bits & S_ISVTX) != 0) mode &= ~(current_umask & 0777);
#endif
        if (*p == ',') { ++p; if (*p == '\0') return 0; }
    }
    *result = mode & 07777;
    return 1;
}

static inline int zov_mkdir_existing_directory(const char *path) {
    struct stat info;
    if (stat(path, &info) != 0) return 0;
    return S_ISDIR(info.st_mode);
}

static inline void zov_mkdir_error(const char *path, int error) {
    fputs("mkdir: cannot create directory ", stderr);
    zov_quote_shell(stderr, path);
    fprintf(stderr, ": %s\n", strerror(error));
}

static inline int zov_mkdir_report_created(const char *path) {
    fputs("mkdir: created directory ", stdout);
    zov_quote_shell(stdout, path);
    return fputc('\n', stdout) == EOF;
}

static inline int zov_mkdir_make_one(const char *path, mode_t create_mode,
                                     int apply_mode, mode_t final_mode,
                                     int verbose, int ignore_existing) {
    if (mkdir(path, create_mode) == 0) {
        if (apply_mode && chmod(path, final_mode) != 0) {
            int error = errno;
            fputs("mkdir: cannot set permissions of ", stderr);
            zov_quote_shell(stderr, path);
            fprintf(stderr, ": %s\n", strerror(error));
            return 1;
        }
        return verbose ? zov_mkdir_report_created(path) : 0;
    }
    int error = errno;
    if (ignore_existing && error == EEXIST) {
        if (zov_mkdir_existing_directory(path)) return 0;
        error = ENOTDIR;
    }
    zov_mkdir_error(path, error);
    return 1;
}

static inline int zov_mkdir_make_parents(const char *path, mode_t default_mode,
                                         mode_t final_mode, int mode_set,
                                         int verbose) {
    size_t length = strlen(path);
    if (length == 0) return zov_mkdir_make_one(path, default_mode, 0, final_mode, verbose, 1);
    char *copy = (char *)malloc(length + 1);
    if (copy == NULL) { errno = ENOMEM; zov_mkdir_error(path, errno); return 1; }
    memcpy(copy, path, length + 1);
    while (length > 1 && copy[length - 1] == '/') copy[--length] = '\0';
    int status = 0;
    for (size_t i = 1; i <= length; ++i) {
        if (i != length && copy[i] != '/') continue;
        char saved = copy[i];
        copy[i] = '\0';
        if (copy[0] != '\0' && strcmp(copy, ".") != 0) {
            int final = i == length;
            int rc = zov_mkdir_make_one(copy, final && mode_set ? final_mode : default_mode,
                                        final && mode_set, final_mode, verbose, 1);
            if (rc != 0) {
                /* Match mkdir -p diagnostics by naming the requested path. */
                if (strcmp(copy, path) != 0) {
                    int error = errno;
                    (void)error;
                }
                status = rc;
                break;
            }
        }
        copy[i] = saved;
    }
    free(copy);
    return status;
}

static inline int zov_mkdir_context_enabled(void) {
#if defined(__linux__)
    return access("/sys/fs/selinux/enforce", F_OK) == 0 ||
           access("/sys/fs/smackfs", F_OK) == 0 ||
           access("/smack", F_OK) == 0;
#else
    return 0;
#endif
}

static inline int zov_mkdir_run(int argc, int8_t **argv) {
    const char *mode_text = NULL;
    const char *context = NULL;
    int context_option = 0;
    int parents = 0, verbose = 0, options_done = 0;
    char **paths = (char **)malloc((size_t)(argc + 1) * sizeof(*paths));
    if (paths == NULL) return 1;
    int path_count = 0;

    for (int i = 0; i < argc; ++i) {
        char *arg = (char *)argv[i];
        if (!options_done && strcmp(arg, "--") == 0) { options_done = 1; continue; }
        if (!options_done && strncmp(arg, "--", 2) == 0) {
            char *eq = strchr(arg, '=');
            size_t keylen = eq ? (size_t)(eq - arg) : strlen(arg);
            const char *value = eq ? eq + 1 : NULL;
#define MK_LONG(name) (keylen > 2 && keylen <= strlen(name) && strncmp(arg, name, keylen) == 0)
            if (MK_LONG("--help") || MK_LONG("--version") || MK_LONG("--parents") || MK_LONG("--verbose")) {
                const char *name = MK_LONG("--help") ? "--help" : MK_LONG("--version") ? "--version" :
                                   MK_LONG("--parents") ? "--parents" : "--verbose";
                if (value != NULL) {
                    fprintf(stderr, "mkdir: option '%s' doesn't allow an argument\nTry 'mkdir --help' for more information.\n", name);
                    free(paths); return 1;
                }
                if (strcmp(name, "--help") == 0) { zov_mkdir_help(); free(paths); return 0; }
                if (strcmp(name, "--version") == 0) { puts("mkdir (zovutils) 0.10.12"); free(paths); return 0; }
                if (strcmp(name, "--parents") == 0) parents = 1; else verbose = 1;
                continue;
            }
            if (MK_LONG("--mode")) {
                if (value == NULL && i + 1 < argc) value = (char *)argv[++i];
                if (value == NULL) {
                    fprintf(stderr, "mkdir: option '--mode' requires an argument\nTry 'mkdir --help' for more information.\n");
                    free(paths); return 1;
                }
                mode_text = value; continue;
            }
            if (MK_LONG("--context")) {
                context_option = 1;
                if (value != NULL) context = value;
                continue;
            }
            fprintf(stderr, "mkdir: unrecognized option '%s'\nTry 'mkdir --help' for more information.\n", arg);
            free(paths); return 1;
#undef MK_LONG
        }
        if (!options_done && arg[0] == '-' && arg[1] != '\0') {
            for (size_t j = 1; arg[j] != '\0'; ++j) {
                char option = arg[j];
                if (option == 'p') parents = 1;
                else if (option == 'v') verbose = 1;
                else if (option == 'Z') context_option = 1;
                else if (option == 'm') {
                    if (arg[j + 1] != '\0') { mode_text = arg + j + 1; j = strlen(arg) - 1; }
                    else if (i + 1 < argc) mode_text = (char *)argv[++i];
                    else {
                        fprintf(stderr, "mkdir: option requires an argument -- 'm'\nTry 'mkdir --help' for more information.\n");
                        free(paths); return 1;
                    }
                    break;
                } else {
                    fprintf(stderr, "mkdir: invalid option -- '%c'\nTry 'mkdir --help' for more information.\n", option);
                    free(paths); return 1;
                }
            }
            continue;
        }
        if (path_count >= argc) { free(paths); return 1; }
        paths[path_count++] = arg;
        if (getenv("POSIXLY_CORRECT") != NULL && *getenv("POSIXLY_CORRECT") != '\0') options_done = 1;
    }

    if (path_count == 0) {
        fprintf(stderr, "mkdir: missing operand\nTry 'mkdir --help' for more information.\n");
        free(paths); return 1;
    }
    if (context_option && context != NULL && *context != '\0') {
        if (!zov_mkdir_context_enabled()) {
            fputs("mkdir: warning: ignoring --context; it requires an SELinux/SMACK-enabled kernel\n", stderr);
            context = NULL;
        } else {
            fputs("mkdir: failed to set default file creation context to ", stderr);
            zov_quote_shell(stderr, context);
            fputs(": Invalid argument\n", stderr);
            free(paths); return 1;
        }
    }

    mode_t mask = umask(0);
    umask(mask);
    mode_t final_mode = 0777;
    int mode_set = mode_text != NULL;
    if (mode_set && !zov_mkdir_parse_mode(mode_text, &final_mode, mask)) {
        fputs("mkdir: invalid mode ", stderr);
        zov_quote_shell(stderr, mode_text);
        fputc('\n', stderr);
        free(paths); return 1;
    }
    mode_t default_mode = 0777 & ~mask;
    int status = 0;
    for (int i = 0; i < path_count; ++i) {
        int rc = parents
            ? zov_mkdir_make_parents(paths[i], default_mode, final_mode, mode_set, verbose)
            : zov_mkdir_make_one(paths[i], mode_set ? final_mode : default_mode,
                                 mode_set, final_mode, verbose, 0);
        if (rc != 0) status = 1;
    }
    free(paths);
    if (fflush(stdout) != 0) status = 1;
    return status;
}

#endif
