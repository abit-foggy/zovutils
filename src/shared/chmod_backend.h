#ifndef ZOVUTILS_CHMOD_BACKEND_H
#define ZOVUTILS_CHMOD_BACKEND_H

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
    int recursive, force, verbose, changes_only, dereference;
    int preserve_root, traverse_mode;
} zov_chmod_options;

static inline int zov_chmod_parse_mode(const char *text, mode_t current,
                                       mode_t mask, int is_directory,
                                       mode_t *result) {
    if (text == NULL || *text == '\0') return 0;
    int octal = 1;
    for (const char *q = text; *q; ++q) if (*q < '0' || *q > '7') { octal = 0; break; }
    if (octal) {
        mode_t value = 0;
        for (const char *q = text; *q; ++q) {
            unsigned d = (unsigned)(*q - '0');
            if (value > (07777u - d) / 8u) return 0;
            value = (value << 3) | d;
        }
        if (value > 07777u) return 0;
        *result = value; return 1;
    }

    const char *p = text;
    mode_t mode = current & 07777;
    int clauses = 0;
    while (*p != '\0') {
        unsigned who = 0; int explicit_who = 0;
        while (*p == 'u' || *p == 'g' || *p == 'o' || *p == 'a') {
            explicit_who = 1;
            if (*p == 'u' || *p == 'a') who |= 1;
            if (*p == 'g' || *p == 'a') who |= 2;
            if (*p == 'o' || *p == 'a') who |= 4;
            ++p;
        }
        if (!explicit_who) who = 7;
        if (*p != '+' && *p != '-' && *p != '=') return 0;
        char op = *p++;
        mode_t affected = 0;
        if (who & 1) affected |= S_IRWXU | S_ISUID;
        if (who & 2) affected |= S_IRWXG | S_ISGID;
        if (who & 4) affected |= S_IRWXO | S_ISVTX;
        mode_t bits = 0; int any = 0;
        while (*p != '\0' && *p != ',') {
            char c = *p++; any = 1;
            if (c == 'r' || c == 'w' || c == 'x' || c == 'X') {
                int enabled = c != 'X' || is_directory || (mode & 0111) != 0;
                if (!enabled) continue;
                int bit = c == 'r' ? 4 : c == 'w' ? 2 : 1;
                if (who & 1) bits |= bit == 4 ? S_IRUSR : bit == 2 ? S_IWUSR : S_IXUSR;
                if (who & 2) bits |= bit == 4 ? S_IRGRP : bit == 2 ? S_IWGRP : S_IXGRP;
                if (who & 4) bits |= bit == 4 ? S_IROTH : bit == 2 ? S_IWOTH : S_IXOTH;
            } else if (c == 's') {
                if (who & 1) bits |= S_ISUID;
                if (who & 2) bits |= S_ISGID;
            } else if (c == 't') {
                if (who & 4) bits |= S_ISVTX;
            } else if (c == 'u' || c == 'g' || c == 'o') {
                mode_t copied = c == 'u' ? (mode & S_IRWXU) >> 6
                              : c == 'g' ? (mode & S_IRWXG) >> 3
                                         : (mode & S_IRWXO);
                if (who & 1) bits |= copied << 6;
                if (who & 2) bits |= copied << 3;
                if (who & 4) bits |= copied;
            } else return 0;
        }
        if (!any && op != '=') return 0;
        if (!explicit_who) {
            mode_t restricted = affected & ~(mask & 0777);
            affected &= ~(mask & 0777);
            bits &= ~restricted;
        }
        if (op == '=') mode = (mode & ~affected) | bits;
        else if (op == '+') mode |= bits;
        else mode &= ~bits;
        clauses++;
        if (*p == ',') { ++p; if (*p == '\0') return 0; }
    }
    if (!clauses) return 0;
    *result = mode & 07777; return 1;
}

static inline void zov_chmod_error(const char *path, int error) {
    fputs("chmod: changing permissions of ", stderr); zov_quote_shell(stderr, path);
    fprintf(stderr, ": %s\n", strerror(error));
}

static inline void zov_chmod_access_error(const char *path, int error) {
    fputs("chmod: cannot access ", stderr); zov_quote_shell(stderr, path);
    fprintf(stderr, ": %s\n", strerror(error));
}

static inline void zov_chmod_mode_text(mode_t mode, char text[10]) {
    text[0] = (mode & S_IRUSR) ? 'r' : '-';
    text[1] = (mode & S_IWUSR) ? 'w' : '-';
    text[2] = (mode & S_ISUID) ? ((mode & S_IXUSR) ? 's' : 'S') : ((mode & S_IXUSR) ? 'x' : '-');
    text[3] = (mode & S_IRGRP) ? 'r' : '-';
    text[4] = (mode & S_IWGRP) ? 'w' : '-';
    text[5] = (mode & S_ISGID) ? ((mode & S_IXGRP) ? 's' : 'S') : ((mode & S_IXGRP) ? 'x' : '-');
    text[6] = (mode & S_IROTH) ? 'r' : '-';
    text[7] = (mode & S_IWOTH) ? 'w' : '-';
    text[8] = (mode & S_ISVTX) ? ((mode & S_IXOTH) ? 't' : 'T') : ((mode & S_IXOTH) ? 'x' : '-');
    text[9] = '\0';
}

static inline int zov_chmod_apply(const char *mode_text, const char *path,
                                  const zov_chmod_options *opt, int is_root) {
    struct stat info;
    if (stat(path, &info) != 0) {
        if (!opt->force) zov_chmod_access_error(path, errno);
        return opt->force ? 0 : 1;
    }
    if (is_root && opt->recursive && opt->preserve_root && S_ISDIR(info.st_mode)) {
        char *resolved = realpath(path, NULL);
        int root = resolved != NULL && strcmp(resolved, "/") == 0;
        free(resolved);
        if (root) {
            fputs("chmod: it is dangerous to operate recursively on '/'\n", stderr);
            fputs("chmod: use --no-preserve-root to override this failsafe\n", stderr); return 1;
        }
    }
    mode_t old_mask = umask(0); umask(old_mask); mode_t desired;
    if (!zov_chmod_parse_mode(mode_text, info.st_mode, old_mask, S_ISDIR(info.st_mode), &desired)) {
        fputs("chmod: invalid mode: ", stderr); zov_quote_shell(stderr, mode_text);
        fputs("\nTry 'chmod --help' for more information.\n", stderr); return 1;
    }
    if ((info.st_mode & 07777) != desired && chmod(path, desired) != 0) {
        if (!opt->force) zov_chmod_error(path, errno);
        return 1;
    }
    int changed = (info.st_mode & 07777) != desired;
    if (opt->verbose && (!opt->changes_only || changed)) {
        char before[10], after[10]; zov_chmod_mode_text(info.st_mode & 07777, before);
        zov_chmod_mode_text(desired, after);
        fputs("mode of ", stdout); zov_quote_shell(stdout, path);
        if (changed) fprintf(stdout, " changed from %04o (%s) to %04o (%s)\n", (unsigned)(info.st_mode & 07777), before, (unsigned)desired, after);
        else fprintf(stdout, " retained as %04o (%s)\n", (unsigned)desired, after);
    }
    return 0;
}

static inline int zov_chmod_apply_link(const char *mode_text, const char *path,
                                       const struct stat *link_info,
                                       const zov_chmod_options *opt) {
#if defined(__APPLE__)
    mode_t old_mask = umask(0); umask(old_mask); mode_t desired;
    if (!zov_chmod_parse_mode(mode_text, link_info->st_mode, old_mask, 0, &desired)) return 1;
    int changed = (link_info->st_mode & 07777) != desired;
    if (changed && lchmod(path, desired) != 0) {
        if (!opt->force) zov_chmod_error(path, errno);
        return 1;
    }
    if (opt->verbose && (!opt->changes_only || changed)) {
        char before[10], after[10]; zov_chmod_mode_text(link_info->st_mode & 07777, before);
        zov_chmod_mode_text(desired, after);
        fputs("mode of ", stdout); zov_quote_shell(stdout, path);
        if (changed) fprintf(stdout, " changed from %04o (%s) to %04o (%s)\n", (unsigned)(link_info->st_mode & 07777), before, (unsigned)desired, after);
        else fprintf(stdout, " retained as %04o (%s)\n", (unsigned)desired, after);
    }
#else
    (void)mode_text; (void)path; (void)link_info; (void)opt;
#endif
    return 0;
}

static inline int zov_chmod_path(const char *mode, const char *path,
                                 const zov_chmod_options *opt, int is_root,
                                 int command_line) {
    struct stat link_info, info;
    if (lstat(path, &link_info) != 0) return zov_chmod_apply(mode, path, opt, is_root);
    if (S_ISLNK(link_info.st_mode)) {
        if (!opt->dereference || (opt->recursive && !command_line && opt->traverse_mode != 1)
            || (opt->recursive && command_line && opt->traverse_mode == 2))
            return zov_chmod_apply_link(mode, path, &link_info, opt);
    }
    if (stat(path, &info) != 0) return zov_chmod_apply(mode, path, opt, is_root);
    if (!S_ISDIR(info.st_mode) || !opt->recursive) return zov_chmod_apply(mode, path, opt, is_root);
    if (is_root && opt->preserve_root) {
        char *resolved = realpath(path, NULL);
        int root = resolved != NULL && strcmp(resolved, "/") == 0;
        free(resolved);
        if (root) {
            fputs("chmod: it is dangerous to operate recursively on '/'\n", stderr);
            fputs("chmod: use --no-preserve-root to override this failsafe\n", stderr); return 1;
        }
    }
    int status = 0; DIR *dir = opendir(path);
    if (dir == NULL) { if (!opt->force) zov_chmod_access_error(path, errno); return opt->force ? 0 : 1; }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        size_t a = strlen(path), b = strlen(entry->d_name); int slash = a > 0 && path[a - 1] != '/';
        char *child = (char *)malloc(a + b + (size_t)slash + 1);
        if (child == NULL) { status = 1; continue; }
        memcpy(child, path, a); if (slash) child[a++] = '/'; memcpy(child + a, entry->d_name, b + 1);
        struct stat child_info;
        if (lstat(child, &child_info) != 0) status = 1;
        else if (zov_chmod_path(mode, child, opt, 0, 0) != 0) status = 1;
        free(child);
    }
    if (closedir(dir) != 0) status = 1;
    if (zov_chmod_apply(mode, path, opt, is_root) != 0) status = 1;
    return status;
}

static inline int zov_chmod_long(const char *arg, const char *name, size_t len) {
    return len > 2 && len <= strlen(name) && strncmp(arg, name, len) == 0;
}

static inline void zov_chmod_help(void);

static inline int zov_chmod_run(int argc, int8_t **argv) {
    zov_chmod_options opt = {0}; opt.dereference = 1; opt.traverse_mode = 0;
    const char *reference_mode = NULL;
    char **operands = (char **)malloc((size_t)(argc + 1) * sizeof(*operands));
    if (operands == NULL) return 1;
    int count = 0, done = 0;
    for (int i = 0; i < argc; ++i) {
        const char *arg = (char *)argv[i];
        if (!done && strcmp(arg, "--") == 0) { done = 1; continue; }
        if (!done && strncmp(arg, "--", 2) == 0) {
            const char *eq = strchr(arg, '='); size_t len = eq ? (size_t)(eq - arg) : strlen(arg);
            const char *value = eq ? eq + 1 : NULL;
            if (zov_chmod_long(arg, "--help", len)) { if (value != NULL) { fputs("chmod: option '--help' doesn't allow an argument\nTry 'chmod --help' for more information.\n", stderr); free(operands); return 1; } zov_chmod_help(); free(operands); return 0; }
            if (zov_chmod_long(arg, "--version", len)) { puts("chmod (zovutils) 0.10.18"); free(operands); return 0; }
            if (zov_chmod_long(arg, "--recursive", len)) { opt.recursive = 1; continue; }
            if (zov_chmod_long(arg, "--changes", len)) { opt.verbose = opt.changes_only = 1; continue; }
            if (zov_chmod_long(arg, "--verbose", len)) { opt.verbose = 1; continue; }
            if (zov_chmod_long(arg, "--quiet", len) || zov_chmod_long(arg, "--silent", len)) { opt.force = 1; continue; }
            if (zov_chmod_long(arg, "--preserve-root", len)) { opt.preserve_root = 1; continue; }
            if (zov_chmod_long(arg, "--no-preserve-root", len)) { opt.preserve_root = 0; continue; }
            if (zov_chmod_long(arg, "--dereference", len)) { opt.dereference = 1; continue; }
            if (zov_chmod_long(arg, "--no-dereference", len)) { opt.dereference = 0; continue; }
            if (zov_chmod_long(arg, "--reference", len)) {
                if (value == NULL && i + 1 < argc) value = (char *)argv[++i];
                if (value == NULL) { fputs("chmod: option '--reference' requires an argument\nTry 'chmod --help' for more information.\n", stderr); free(operands); return 1; }
                struct stat ref; if (stat(value, &ref) != 0) { zov_chmod_access_error(value, errno); free(operands); return 1; }
                static char mode_buf[8]; snprintf(mode_buf, sizeof(mode_buf), "%o", ref.st_mode & 07777);
                reference_mode = mode_buf; continue;
            }
            fprintf(stderr, "chmod: unrecognized option '%s'\nTry 'chmod --help' for more information.\n", arg); free(operands); return 1;
        }
        if (!done && arg[0] == '-' && arg[1] != '\0') {
            for (size_t j = 1; arg[j] != '\0'; ++j) {
                char c = arg[j];
                if (c == 'R') opt.recursive = 1;
                else if (c == 'f') opt.force = 1;
                else if (c == 'v') opt.verbose = 1;
                else if (c == 'c') opt.verbose = opt.changes_only = 1;
                else if (c == 'h') opt.dereference = 0;
                else if (c == 'H') opt.traverse_mode = 0;
                else if (c == 'L') opt.traverse_mode = 1;
                else if (c == 'P') opt.traverse_mode = 2;
                else { fprintf(stderr, "chmod: invalid option -- '%c'\nTry 'chmod --help' for more information.\n", c); free(operands); return 1; }
            }
            continue;
        }
        operands[count++] = (char *)arg;
    }
    if (count == 0) { fputs("chmod: missing operand\nTry 'chmod --help' for more information.\n", stderr); free(operands); return 1; }
    int first_file = reference_mode != NULL ? 0 : 1;
    if (count <= first_file) {
        if (reference_mode != NULL) fputs("chmod: missing operand\nTry 'chmod --help' for more information.\n", stderr);
        else { fputs("chmod: missing operand after ", stderr); zov_quote_shell(stderr, operands[0]); fputs("\nTry 'chmod --help' for more information.\n", stderr); }
        free(operands); return 1;
    }
    const char *mode = reference_mode != NULL ? reference_mode : operands[0]; int status = 0;
    for (int i = first_file; i < count; ++i)
        if (zov_chmod_path(mode, operands[i], &opt, 1, 1) != 0) status = 1;
    free(operands); return status;
}

static inline void zov_chmod_help(void) {
    fputs(
          "Usage: chmod [OPTION]... MODE[,MODE]... FILE...\n"
          "  or:  chmod [OPTION]... OCTAL-MODE FILE...\n"
          "  or:  chmod [OPTION]... --reference=RFILE FILE...\n"
          "Change the mode of each FILE to MODE.\n"
          "With --reference, change the mode of each FILE to that of RFILE.\n"
          "\n"
          "  -c, --changes\n"
          "         like verbose but report only when a change is made\n"
          "  -f, --silent, --quiet\n"
          "         suppress most error messages\n"
          "  -v, --verbose\n"
          "         output a diagnostic for every file processed\n"
          "      --dereference\n"
          "         affect the referent of each symbolic link,\n"
          "         rather than the symbolic link itself\n"
          "  -h, --no-dereference\n"
          "         affect each symbolic link, rather than the referent\n"
          "      --no-preserve-root\n"
          "         do not treat '/' specially (the default)\n"
          "      --preserve-root\n"
          "         fail to operate recursively on '/'\n"
          "      --reference=RFILE\n"
          "         use RFILE's mode instead of specifying MODE values.\n"
          "         RFILE is always dereferenced if a symbolic link.\n"
          "  -R, --recursive\n"
          "         change files and directories recursively\n"
          "\n"
          "The following options modify how a hierarchy is traversed when the -R\n"
          "option is also specified.  If more than one is specified, only the final\n"
          "one takes effect. -H is the default.\n"
          "\n"
          "  -H\n"
          "         if a command line argument is a symlink to a directory, traverse it\n"
          "  -L\n"
          "         traverse every symbolic link to a directory encountered\n"
          "  -P\n"
          "         do not traverse any symbolic links\n"
          "\n"
          "      --help\n"
          "         display this help and exit\n"
          "      --version\n"
          "         output version information and exit\n"
          "\n"
          "Each MODE is of the form '[ugoa]*([-+=]([rwxXst]*|[ugo]))+|[-+=][0-7]+'.\n"
          "\n"
          "Report bugs to: bug-coreutils@gnu.org\n"
          "GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n"
          "General help using GNU software: <https://www.gnu.org/gethelp/>\n"
          "Report any translation bugs to <https://translationproject.org/team/>\n"
          "Full documentation <https://www.gnu.org/software/coreutils/chmod>\n"
          "or available locally via: info '(coreutils) chmod invocation'\n"
, stdout);
}

#endif
