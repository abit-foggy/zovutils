#ifndef ZOVUTILS_RM_BACKEND_H
#define ZOVUTILS_RM_BACKEND_H

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "platform_quote.h"

typedef struct {
    int recursive, force, interactive, verbose, directory_only;
    int one_file_system, preserve_root;
    dev_t root_device;
} zov_rm_options;

static inline int zov_rm_confirm(const char *path, const char *kind, int ask) {
    if (!ask) return 1;
    fprintf(stderr, "rm: %s ", kind); zov_quote_shell(stderr, path);
    fputs("? ", stderr); fflush(stderr);
    char answer[32];
    return fgets(answer, sizeof(answer), stdin) != NULL && (answer[0] == 'y' || answer[0] == 'Y');
}

static inline void zov_rm_error(const char *path, int error) {
    fputs("rm: cannot remove ", stderr); zov_quote_shell(stderr, path);
    fprintf(stderr, ": %s\n", strerror(error));
}

static inline void zov_rm_report(const char *path, int directory) {
    fputs("removed ", stdout);
    if (directory) fputs("directory ", stdout);
    zov_quote_shell(stdout, path); fputc('\n', stdout);
}

static inline int zov_rm_root_path(const char *path) {
    char *resolved = realpath(path, NULL);
    if (resolved == NULL) return 0;
    int root = strcmp(resolved, "/") == 0; free(resolved); return root;
}

static inline int zov_rm_one(const char *path, const zov_rm_options *opt,
                             int interactive, int root_call) {
    struct stat info;
    if (lstat(path, &info) != 0) {
        if (errno == ENOENT && opt->force) return 0;
        zov_rm_error(path, errno); return 1;
    }
    int is_directory = S_ISDIR(info.st_mode);
    if (root_call && opt->recursive && opt->preserve_root && is_directory && zov_rm_root_path(path)) {
        fputs("rm: it is dangerous to operate recursively on '/'\n", stderr);
        fputs("rm: use --no-preserve-root to override this failsafe\n", stderr); return 1;
    }
    if (!is_directory || !opt->recursive) {
        if (is_directory && !opt->recursive && !opt->directory_only) {
            fputs("rm: cannot remove ", stderr); zov_quote_shell(stderr, path);
            fputs(": Is a directory\n", stderr); return 1;
        }
        const char *kind = is_directory ? "remove directory"
            : S_ISLNK(info.st_mode) ? "remove symbolic link"
            : S_ISREG(info.st_mode) ? "remove regular file" : "remove special file";
        if (!zov_rm_confirm(path, kind, interactive)) return 0;
        int result = is_directory ? rmdir(path) : unlink(path);
        if (result != 0) { zov_rm_error(path, errno); return 1; }
        if (opt->verbose) zov_rm_report(path, is_directory);
        return 0;
    }
    if (opt->one_file_system && !root_call && info.st_dev != opt->root_device) return 0;
    if (!zov_rm_confirm(path, "descend into directory", interactive)) return 0;
    DIR *dir = opendir(path);
    if (dir == NULL) { zov_rm_error(path, errno); return 1; }
    int status = 0; struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        size_t a = strlen(path), b = strlen(entry->d_name);
        int slash = a > 0 && path[a - 1] != '/';
        char *child = (char *)malloc(a + b + (size_t)slash + 1);
        if (child == NULL) { status = 1; continue; }
        memcpy(child, path, a); if (slash) child[a++] = '/';
        memcpy(child + a, entry->d_name, b + 1);
        if (zov_rm_one(child, opt, interactive, 0) != 0) status = 1;
        free(child);
    }
    if (closedir(dir) != 0) status = 1;
    if (status != 0) return status;
    if (!zov_rm_confirm(path, "remove directory", interactive)) return 0;
    if (rmdir(path) != 0) { zov_rm_error(path, errno); return 1; }
    if (opt->verbose) zov_rm_report(path, 1);
    return 0;
}

static inline int zov_rm_long(const char *arg, const char *name, size_t len) {
    return len > 2 && len <= strlen(name) && strncmp(arg, name, len) == 0;
}

static inline void zov_rm_help(void);

static inline int zov_rm_run(int argc, int8_t **argv) {
    zov_rm_options opt = {0}; opt.preserve_root = 1;
    char **paths = (char **)malloc((size_t)(argc + 1) * sizeof(*paths));
    if (paths == NULL) return 1;
    int count = 0, done = 0, options_done = 0;
    for (int i = 0; i < argc; ++i) {
        const char *arg = (char *)argv[i];
        if (!done && strcmp(arg, "--") == 0) { done = 1; options_done = 1; continue; }
        if (!done && strncmp(arg, "--", 2) == 0) {
            const char *eq = strchr(arg, '='); size_t len = eq ? (size_t)(eq - arg) : strlen(arg);
            const char *value = eq ? eq + 1 : NULL;
            if (zov_rm_long(arg, "--help", len)) { if (value != NULL) { fputs("rm: option '--help' doesn't allow an argument\nTry 'rm --help' for more information.\n", stderr); free(paths); return 1; } zov_rm_help(); free(paths); return 0; }
            if (zov_rm_long(arg, "--version", len)) { puts("rm (zovutils) 0.10.24"); free(paths); return 0; }
            if (zov_rm_long(arg, "--recursive", len)) { opt.recursive = 1; continue; }
            if (zov_rm_long(arg, "--force", len)) { opt.force = 1; opt.interactive = 0; continue; }
            if (zov_rm_long(arg, "--interactive", len)) {
                if (value != NULL && (strcmp(value, "never") == 0 || strcmp(value, "no") == 0 || strcmp(value, "none") == 0)) opt.interactive = 0;
                else if (value != NULL && strcmp(value, "once") == 0) opt.interactive = 2;
                else if (value == NULL || strcmp(value, "always") == 0 || strcmp(value, "yes") == 0) opt.interactive = 1;
                else { fprintf(stderr, "rm: invalid argument '%s' for '--interactive'\nValid arguments are:\n  - 'never', 'no', 'none'\n  - 'once'\n  - 'always', 'yes'\nTry 'rm --help' for more information.\n", value); free(paths); return 1; }
                opt.force = 0; continue;
            }
            if (zov_rm_long(arg, "--verbose", len)) { opt.verbose = 1; continue; }
            if (zov_rm_long(arg, "--dir", len)) { opt.directory_only = 1; continue; }
            if (zov_rm_long(arg, "--one-file-system", len)) { opt.one_file_system = 1; continue; }
            if (zov_rm_long(arg, "--preserve-root", len)) { opt.preserve_root = 1; continue; }
            if (zov_rm_long(arg, "--no-preserve-root", len)) { opt.preserve_root = 0; continue; }
            fprintf(stderr, "rm: unrecognized option '%s'\nTry 'rm --help' for more information.\n", arg); free(paths); return 1;
        }
        if (!done && !options_done && arg[0] == '-' && arg[1] != '\0') {
            for (size_t j = 1; arg[j] != '\0'; ++j) {
                char c = arg[j];
                if (c == 'r' || c == 'R') opt.recursive = 1;
                else if (c == 'f') { opt.force = 1; opt.interactive = 0; }
                else if (c == 'i') { opt.interactive = 1; opt.force = 0; }
                else if (c == 'I') { opt.interactive = 2; opt.force = 0; }
                else if (c == 'v') opt.verbose = 1;
                else if (c == 'd') opt.directory_only = 1;
                else { fprintf(stderr, "rm: invalid option -- '%c'\nTry 'rm --help' for more information.\n", c); free(paths); return 1; }
            }
            continue;
        }
        paths[count++] = (char *)arg;
        if (getenv("POSIXLY_CORRECT") != NULL) options_done = 1;
    }
    if (count == 0) {
        if (opt.force) { free(paths); return 0; }
        fputs("rm: missing operand\nTry 'rm --help' for more information.\n", stderr); free(paths); return 1;
    }
    if (opt.interactive == 2 && (opt.recursive || count > 3)) {
        if (opt.recursive) fputs("rm: remove all arguments recursively? ", stderr);
        else fprintf(stderr, "rm: remove %d arguments? ", count);
        fflush(stderr);
        char answer[32];
        if (fgets(answer, sizeof(answer), stdin) == NULL || (answer[0] != 'y' && answer[0] != 'Y')) { free(paths); return 0; }
        opt.interactive = 0;
    }
    int status = 0;
    for (int i = 0; i < count; ++i) {
        struct stat info;
        if (lstat(paths[i], &info) == 0) opt.root_device = info.st_dev;
        if (zov_rm_one(paths[i], &opt, opt.interactive == 1, 1) != 0) status = 1;
    }
    free(paths); return status;
}

static inline void zov_rm_help(void) {
    fputs(
          "Usage: rm [OPTION]... [FILE]...\n"
          "Remove (unlink) the FILE(s).\n"
          "\n"
          "  -f, --force\n"
          "         ignore nonexistent files and arguments, never prompt\n"
          "  -i\n"
          "         prompt before every removal\n"
          "  -I\n"
          "         prompt once before removing more than three files,\n"
          "         or when removing recursively; less intrusive than -i,\n"
          "         while still giving protection against most mistakes\n"
          "      --interactive[=WHEN]\n"
          "         prompt according to WHEN: never, once (-I), or always (-i);\n"
          "         without WHEN, prompt always\n"
          "      --one-file-system\n"
          "         when removing a hierarchy recursively,\n"
          "         skip any directory that is on a file system different\n"
          "         from that of the corresponding command line argument\n"
          "      --no-preserve-root\n"
          "         do not treat '/' specially\n"
          "      --preserve-root[=all]\n"
          "         do not remove '/' (default);\n"
          "         with 'all', reject any command line argument\n"
          "         on a separate device from its parent\n"
          "  -r, -R, --recursive\n"
          "         remove directories and their contents recursively\n"
          "  -d, --dir\n"
          "         remove empty directories\n"
          "  -v, --verbose\n"
          "         explain what is being done\n"
          "      --help\n"
          "         display this help and exit\n"
          "      --version\n"
          "         output version information and exit\n"
          "\n"
          "By default, rm does not remove directories.  Use the --recursive (-r or -R)\n"
          "option to remove each listed directory, too, along with all of its contents.\n"
          "\n"
          "Any attempt to remove a file whose last file name component is '.' or '..'\n"
          "is rejected with a diagnostic.\n"
          "\n"
          "To remove a file whose name starts with a '-', for example '-foo',\n"
          "use one of these commands:\n"
          "  rm -- -foo\n"
          "\n"
          "  rm ./-foo\n"
          "\n"
          "If you use rm to remove a file, it might be possible to recover\n"
          "some of its contents, given sufficient expertise and/or time.  For greater\n"
          "assurance that the contents are unrecoverable, consider using shred(1).\n"
          "\n"
          "Report bugs to: bug-coreutils@gnu.org\n"
          "GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n"
          "General help using GNU software: <https://www.gnu.org/gethelp/>\n"
          "Report any translation bugs to <https://translationproject.org/team/>\n"
          "Full documentation <https://www.gnu.org/software/coreutils/rm>\n"
          "or available locally via: info '(coreutils) rm invocation'\n"
, stdout);
}

#endif
