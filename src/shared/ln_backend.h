#ifndef ZOVUTILS_LN_BACKEND_H
#define ZOVUTILS_LN_BACKEND_H

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "platform_quote.h"

#ifndef AT_FDCWD
#define AT_FDCWD -100
#endif
#ifndef AT_SYMLINK_FOLLOW
#define AT_SYMLINK_FOLLOW 0x400
#endif

#if defined(__APPLE__)
#define ZOV_LN_DEFAULT_LINK_OPTION "-L"
#define ZOV_LN_DEFAULT_LOGICAL 1
#else
#define ZOV_LN_DEFAULT_LINK_OPTION "-P"
#define ZOV_LN_DEFAULT_LOGICAL 0
#endif

static inline int zov_ln_long(const char *arg, const char *name, size_t keylen) {
    return keylen > 2 && keylen <= strlen(name) && strncmp(arg, name, keylen) == 0;
}

static inline void zov_ln_help(void) {
    fputs("Usage: ln [OPTION]... [-T] TARGET LINK_NAME\n"
          "  or:  ln [OPTION]... TARGET\n"
          "  or:  ln [OPTION]... TARGET... DIRECTORY\n"
          "  or:  ln [OPTION]... -t DIRECTORY TARGET...\n"
          "In the 1st form, create a link to TARGET with the name LINK_NAME.\n"
          "In the 2nd form, create a link to TARGET in the current directory.\n"
          "In the 3rd and 4th forms, create links to each TARGET in DIRECTORY.\n"
          "Create hard links by default, symbolic links with --symbolic.\n"
          "By default, each destination (name of new link) should not already exist.\n"
          "When creating hard links, each TARGET must exist.  Symbolic links\n"
          "can hold arbitrary text; if later resolved, a relative link is\n"
          "interpreted in relation to its parent directory.\n"
          "\n"
          "Mandatory arguments to long options are mandatory for short options too.\n"
          "      --backup[=CONTROL]\n"
          "         make a backup of each existing destination file\n"
          "  -b\n"
          "         like --backup but does not accept an argument\n"
          "  -d, -F, --directory\n"
          "         allow the superuser to attempt to hard link directories,\n"
          "         if supported by the system\n"
          "  -f, --force\n"
          "         remove existing destination files\n"
          "  -i, --interactive\n"
          "         prompt whether to remove destinations\n"
          "  -L, --logical\n"
          "         dereference TARGETs that are symbolic links\n"
          "  -n, --no-dereference\n"
          "         treat LINK_NAME as a normal file\n"
          "         if it is a symbolic link to a directory\n"
          "  -P, --physical\n"
          "         make hard links directly to symbolic links\n"
          "  -r, --relative\n"
          "         with -s, create links relative to link location\n"
          "  -s, --symbolic\n"
          "         make symbolic links instead of hard links\n"
          "  -S, --suffix=SUFFIX\n"
          "         override the usual backup suffix\n"
          "  -t, --target-directory=DIRECTORY\n"
          "         specify the DIRECTORY in which to create the links\n"
          "  -T, --no-target-directory\n"
          "         treat LINK_NAME as a normal file always\n"
          "  -v, --verbose\n"
          "         print name of each linked file\n"
          "      --help\n"
          "         display this help and exit\n"
          "      --version\n"
          "         output version information and exit\n"
          "\n"
          "The backup suffix is '~', unless set with --suffix or SIMPLE_BACKUP_SUFFIX.\n"
          "The version control method may be selected via the --backup option or through\n"
          "the VERSION_CONTROL environment variable.  Here are the values:\n"
          "\n"
          "  none, off       never make backups (even if --backup is given)\n"
          "  numbered, t     make numbered backups\n"
          "  existing, nil   numbered if numbered backups exist, simple otherwise\n"
          "  simple, never   always make simple backups\n"
          "\n"
          "Using -s ignores -L and -P.  Otherwise, the last option specified controls\n"
          "behavior when a TARGET is a symbolic link, defaulting to " ZOV_LN_DEFAULT_LINK_OPTION ".\n"
          "\n"
          "Report bugs to: bug-coreutils@gnu.org\n"
          "GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n"
          "General help using GNU software: <https://www.gnu.org/gethelp/>\n"
          "Report any translation bugs to <https://translationproject.org/team/>\n"
          "Full documentation <https://www.gnu.org/software/coreutils/ln>\n"
          "or available locally via: info '(coreutils) ln invocation'\n", stdout);
}

static inline char *zov_ln_basename(const char *path) {
    size_t length = strlen(path);
    while (length > 1 && path[length - 1] == '/') --length;
    const char *end = path + length;
    const char *slash = end;
    while (slash > path && slash[-1] != '/') --slash;
    size_t base_length = (size_t)(end - slash);
    if (base_length == 0) { slash = "/"; base_length = 1; }
    char *base = (char *)malloc(base_length + 1);
    if (base == NULL) return NULL;
    memcpy(base, slash, base_length);
    base[base_length] = '\0';
    return base;
}

static inline char *zov_ln_join(const char *dir, const char *name) {
    size_t a = strlen(dir), b = strlen(name);
    int slash = a > 0 && dir[a - 1] != '/';
    if (a > (size_t)-1 - b - (size_t)slash - 1) return NULL;
    char *joined = (char *)malloc(a + b + (size_t)slash + 1);
    if (joined == NULL) return NULL;
    memcpy(joined, dir, a);
    if (slash) joined[a++] = '/';
    memcpy(joined + a, name, b + 1);
    return joined;
}

static inline int zov_ln_is_dir(const char *path, int nofollow) {
    struct stat info;
    if ((nofollow ? lstat(path, &info) : stat(path, &info)) != 0) return 0;
    return S_ISDIR(info.st_mode);
}

static inline int zov_ln_report_error(const char *path, const char *target,
                                      int symbolic, int error) {
    fputs(symbolic ? "ln: failed to create symbolic link " : "ln: failed to create hard link ", stderr);
    zov_quote_shell(stderr, path);
    if (target != NULL && error != EEXIST) {
        fputs(symbolic ? " -> " : " => ", stderr);
        zov_quote_shell(stderr, target);
    }
    fprintf(stderr, ": %s\n", strerror(error));
    return 1;
}

static inline int zov_ln_backup_control(const char *value) {
    if (value == NULL) return 0;
    if (strcmp(value, "none") == 0 || strcmp(value, "off") == 0) return 4;
    if (strcmp(value, "simple") == 0 || strcmp(value, "never") == 0) return 1;
    if (strcmp(value, "numbered") == 0 || strcmp(value, "t") == 0) return 2;
    if (strcmp(value, "existing") == 0 || strcmp(value, "nil") == 0) return 3;
    return -1;
}

static inline char *zov_ln_backup_name(const char *path, const char *suffix,
                                       int backup_control) {
    int next_number = 1;
    if (backup_control == 2 || backup_control == 3) {
        for (int number = 1; number < 100000; ++number) {
            int needed = snprintf(NULL, 0, "%s.~%d~", path, number);
            if (needed < 0) return NULL;
            char *candidate = (char *)malloc((size_t)needed + 1);
            if (candidate == NULL) return NULL;
            snprintf(candidate, (size_t)needed + 1, "%s.~%d~", path, number);
            struct stat info;
            int exists = lstat(candidate, &info) == 0;
            free(candidate);
            if (!exists) { next_number = number; break; }
            next_number = number + 1;
        }
        if (backup_control == 3 && next_number == 1) backup_control = 1;
        else if (backup_control == 3) backup_control = 2;
    }
    if (backup_control == 2) {
        int number = next_number;
        if (number == 100000) { errno = EOVERFLOW; return NULL; }
        int needed = snprintf(NULL, 0, "%s.~%d~", path, number);
        if (needed < 0) return NULL;
        char *result = (char *)malloc((size_t)needed + 1);
        if (result == NULL) return NULL;
        snprintf(result, (size_t)needed + 1, "%s.~%d~", path, number);
        return result;
    }
    size_t a = strlen(path), b = strlen(suffix);
    if (a > (size_t)-1 - b - 1) { errno = ENOMEM; return NULL; }
    char *result = (char *)malloc(a + b + 1);
    if (result == NULL) return NULL;
    memcpy(result, path, a);
    memcpy(result + a, suffix, b + 1);
    return result;
}

static inline int zov_ln_backup_destination(const char *path, const char *suffix,
                                            int backup_control) {
    char *backup = zov_ln_backup_name(path, suffix, backup_control);
    if (backup == NULL) { errno = ENOMEM; return -1; }
    int result = rename(path, backup);
    free(backup);
    return result;
}

static inline char *zov_ln_absolute_lexical(const char *path) {
    char *input;
    if (path[0] == '/') input = strdup(path);
    else {
        char *cwd = getcwd(NULL, 0);
        if (cwd == NULL) return NULL;
        input = zov_ln_join(cwd, path);
        free(cwd);
    }
    if (input == NULL) return NULL;
    size_t length = strlen(input);
    char *output = (char *)malloc(length + 2);
    size_t *starts = (size_t *)malloc((length + 1) * sizeof(*starts));
    if (output == NULL || starts == NULL) { free(input); free(output); free(starts); return NULL; }
    size_t out = 1, depth = 0;
    output[0] = '/';
    char *save = NULL;
    for (char *part = strtok_r(input, "/", &save); part != NULL; part = strtok_r(NULL, "/", &save)) {
        if (strcmp(part, ".") == 0 || part[0] == '\0') continue;
        if (strcmp(part, "..") == 0) {
            if (depth != 0) { out = starts[--depth]; output[out] = '\0'; }
            continue;
        }
        starts[depth++] = out;
        size_t part_length = strlen(part);
        if (out > 1) output[out++] = '/';
        memcpy(output + out, part, part_length);
        out += part_length;
        output[out] = '\0';
    }
    output[out] = '\0';
    free(starts);
    free(input);
    return output;
}

static inline char *zov_ln_relative_target(const char *source, const char *destination) {
    char *source_path = realpath(source, NULL);
    if (source_path == NULL) source_path = zov_ln_absolute_lexical(source);
    if (source_path == NULL) return NULL;
    char *parent = strdup(destination);
    if (parent == NULL) { free(source_path); return NULL; }
    char *slash = strrchr(parent, '/');
    if (slash == NULL) strcpy(parent, ".");
    else if (slash == parent) slash[1] = '\0';
    else *slash = '\0';
    char *dest_path = realpath(parent, NULL);
    if (dest_path == NULL) dest_path = zov_ln_absolute_lexical(parent);
    free(parent);
    if (dest_path == NULL) { free(source_path); return NULL; }

    char *source_copy = strdup(source_path), *dest_copy = strdup(dest_path);
    free(source_path); free(dest_path);
    if (source_copy == NULL || dest_copy == NULL) { free(source_copy); free(dest_copy); return NULL; }
    char **source_parts = (char **)malloc((strlen(source_copy) + 1) * sizeof(*source_parts));
    char **dest_parts = (char **)malloc((strlen(dest_copy) + 1) * sizeof(*dest_parts));
    if (source_parts == NULL || dest_parts == NULL) { free(source_copy); free(dest_copy); free(source_parts); free(dest_parts); return NULL; }
    size_t source_count = 0, dest_count = 0;
    char *save_source = NULL, *save_dest = NULL;
    for (char *part = strtok_r(source_copy, "/", &save_source); part != NULL; part = strtok_r(NULL, "/", &save_source)) source_parts[source_count++] = part;
    for (char *part = strtok_r(dest_copy, "/", &save_dest); part != NULL; part = strtok_r(NULL, "/", &save_dest)) dest_parts[dest_count++] = part;
    size_t common = 0;
    while (common < source_count && common < dest_count && strcmp(source_parts[common], dest_parts[common]) == 0) ++common;
    if (dest_count > ((size_t)-1 - strlen(source_copy) - 4) / 3) {
        free(source_copy); free(dest_copy); free(source_parts); free(dest_parts); return NULL;
    }
    size_t capacity = strlen(source_copy) + dest_count * 3 + 4;
    char *relative = (char *)malloc(capacity);
    if (relative == NULL) { free(source_copy); free(dest_copy); free(source_parts); free(dest_parts); return NULL; }
    size_t out = 0;
    for (size_t i = common; i < dest_count; ++i) {
        if (out != 0) relative[out++] = '/';
        relative[out++] = '.'; relative[out++] = '.';
    }
    for (size_t i = common; i < source_count; ++i) {
        if (out != 0) relative[out++] = '/';
        size_t part_length = strlen(source_parts[i]);
        memcpy(relative + out, source_parts[i], part_length);
        out += part_length;
    }
    if (out == 0) relative[out++] = '.';
    relative[out] = '\0';
    free(source_copy); free(dest_copy); free(source_parts); free(dest_parts);
    return relative;
}

static inline int zov_ln_create_one(const char *source, const char *destination,
                                    int symbolic, int logical, int force,
                                    int no_dereference, int relative, int verbose,
                                    int backup, int backup_control,
                                    const char *suffix) {
    if (!symbolic) {
        struct stat source_info;
        int stat_result = logical ? stat(source, &source_info) : lstat(source, &source_info);
        if (stat_result != 0) {
            fputs("ln: failed to access ", stderr);
            zov_quote_shell(stderr, source);
            fprintf(stderr, ": %s\n", strerror(errno));
            return 1;
        }
    }
    struct stat info;
    if (lstat(destination, &info) == 0) {
        if (backup) {
            if (zov_ln_backup_destination(destination, suffix, backup_control) != 0)
                return zov_ln_report_error(destination, source, symbolic, errno);
        } else if (force) {
            if (unlink(destination) != 0 && errno != ENOENT)
                return zov_ln_report_error(destination, source, symbolic, errno);
        }
    }
    int result;
    char *relative_source = symbolic && relative ? zov_ln_relative_target(source, destination) : NULL;
    if (symbolic && relative && relative_source == NULL)
        return zov_ln_report_error(destination, source, symbolic, ENOMEM);
    if (symbolic) result = symlink(relative ? relative_source : source, destination);
    else result = linkat(AT_FDCWD, source, AT_FDCWD, destination,
                         logical ? AT_SYMLINK_FOLLOW : 0);
    if (result != 0) {
        int error = errno;
        free(relative_source);
        return zov_ln_report_error(destination, source, symbolic, error);
    }
    if (verbose) {
        zov_quote_shell(stdout, destination);
        fputs(symbolic ? " -> " : " => ", stdout);
        zov_quote_shell(stdout, symbolic && relative ? relative_source : source);
        fputc('\n', stdout);
    }
    free(relative_source);
    return 0;
}

static inline int zov_ln_run(int argc, int8_t **argv) {
    int symbolic = 0, logical = ZOV_LN_DEFAULT_LOGICAL, no_dereference = 0;
    int force = 0, interactive = 0, verbose = 0, no_target_dir = 0;
    int options_done = 0, backup = 0, backup_control = 0, relative = 0;
    const char *target_dir = NULL, *suffix = getenv("SIMPLE_BACKUP_SUFFIX");
    char **operands = (char **)malloc((size_t)(argc + 1) * sizeof(*operands));
    if (operands == NULL) return 1;
    int operand_count = 0;
    for (int i = 0; i < argc; ++i) {
        char *arg = (char *)argv[i];
        if (!options_done && strcmp(arg, "--") == 0) { options_done = 1; continue; }
        if (!options_done && strncmp(arg, "--", 2) == 0) {
            char *equal = strchr(arg, '=');
            size_t keylen = equal ? (size_t)(equal - arg) : strlen(arg);
            const char *value = equal ? equal + 1 : NULL;
            if (zov_ln_long(arg, "--help", keylen) || zov_ln_long(arg, "--version", keylen)) {
                int is_help = zov_ln_long(arg, "--help", keylen);
                if (value != NULL) { fprintf(stderr, "ln: option '%s' doesn't allow an argument\nTry 'ln --help' for more information.\n", is_help ? "--help" : "--version"); free(operands); return 1; }
                if (is_help) { zov_ln_help(); free(operands); return 0; }
                puts("ln (zovutils) 0.10.4"); free(operands); return 0;
            }
            if (zov_ln_long(arg, "--backup", keylen)) {
                backup = 1;
                if (value != NULL) {
                    backup_control = zov_ln_backup_control(value);
                    if (backup_control < 0) {
                        fprintf(stderr, "ln: invalid argument '%s' for 'backup type'\nValid arguments are:\n  - 'none', 'off'\n  - 'simple', 'never'\n  - 'existing', 'nil'\n  - 'numbered', 't'\nTry 'ln --help' for more information.\n", value);
                        free(operands); return 1;
                    }
                    if (backup_control == 4) backup = 0;
                }
                continue;
            }
            if (zov_ln_long(arg, "--force", keylen)) { force = 1; continue; }
            if (zov_ln_long(arg, "--interactive", keylen)) { interactive = 1; continue; }
            if (zov_ln_long(arg, "--logical", keylen)) { logical = 1; continue; }
            if (zov_ln_long(arg, "--no-dereference", keylen)) { no_dereference = 1; continue; }
            if (zov_ln_long(arg, "--physical", keylen)) { logical = 0; continue; }
            if (zov_ln_long(arg, "--relative", keylen)) { relative = 1; continue; }
            if (zov_ln_long(arg, "--symbolic", keylen)) { symbolic = 1; continue; }
            if (zov_ln_long(arg, "--directory", keylen)) { continue; }
            if (zov_ln_long(arg, "--no-target-directory", keylen)) { no_target_dir = 1; continue; }
            if (zov_ln_long(arg, "--verbose", keylen)) { verbose = 1; continue; }
            if (zov_ln_long(arg, "--target-directory", keylen) || zov_ln_long(arg, "--suffix", keylen)) {
                int is_target = zov_ln_long(arg, "--target-directory", keylen);
                const char *name = is_target ? "--target-directory" : "--suffix";
                if (value == NULL && i + 1 < argc) value = (char *)argv[++i];
                if (value == NULL) { fprintf(stderr, "ln: option '%s' requires an argument\nTry 'ln --help' for more information.\n", name); free(operands); return 1; }
                if (is_target) target_dir = value; else suffix = value;
                continue;
            }
            fprintf(stderr, "ln: unrecognized option '%s'\nTry 'ln --help' for more information.\n", arg); free(operands); return 1;
        }
        if (!options_done && arg[0] == '-' && arg[1] != '\0') {
            for (size_t j = 1; arg[j] != '\0'; ++j) {
                char option = arg[j];
                if (option == 's') symbolic = 1;
                else if (option == 'f') force = 1;
                else if (option == 'i') interactive = 1;
                else if (option == 'L') logical = 1;
                else if (option == 'P') logical = 0;
                else if (option == 'n') no_dereference = 1;
                else if (option == 'T') no_target_dir = 1;
                else if (option == 'v') verbose = 1;
                else if (option == 'b') backup = 1;
                else if (option == 'd' || option == 'F') { }
                else if (option == 'r') relative = 1;
                else if (option == 't' || option == 'S') {
                    const char *value = arg[j + 1] != '\0' ? arg + j + 1 : (i + 1 < argc ? (char *)argv[++i] : NULL);
                    if (value == NULL) { fprintf(stderr, "ln: option requires an argument -- '%c'\nTry 'ln --help' for more information.\n", option); free(operands); return 1; }
                    if (option == 't') target_dir = value; else suffix = value;
                    break;
                } else { fprintf(stderr, "ln: invalid option -- '%c'\nTry 'ln --help' for more information.\n", option); free(operands); return 1; }
            }
            continue;
        }
        operands[operand_count++] = arg;
        const char *posix = getenv("POSIXLY_CORRECT");
        if (posix != NULL && *posix != '\0') options_done = 1;
    }
    if (backup && backup_control == 0) {
        backup_control = zov_ln_backup_control(getenv("VERSION_CONTROL"));
        if (backup_control == 0) backup_control = 3;
        if (backup_control == 4) backup = 0;
        if (backup_control < 0) {
            const char *value = getenv("VERSION_CONTROL");
            fprintf(stderr, "ln: invalid argument '%s' for 'backup type'\nValid arguments are:\n  - 'none', 'off'\n  - 'simple', 'never'\n  - 'existing', 'nil'\n  - 'numbered', 't'\nTry 'ln --help' for more information.\n", value == NULL ? "" : value);
            free(operands); return 1;
        }
    }
    if (backup_control == 0) backup_control = 1;
    if (backup && suffix == NULL) suffix = "~";
    if (suffix == NULL) suffix = "~";
    if (operand_count == 0) { fputs("ln: missing file operand\nTry 'ln --help' for more information.\n", stderr); free(operands); return 1; }
    if (relative && !symbolic) { fputs("ln: cannot do --relative without --symbolic\n", stderr); free(operands); return 1; }
    if (symbolic && logical) logical = 0;
    int source_count = operand_count;
    const char *destination = target_dir;
    int destination_is_dir = target_dir != NULL;
    if (target_dir != NULL) {
        struct stat info;
        if (stat(target_dir, &info) != 0) {
            fputs("ln: failed to access ", stderr); zov_quote_shell(stderr, target_dir);
            fprintf(stderr, ": %s\n", strerror(errno)); free(operands); return 1;
        }
        if (!S_ISDIR(info.st_mode)) {
            fputs("ln: failed to access ", stderr); zov_quote_shell(stderr, target_dir);
            fprintf(stderr, ": %s\n", strerror(ENOTDIR)); free(operands); return 1;
        }
    } else if (operand_count == 1) {
        destination = ".";
        destination_is_dir = 1;
    } else {
        destination = operands[operand_count - 1];
        struct stat info;
        int exists_as_dir = (no_dereference ? lstat(destination, &info) : stat(destination, &info)) == 0 && S_ISDIR(info.st_mode);
        if (operand_count == 2) {
            destination_is_dir = !no_target_dir && exists_as_dir;
            source_count = 1;
        } else {
            if (no_target_dir) {
                fputs("ln: extra operand ", stderr); zov_quote_shell(stderr, destination);
                fputs("\nTry 'ln --help' for more information.\n", stderr); free(operands); return 1;
            }
            if (!exists_as_dir) {
                fputs("ln: target ", stderr); zov_quote_shell(stderr, destination);
                fprintf(stderr, ": %s\n", errno == ENOENT ? strerror(ENOENT) : strerror(ENOTDIR));
                free(operands); return 1;
            }
            destination_is_dir = 1;
            source_count = operand_count - 1;
        }
    }
    int status = 0;
    for (int i = 0; i < source_count; ++i) {
        char *link_name;
        if (destination_is_dir) {
            char *base = zov_ln_basename(operands[i]);
            link_name = base == NULL ? NULL : zov_ln_join(destination, base);
            free(base);
        } else link_name = strdup((char *)destination);
        if (link_name == NULL) { status = 1; continue; }
        struct stat existing;
        int exists = lstat(link_name, &existing) == 0;
        int operation_force = force;
        if (interactive && exists) {
            fprintf(stderr, "ln: replace "); zov_quote_shell(stderr, link_name); fputs("? ", stderr); fflush(stderr);
            char answer[16];
            if (fgets(answer, sizeof(answer), stdin) == NULL || (answer[0] != 'y' && answer[0] != 'Y')) { status = 1; free(link_name); continue; }
            operation_force = 1;
        }
        if (zov_ln_create_one(operands[i], link_name, symbolic, logical, operation_force,
                              no_dereference, relative, verbose, backup,
                              backup_control, suffix)) status = 1;
        free(link_name);
    }
    free(operands);
    if (fflush(stdout) != 0) status = 1;
    return status;
}

#endif
