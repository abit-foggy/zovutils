#ifndef ZOVUTILS_MV_BACKEND_H
#define ZOVUTILS_MV_BACKEND_H

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "cp_backend.h"

typedef struct { int force, interactive, no_clobber, verbose, update, no_target_dir; } zov_mv_options;

static inline int zov_mv_remove_tree(const char *path) {
    struct stat info;
    if (lstat(path, &info) != 0) return -1;
    if (!S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode)) return unlink(path);
    DIR *dir = opendir(path);
    if (dir == NULL) return -1;
    int status = 0; struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char *child = zov_cp_join(path, entry->d_name);
        if (child == NULL || zov_mv_remove_tree(child) != 0) status = -1;
        free(child);
    }
    if (closedir(dir) != 0) status = -1;
    if (status == 0) status = rmdir(path);
    return status;
}

static inline void zov_mv_error(const char *source, const char *dest, int error) {
    fputs("mv: cannot move ", stderr); zov_quote_shell(stderr, source);
    fputs(" to ", stderr); zov_quote_shell(stderr, dest);
    fprintf(stderr, ": %s\n", strerror(error));
}

static inline int zov_mv_one(const char *source, const char *dest, const zov_mv_options *opt) {
    struct stat src_info, dest_link_info, dest_info;
    if (lstat(source, &src_info) != 0) {
        fputs("mv: cannot stat ", stderr); zov_quote_shell(stderr, source);
        fprintf(stderr, ": %s\n", strerror(errno)); return 1;
    }
    int dest_exists = lstat(dest, &dest_link_info) == 0;
    int dest_stat = stat(dest, &dest_info) == 0;
    if (dest_exists && S_ISDIR(dest_link_info.st_mode) && !S_ISDIR(src_info.st_mode)) {
        fputs("mv: cannot overwrite directory ", stderr); zov_quote_shell(stderr, dest);
        fputs(" with non-directory ", stderr); zov_quote_shell(stderr, source); fputc('\n', stderr); return 1;
    }
    if (dest_exists && !S_ISDIR(dest_link_info.st_mode) && S_ISDIR(src_info.st_mode)) {
        fputs("mv: cannot overwrite non-directory ", stderr); zov_quote_shell(stderr, dest);
        fputs(" with directory ", stderr); zov_quote_shell(stderr, source); fputc('\n', stderr); return 1;
    }
    if (dest_stat && src_info.st_dev == dest_info.st_dev && src_info.st_ino == dest_info.st_ino) {
        fputs("mv: ", stderr); zov_quote_shell(stderr, source); fputs(" and ", stderr);
        zov_quote_shell(stderr, dest); fputs(" are the same file\n", stderr); return 1;
    }
    if (dest_exists && opt->update && dest_stat && src_info.st_mtime <= dest_info.st_mtime) return 0;
    if (dest_exists && opt->no_clobber) return 0;
    if (dest_exists && opt->interactive) {
        fputs("mv: overwrite ", stderr); zov_quote_shell(stderr, dest); fputs("? ", stderr); fflush(stderr);
        char answer[32];
        if (fgets(answer, sizeof(answer), stdin) == NULL || (answer[0] != 'y' && answer[0] != 'Y')) return 1;
    }
    if (rename(source, dest) == 0) {
        if (opt->verbose) { fputs("renamed ", stdout); zov_quote_shell(stdout, source); fputs(" -> ", stdout); zov_quote_shell(stdout, dest); fputc('\n', stdout); }
        return 0;
    }
    int error = errno;
    if (error == EXDEV) {
        zov_cp_options cp = {0}; cp.recursive = 1; cp.preserve = 1; cp.dereference = 0;
        if (zov_cp_copy_one(source, dest, &cp, 1) == 0 && zov_mv_remove_tree(source) == 0) {
            if (opt->verbose) { fputs("copied ", stdout); zov_quote_shell(stdout, source); fputs(" -> ", stdout); zov_quote_shell(stdout, dest); fputc('\n', stdout); }
            return 0;
        }
        error = errno;
    }
    zov_mv_error(source, dest, error); return 1;
}

static inline void zov_mv_help(void) {
    fputs(
          "Usage: mv [OPTION]... [-T] SOURCE DEST\n"
          "  or:  mv [OPTION]... SOURCE... DIRECTORY\n"
          "  or:  mv [OPTION]... -t DIRECTORY SOURCE...\n"
          "Rename SOURCE to DEST, or move SOURCE(s) to DIRECTORY.\n"
          "\n"
          "Mandatory arguments to long options are mandatory for short options too.\n"
          "      --backup[=CONTROL]\n"
          "         make a backup of each existing destination file\n"
          "  -b\n"
          "         like --backup but does not accept an argument\n"
          "      --debug\n"
          "         explain how a file is copied.  Implies -v\n"
          "      --exchange\n"
          "         exchange source and destination\n"
          "  -f, --force\n"
          "         do not prompt before overwriting\n"
          "  -i, --interactive\n"
          "         prompt before overwrite\n"
          "  -n, --no-clobber\n"
          "         do not overwrite an existing file\n"
          "If you specify more than one of -i, -f, -n, only the final one takes effect.\n"
          "      --no-copy\n"
          "         do not copy if renaming fails\n"
          "      --strip-trailing-slashes\n"
          "         remove any trailing slashes from each SOURCE argument\n"
          "  -S, --suffix=SUFFIX\n"
          "         override the usual backup suffix\n"
          "  -t, --target-directory=DIRECTORY\n"
          "         move all SOURCE arguments into DIRECTORY\n"
          "  -T, --no-target-directory\n"
          "         treat DEST as a normal file\n"
          "      --update[=UPDATE]\n"
          "         control which existing files are updated;\n"
          "         UPDATE={all,none,none-fail,older(default)}\n"
          "  -u\n"
          "         equivalent to --update[=older].  See below\n"
          "  -v, --verbose\n"
          "         explain what is being done\n"
          "  -Z, --context\n"
          "         set SELinux security context of destination file to default type\n"
          "      --help\n"
          "         display this help and exit\n"
          "      --version\n"
          "         output version information and exit\n"
          "\n"
          "UPDATE controls which existing files in the destination are replaced.\n"
          "'all' is the default operation when an --update option is not specified,\n"
          "and results in all existing files in the destination being replaced.\n"
          "'none' is like the --no-clobber option, in that no files in the\n"
          "destination are replaced, and skipped files do not induce a failure.\n"
          "'none-fail' also ensures no files are replaced in the destination,\n"
          "but any skipped files are diagnosed and induce a failure.\n"
          "'older' is the default operation when --update is specified, and results\n"
          "in files being replaced if they're older than the corresponding source file.\n"
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
          "Report bugs to: bug-coreutils@gnu.org\n"
          "GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n"
          "General help using GNU software: <https://www.gnu.org/gethelp/>\n"
          "Report any translation bugs to <https://translationproject.org/team/>\n"
          "Full documentation <https://www.gnu.org/software/coreutils/mv>\n"
          "or available locally via: info '(coreutils) mv invocation'\n"
, stdout);
}

static inline int zov_mv_long(const char *arg, const char *name, size_t len) {
    return len > 2 && len <= strlen(name) && strncmp(arg, name, len) == 0;
}

static inline int zov_mv_run(int argc, int8_t **argv) {
    zov_mv_options opt = {0}; const char *target_dir = NULL; int done = 0;
    char **operands = (char **)malloc((size_t)(argc + 1) * sizeof(*operands));
    if (operands == NULL) return 1;
    int count = 0;
    for (int i = 0; i < argc; ++i) {
        const char *arg = (char *)argv[i];
        if (!done && strcmp(arg, "--") == 0) { done = 1; continue; }
        if (!done && strncmp(arg, "--", 2) == 0) {
            const char *eq = strchr(arg, '='); size_t len = eq ? (size_t)(eq - arg) : strlen(arg);
            const char *value = eq ? eq + 1 : NULL;
            if (zov_mv_long(arg, "--help", len)) { if (value != NULL) { fputs("mv: option '--help' doesn't allow an argument\nTry 'mv --help' for more information.\n", stderr); free(operands); return 1; } zov_mv_help(); free(operands); return 0; }
            if (zov_mv_long(arg, "--version", len)) { puts("mv (zovutils) 0.10.58"); free(operands); return 0; }
            if (zov_mv_long(arg, "--force", len)) { opt.force = 1; opt.interactive = 0; continue; }
            if (zov_mv_long(arg, "--interactive", len)) { opt.interactive = 1; opt.force = 0; opt.no_clobber = 0; continue; }
            if (zov_mv_long(arg, "--no-clobber", len)) { opt.no_clobber = 1; opt.interactive = 0; continue; }
            if (zov_mv_long(arg, "--verbose", len)) { opt.verbose = 1; continue; }
            if (zov_mv_long(arg, "--update", len)) { opt.update = 1; continue; }
            if (zov_mv_long(arg, "--no-target-directory", len)) { opt.no_target_dir = 1; continue; }
            if (zov_mv_long(arg, "--target-directory", len)) {
                if (value == NULL && i + 1 < argc) value = (char *)argv[++i];
                if (value == NULL) { fputs("mv: option '--target-directory' requires an argument\nTry 'mv --help' for more information.\n", stderr); free(operands); return 1; }
                target_dir = value; continue;
            }
            fprintf(stderr, "mv: unrecognized option '%s'\nTry 'mv --help' for more information.\n", arg); free(operands); return 1;
        }
        if (!done && arg[0] == '-' && arg[1] != '\0') {
            for (size_t j = 1; arg[j] != '\0'; ++j) {
                char c = arg[j];
                if (c == 'f') { opt.force = 1; opt.interactive = 0; }
                else if (c == 'i') { opt.interactive = 1; opt.force = 0; opt.no_clobber = 0; }
                else if (c == 'n') { opt.no_clobber = 1; opt.interactive = 0; }
                else if (c == 'v') opt.verbose = 1;
                else if (c == 'u') opt.update = 1;
                else if (c == 'T') opt.no_target_dir = 1;
                else if (c == 't') {
                    const char *v = arg[j + 1] ? arg + j + 1 : (i + 1 < argc ? (char *)argv[++i] : NULL);
                    if (v == NULL) { fputs("mv: option requires an argument -- 't'\nTry 'mv --help' for more information.\n", stderr); free(operands); return 1; }
                    target_dir = v; break;
                } else { fprintf(stderr, "mv: invalid option -- '%c'\nTry 'mv --help' for more information.\n", c); free(operands); return 1; }
            }
            continue;
        }
        operands[count++] = (char *)arg;
    }
    if (count == 0) { fputs("mv: missing file operand\nTry 'mv --help' for more information.\n", stderr); free(operands); return 1; }
    if (target_dir != NULL) {
        struct stat info;
        if (stat(target_dir, &info) != 0 || !S_ISDIR(info.st_mode)) {
            fputs("mv: target ", stderr); zov_quote_shell(stderr, target_dir); fputs(" is not a directory\n", stderr); free(operands); return 1;
        }
        int status = 0;
        for (int i = 0; i < count; ++i) {
            char *base = zov_cp_base(operands[i]); char *dest = base ? zov_cp_join(target_dir, base) : NULL; free(base);
            if (dest == NULL || zov_mv_one(operands[i], dest, &opt) != 0) status = 1;
            free(dest);
        }
        free(operands); return status;
    }
    if (count == 1) { fputs("mv: missing destination file operand after ", stderr); zov_quote_shell(stderr, operands[0]); fputs("\nTry 'mv --help' for more information.\n", stderr); free(operands); return 1; }
    const char *target = operands[count - 1]; struct stat info;
    int directory = !opt.no_target_dir && stat(target, &info) == 0 && S_ISDIR(info.st_mode);
    if (count > 2 && !directory) {
        int e = stat(target, &info) == 0 ? ENOTDIR : errno;
        fputs("mv: target ", stderr); zov_quote_shell(stderr, target); fprintf(stderr, ": %s\n", strerror(e)); free(operands); return 1;
    }
    int status = 0;
    for (int i = 0; i < count - 1; ++i) {
        char *base = directory ? zov_cp_base(operands[i]) : NULL;
        char *dest = directory ? (base ? zov_cp_join(target, base) : NULL) : strdup((char *)target);
        free(base);
        if (dest == NULL || zov_mv_one(operands[i], dest, &opt) != 0) status = 1;
        free(dest);
    }
    free(operands); return status;
}

#endif
