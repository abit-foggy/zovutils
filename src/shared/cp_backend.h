#ifndef ZOVUTILS_CP_BACKEND_H
#define ZOVUTILS_CP_BACKEND_H

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "platform_quote.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    int recursive, force, interactive, no_clobber, verbose, preserve, remove_destination;
    int dereference, symlink, hardlink, update, parents, no_target_dir;
    int backup, backup_control;
    const char *suffix;
    const char *target_dir;
} zov_cp_options;

static inline char *zov_cp_base(const char *path) {
    size_t n = strlen(path);
    while (n > 1 && path[n - 1] == '/') --n;
    size_t start = n;
    while (start > 0 && path[start - 1] != '/') --start;
    if (start == n && n == 0) return strdup(".");
    char *out = (char *)malloc(n - start + 1);
    if (out == NULL) return NULL;
    memcpy(out, path + start, n - start); out[n - start] = '\0';
    return out;
}

static inline char *zov_cp_join(const char *dir, const char *name) {
    size_t a = strlen(dir), b = strlen(name);
    int slash = a != 0 && dir[a - 1] != '/';
    if (a > SIZE_MAX - b - (size_t)slash - 1) return NULL;
    char *out = (char *)malloc(a + b + (size_t)slash + 1);
    if (out == NULL) return NULL;
    memcpy(out, dir, a); if (slash) out[a++] = '/';
    memcpy(out + a, name, b + 1); return out;
}

static inline int zov_cp_make_parents(const char *path) {
    char *copy = strdup(path);
    if (copy == NULL) return -1;
    size_t n = strlen(copy);
    for (size_t i = 1; i < n; ++i) {
        if (copy[i] != '/') continue;
        copy[i] = '\0';
        if (*copy != '\0' && strcmp(copy, ".") != 0 && strcmp(copy, "..") != 0) {
            if (mkdir(copy, 0777) != 0) {
                int e = errno; struct stat st;
                if (e != EEXIST || stat(copy, &st) != 0 || !S_ISDIR(st.st_mode)) {
                    free(copy); errno = e; return -1;
                }
            }
        }
        copy[i] = '/';
    }
    free(copy); return 0;
}

static inline char *zov_cp_destination(const char *source, const char *directory,
                                        int parents) {
    if (!parents) {
        char *base = zov_cp_base(source);
        char *result = base == NULL ? NULL : zov_cp_join(directory, base);
        free(base); return result;
    }
    while (*source == '/') ++source;
    char *result = zov_cp_join(directory, source);
    if (result != NULL && zov_cp_make_parents(result) != 0) { free(result); return NULL; }
    return result;
}

static inline int zov_cp_same(const struct stat *a, const struct stat *b) {
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino;
}

static inline int zov_cp_destination_inside(const char *source, const char *dest) {
    char *source_real = realpath(source, NULL);
    if (source_real == NULL) return 0;
    char *copy = strdup(dest);
    if (copy == NULL) { free(source_real); return 0; }
    char *slash = strrchr(copy, '/');
    if (slash == NULL) strcpy(copy, ".");
    else if (slash == copy) slash[1] = '\0';
    else *slash = '\0';
    char *parent_real = realpath(copy, NULL);
    free(copy);
    if (parent_real == NULL) { free(source_real); return 0; }
    size_t n = strlen(source_real);
    int inside = strncmp(parent_real, source_real, n) == 0
        && (parent_real[n] == '/' || parent_real[n] == '\0');
    free(source_real); free(parent_real); return inside;
}

static inline int zov_cp_path_contains(const char *parent, const char *child) {
    char *a = realpath(parent, NULL), *b = realpath(child, NULL);
    if (a == NULL || b == NULL) { free(a); free(b); return 0; }
    size_t n = strlen(a);
    int result = strncmp(b, a, n) == 0 && (b[n] == '/' || b[n] == '\0');
    free(a); free(b); return result;
}

static inline void zov_cp_error(const char *action, const char *path, int error) {
    fprintf(stderr, "cp: %s ", action); zov_quote_shell(stderr, path);
    fprintf(stderr, ": %s\n", strerror(error));
}

static inline int zov_cp_backup_existing(const char *dest, const zov_cp_options *opt) {
    if (!opt->backup) return 0;
    struct stat info;
    if (lstat(dest, &info) != 0) return 0;
    int numbered = opt->backup_control == 1;
    if (opt->backup_control == 0) {
        const char *control = getenv("VERSION_CONTROL");
        numbered = control != NULL && (strcmp(control, "numbered") == 0 || strcmp(control, "t") == 0);
        if (control != NULL && (strcmp(control, "off") == 0 || strcmp(control, "none") == 0)) return 0;
        if (!numbered) {
            size_t n = strlen(dest) + 16; char *candidate = (char *)malloc(n);
            if (candidate == NULL) return -1;
            snprintf(candidate, n, "%s.~1~", dest);
            numbered = lstat(candidate, &info) == 0;
            free(candidate);
        }
    }
    const char *suffix = opt->suffix != NULL ? opt->suffix : getenv("SIMPLE_BACKUP_SUFFIX");
    if (suffix == NULL) suffix = "~";
    size_t n = strlen(dest) + strlen(suffix) + 32;
    char *candidate = (char *)malloc(n);
    if (candidate == NULL) return -1;
    int result = -1;
    if (numbered) {
        for (unsigned long i = 1; i < 1000000; ++i) {
            snprintf(candidate, n, "%s.~%lu~", dest, i);
            if (lstat(candidate, &info) != 0 && errno == ENOENT) {
                result = rename(dest, candidate); break;
            }
        }
    } else {
        snprintf(candidate, n, "%s%s", dest, suffix);
        result = rename(dest, candidate);
    }
    int error = errno; free(candidate); errno = error;
    return result;
}

static inline int zov_cp_verbose(const char *source, const char *dest, const char *op) {
    zov_quote_shell(stdout, source); fputs(op, stdout); zov_quote_shell(stdout, dest);
    return fputc('\n', stdout) == EOF;
}

static inline char *zov_cp_readlink(const char *path, const struct stat *info) {
    size_t cap = info->st_size > 0 ? (size_t)info->st_size + 1 : 256;
    for (;;) {
        if (cap > SIZE_MAX / 2) { errno = ENAMETOOLONG; return NULL; }
        char *value = (char *)malloc(cap + 1);
        if (value == NULL) return NULL;
        ssize_t n = readlink(path, value, cap);
        if (n < 0) { int e = errno; free(value); errno = e; return NULL; }
        if ((size_t)n < cap) { value[n] = '\0'; return value; }
        free(value); cap *= 2;
    }
}

static inline int zov_cp_copy_one(const char *source, const char *dest,
                                   const zov_cp_options *opt, int command_line);

static inline int zov_cp_copy_regular(const char *source, const char *dest,
                                      const struct stat *src_info,
                                      const zov_cp_options *opt) {
    struct stat dest_info;
    struct stat dest_link_info;
    int dest_exists = lstat(dest, &dest_link_info) == 0;
    int dest_target_exists = stat(dest, &dest_info) == 0;
    if (dest_target_exists && zov_cp_same(src_info, &dest_info)) {
        fprintf(stderr, "cp: "); zov_quote_shell(stderr, source);
        fputs(" and ", stderr); zov_quote_shell(stderr, dest);
        fputs(" are the same file\n", stderr); return 1;
    }
    if (dest_target_exists && opt->update && src_info->st_mtime <= dest_info.st_mtime) return 0;
    if (dest_exists && opt->no_clobber) return 0;
    if (dest_exists && opt->interactive) {
        fputs("cp: overwrite ", stderr); zov_quote_shell(stderr, dest);
        fputs("? ", stderr); fflush(stderr);
        char answer[32];
        if (fgets(answer, sizeof(answer), stdin) == NULL || (answer[0] != 'y' && answer[0] != 'Y')) return 1;
    }
    if (dest_exists && opt->remove_destination && unlink(dest) != 0) {
        zov_cp_error("cannot remove", dest, errno); return 1;
    }
    if (dest_exists && opt->backup && zov_cp_backup_existing(dest, opt) != 0) {
        zov_cp_error("cannot create backup", dest, errno); return 1;
    }
    int in = open(source, O_RDONLY);
    if (in < 0) { zov_cp_error("cannot open", source, errno); return 1; }
    struct stat opened;
    if (fstat(in, &opened) != 0) { int e = errno; close(in); zov_cp_error("cannot stat", source, e); return 1; }
    if (stat(dest, &dest_info) == 0 && zov_cp_same(&opened, &dest_info)) {
        close(in); fprintf(stderr, "cp: "); zov_quote_shell(stderr, source);
        fputs(" and ", stderr); zov_quote_shell(stderr, dest);
        fputs(" are the same file\n", stderr); return 1;
    }
    mode_t old_mask = umask(0); umask(old_mask);
    mode_t create_mode = (src_info->st_mode & 0777) & ~old_mask;
    int out = open(dest, O_WRONLY | O_CREAT | O_TRUNC, create_mode);
    if (out < 0 && opt->force && !opt->no_clobber && unlink(dest) == 0)
        out = open(dest, O_WRONLY | O_CREAT | O_TRUNC, create_mode);
    if (out < 0) {
        int e = errno; close(in); zov_cp_error("cannot create regular file", dest, e); return 1;
    }
    char buffer[65536]; int status = 0;
    for (;;) {
        ssize_t n = read(in, buffer, sizeof(buffer));
        if (n == 0) break;
        if (n < 0) { if (errno == EINTR) continue; zov_cp_error("error reading", source, errno); status = 1; break; }
        ssize_t offset = 0;
        while (offset < n) {
            ssize_t written = write(out, buffer + offset, (size_t)(n - offset));
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) { zov_cp_error("error writing", dest, errno); status = 1; break; }
            offset += written;
        }
        if (status) break;
    }
    if (opt->preserve) {
        if (fchmod(out, src_info->st_mode & 07777) != 0) { zov_cp_error("preserving permissions for", dest, errno); status = 1; }
        struct timespec times[2];
#if defined(__APPLE__)
        times[0] = src_info->st_atimespec; times[1] = src_info->st_mtimespec;
#else
        times[0] = src_info->st_atim; times[1] = src_info->st_mtim;
#endif
        if (futimens(out, times) != 0) { zov_cp_error("preserving times for", dest, errno); status = 1; }
    }
    if (close(in) != 0 && !status) { zov_cp_error("closing", source, errno); status = 1; }
    if (close(out) != 0 && !status) { zov_cp_error("closing", dest, errno); status = 1; }
    if (status) return status;
    return opt->verbose ? zov_cp_verbose(source, dest, " -> ") : 0;
}

static inline int zov_cp_copy_link(const char *source, const char *dest,
                                    const struct stat *src_info,
                                    const zov_cp_options *opt) {
    char *target = zov_cp_readlink(source, src_info);
    if (target == NULL) { zov_cp_error("cannot read symbolic link", source, errno); return 1; }
    if (symlink(target, dest) != 0) { int e = errno; free(target); zov_cp_error("cannot create symbolic link", dest, e); return 1; }
    free(target);
    return opt->verbose ? zov_cp_verbose(source, dest, " -> ") : 0;
}

static inline int zov_cp_copy_directory(const char *source, const char *dest,
                                         const struct stat *src_info,
                                         const zov_cp_options *opt) {
    if (!opt->recursive) {
        fputs("cp: -r not specified; omitting directory ", stderr);
        zov_quote_shell(stderr, source); fputc('\n', stderr); return 1;
    }
    struct stat dst_info;
    if (zov_cp_destination_inside(source, dest)) {
        if (stat(dest, &dst_info) != 0 && mkdir(dest, (src_info->st_mode & 0777) & 0777) != 0) {
            zov_cp_error("cannot create directory", dest, errno); return 1;
        }
        DIR *inside = opendir(source);
        if (inside == NULL) { zov_cp_error("cannot open directory", source, errno); return 1; }
        struct dirent *item;
        while ((item = readdir(inside)) != NULL) {
            if (strcmp(item->d_name, ".") == 0 || strcmp(item->d_name, "..") == 0) continue;
            char *src_child = zov_cp_join(source, item->d_name);
            char *dst_child = zov_cp_join(dest, item->d_name);
            if (src_child == NULL || dst_child == NULL) { free(src_child); free(dst_child); continue; }
            if (!zov_cp_path_contains(src_child, dest)) (void)zov_cp_copy_one(src_child, dst_child, opt, 0);
            free(src_child); free(dst_child);
        }
        closedir(inside);
        fprintf(stderr, "cp: cannot copy a directory, "); zov_quote_shell(stderr, source);
        fputs(", into itself, ", stderr); zov_quote_shell(stderr, dest); fputc('\n', stderr); return 1;
    }
    if (stat(dest, &dst_info) == 0) {
        if (!S_ISDIR(dst_info.st_mode)) { zov_cp_error("cannot overwrite non-directory", dest, ENOTDIR); return 1; }
        if (zov_cp_same(src_info, &dst_info)) {
            fprintf(stderr, "cp: cannot copy a directory, "); zov_quote_shell(stderr, source);
            fputs(", into itself, ", stderr); zov_quote_shell(stderr, dest); fputc('\n', stderr); return 1;
        }
    } else if (mkdir(dest, (src_info->st_mode & 0777) & 0777) != 0) {
        zov_cp_error("cannot create directory", dest, errno); return 1;
    }
    DIR *dir = opendir(source);
    if (dir == NULL) { zov_cp_error("cannot open directory", source, errno); return 1; }
    int status = 0; struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        char *src_child = zov_cp_join(source, entry->d_name);
        char *dst_child = zov_cp_join(dest, entry->d_name);
        if (src_child == NULL || dst_child == NULL) { free(src_child); free(dst_child); status = 1; continue; }
        if (zov_cp_copy_one(src_child, dst_child, opt, 0) != 0) status = 1;
        free(src_child); free(dst_child);
    }
    if (closedir(dir) != 0) status = 1;
    if (opt->preserve) {
        if (chmod(dest, src_info->st_mode & 07777) != 0) status = 1;
        struct timespec times[2];
#if defined(__APPLE__)
        times[0] = src_info->st_atimespec; times[1] = src_info->st_mtimespec;
#else
        times[0] = src_info->st_atim; times[1] = src_info->st_mtim;
#endif
        if (utimensat(AT_FDCWD, dest, times, 0) != 0) status = 1;
    }
    if (opt->verbose && !status) status = zov_cp_verbose(source, dest, " -> ");
    return status;
}

static inline int zov_cp_copy_one(const char *source, const char *dest,
                                   const zov_cp_options *opt, int command_line) {
    struct stat info;
    int stat_result = (!opt->dereference || (!command_line && opt->dereference == 2))
        ? lstat(source, &info) : stat(source, &info);
    if (stat_result != 0) { zov_cp_error("cannot stat", source, errno); return 1; }
    if (S_ISLNK(info.st_mode) && (!opt->dereference || (!command_line && opt->dereference == 2)))
        return zov_cp_copy_link(source, dest, &info, opt);
    if (S_ISDIR(info.st_mode)) return zov_cp_copy_directory(source, dest, &info, opt);
    if (opt->symlink) {
        if (symlink(source, dest) != 0) { zov_cp_error("cannot create symbolic link", dest, errno); return 1; }
        return opt->verbose ? zov_cp_verbose(source, dest, " -> ") : 0;
    }
    if (opt->hardlink) {
        if (link(source, dest) != 0) { zov_cp_error("cannot create hard link", dest, errno); return 1; }
        return opt->verbose ? zov_cp_verbose(source, dest, " => ") : 0;
    }
    return zov_cp_copy_regular(source, dest, &info, opt);
}

static inline int zov_cp_long(const char *arg, const char *name, size_t keylen) {
    return keylen > 2 && keylen <= strlen(name) && strncmp(arg, name, keylen) == 0;
}

static inline int zov_cp_backup_mode(const char *value) {
    if (value == NULL || strcmp(value, "existing") == 0 || strcmp(value, "nil") == 0) return 0;
    if (strcmp(value, "numbered") == 0 || strcmp(value, "t") == 0) return 1;
    if (strcmp(value, "simple") == 0 || strcmp(value, "never") == 0) return 2;
    if (strcmp(value, "none") == 0 || strcmp(value, "off") == 0) return 3;
    return -1;
}

static inline void zov_cp_help(void) {
    fputs(
          "Usage: cp [OPTION]... [-T] SOURCE DEST\n"
          "  or:  cp [OPTION]... SOURCE... DIRECTORY\n"
          "  or:  cp [OPTION]... -t DIRECTORY SOURCE...\n"
          "Copy SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.\n"
          "\n"
          "Mandatory arguments to long options are mandatory for short options too.\n"
          "  -a, --archive\n"
          "         same as -dR --preserve=all\n"
          "      --attributes-only\n"
          "         don't copy the file data, just the attributes\n"
          "      --backup[=CONTROL]\n"
          "         make a backup of each existing destination file\n"
          "  -b\n"
          "         like --backup but does not accept an argument\n"
          "      --copy-contents\n"
          "         copy contents of special files when recursive\n"
          "  -d\n"
          "         same as --no-dereference --preserve=links\n"
          "      --debug\n"
          "         explain how a file is copied.  Implies -v\n"
          "  -f, --force\n"
          "         if an existing destination file cannot be opened, remove it and try\n"
          "         again (this option is ignored when the -n option is also used)\n"
          "  -i, --interactive\n"
          "         prompt before overwrite (overrides a previous -n option)\n"
          "  -H\n"
          "         follow command-line symbolic links in SOURCE\n"
          "  -L, --dereference\n"
          "         always follow symbolic links in SOURCE\n"
          "  -P, --no-dereference\n"
          "         never follow symbolic links in SOURCE\n"
          "      --keep-directory-symlink\n"
          "         follow existing symlinks to directories\n"
          "  -l, --link\n"
          "         hard link files instead of copying\n"
          "  -n, --no-clobber\n"
          "         (deprecated) silently skip existing files.  See also --update\n"
          "  -p\n"
          "         same as --preserve=mode,ownership,timestamps\n"
          "      --preserve[=ATTR_LIST]\n"
          "         preserve the specified attributes\n"
          "      --no-preserve=ATTR_LIST\n"
          "         don't preserve the specified attributes\n"
          "      --parents\n"
          "         use full source file name under DIRECTORY\n"
          "  -R, -r, --recursive\n"
          "         copy directories recursively\n"
          "      --reflink[=WHEN]\n"
          "         control clone/CoW copies. See below\n"
          "      --remove-destination\n"
          "         remove each existing destination file before attempting to open it\n"
          "         (contrast with --force)\n"
          "      --sparse=WHEN\n"
          "         control creation of sparse files. See below\n"
          "      --strip-trailing-slashes\n"
          "         remove any trailing slashes from each SOURCE argument\n"
          "  -s, --symbolic-link\n"
          "         make symbolic links instead of copying\n"
          "  -S, --suffix=SUFFIX\n"
          "         override the usual backup suffix\n"
          "  -t, --target-directory=DIRECTORY\n"
          "         copy all SOURCE arguments into DIRECTORY\n"
          "  -T, --no-target-directory\n"
          "         treat DEST as a normal file\n"
          "      --update[=UPDATE]\n"
          "         control which existing files are updated;\n"
          "         UPDATE={all,none,none-fail,older(default)}\n"
          "  -u\n"
          "         equivalent to --update[=older].  See below\n"
          "  -v, --verbose\n"
          "         explain what is being done\n"
          "  -x, --one-file-system\n"
          "         stay on this file system\n"
          "  -Z\n"
          "         set SELinux security context of destination file to default type\n"
          "      --context[=CTX]\n"
          "         like -Z, or if CTX is specified then set the\n"
          "         SELinux or SMACK security context to CTX\n"
          "      --help\n"
          "         display this help and exit\n"
          "      --version\n"
          "         output version information and exit\n"
          "\n"
          "ATTR_LIST is a comma-separated list of attributes. Attributes are 'mode' for\n"
          "permissions (including any ACL and xattr permissions), 'ownership' for user\n"
          "and group, 'timestamps' for file timestamps, 'links' for hard links, 'context'\n"
          "for security context, 'xattr' for extended attributes, and 'all' for all\n"
          "attributes.\n"
          "\n"
          "By default, sparse SOURCE files are detected by a crude heuristic and the\n"
          "corresponding DEST file is made sparse as well.  That is the behavior\n"
          "selected by --sparse=auto.  Specify --sparse=always to create a sparse DEST\n"
          "file whenever the SOURCE file contains a long enough sequence of zero bytes.\n"
          "Use --sparse=never to inhibit creation of sparse files.\n"
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
          "By default or with --reflink=auto, cp will try a lightweight copy, where the\n"
          "data blocks are copied only when modified, falling back to a standard copy\n"
          "if this is not possible.  With --reflink[=always] cp will fail if CoW is not\n"
          "supported, while --reflink=never ensures a standard copy is performed.\n"
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
          "As a special case, cp makes a backup of SOURCE when the force and backup\n"
          "options are given and SOURCE and DEST are the same name for an existing,\n"
          "regular file.\n"
          "\n"
          "Report bugs to: bug-coreutils@gnu.org\n"
          "GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n"
          "General help using GNU software: <https://www.gnu.org/gethelp/>\n"
          "Report any translation bugs to <https://translationproject.org/team/>\n"
          "Full documentation <https://www.gnu.org/software/coreutils/cp>\n"
          "or available locally via: info '(coreutils) cp invocation'\n", stdout);
}

static inline int zov_cp_run(int argc, int8_t **argv) {
    zov_cp_options opt = {0}; opt.dereference = 2;
    const char *target_dir = NULL;
    char **operands = (char **)malloc((size_t)(argc + 1) * sizeof(*operands));
    if (operands == NULL) return 1;
    int count = 0, done = 0;
    for (int i = 0; i < argc; ++i) {
        const char *arg = (char *)argv[i];
        if (!done && strcmp(arg, "--") == 0) { done = 1; continue; }
        if (!done && strncmp(arg, "--", 2) == 0) {
            const char *eq = strchr(arg, '='); size_t n = eq ? (size_t)(eq - arg) : strlen(arg);
            const char *value = eq ? eq + 1 : NULL;
            if (zov_cp_long(arg, "--help", n)) { if (value != NULL) { fputs("cp: option '--help' doesn't allow an argument\nTry 'cp --help' for more information.\n", stderr); free(operands); return 1; } zov_cp_help(); free(operands); return 0; }
            if (zov_cp_long(arg, "--version", n)) { puts("cp (zovutils) 0.10.30"); free(operands); return 0; }
            if (zov_cp_long(arg, "--recursive", n)) { opt.recursive = 1; continue; }
            if (zov_cp_long(arg, "--archive", n)) { opt.recursive = 1; opt.preserve = 1; opt.dereference = 0; continue; }
            if (zov_cp_long(arg, "--preserve", n)) { opt.preserve = 1; continue; }
            if (zov_cp_long(arg, "--no-preserve", n)) { opt.preserve = 0; continue; }
            if (zov_cp_long(arg, "--force", n)) { opt.force = 1; continue; }
            if (zov_cp_long(arg, "--interactive", n)) { opt.interactive = 1; opt.no_clobber = 0; continue; }
            if (zov_cp_long(arg, "--no-clobber", n)) { opt.no_clobber = 1; opt.interactive = 0; continue; }
            if (zov_cp_long(arg, "--verbose", n)) { opt.verbose = 1; continue; }
            if (zov_cp_long(arg, "--dereference", n)) { opt.dereference = 1; continue; }
            if (zov_cp_long(arg, "--no-dereference", n)) { opt.dereference = 0; continue; }
            if (zov_cp_long(arg, "--parents", n)) { opt.parents = 1; continue; }
            if (zov_cp_long(arg, "--no-target-directory", n)) { opt.no_target_dir = 1; continue; }
            if (zov_cp_long(arg, "--update", n)) { opt.update = 1; continue; }
            if (zov_cp_long(arg, "--remove-destination", n)) { opt.remove_destination = 1; continue; }
            if (zov_cp_long(arg, "--symbolic-link", n)) { opt.symlink = 1; continue; }
            if (zov_cp_long(arg, "--link", n)) { opt.hardlink = 1; continue; }
            if (zov_cp_long(arg, "--target-directory", n)) {
                if (value == NULL && i + 1 < argc) value = (char *)argv[++i];
                if (value == NULL) { fputs("cp: option '--target-directory' requires an argument\nTry 'cp --help' for more information.\n", stderr); free(operands); return 1; }
                target_dir = value; continue;
            }
            if (zov_cp_long(arg, "--backup", n)) {
                opt.backup = 1;
                if (value != NULL) {
                    opt.backup_control = zov_cp_backup_mode(value);
                    if (opt.backup_control < 0) { fprintf(stderr, "cp: invalid argument '%s' for 'backup type'\nTry 'cp --help' for more information.\n", value); free(operands); return 1; }
                    if (opt.backup_control == 3) opt.backup = 0;
                }
                continue;
            }
            if (zov_cp_long(arg, "--suffix", n)) {
                if (value == NULL && i + 1 < argc) value = (char *)argv[++i];
                if (value == NULL) { fputs("cp: option '--suffix' requires an argument\nTry 'cp --help' for more information.\n", stderr); free(operands); return 1; }
                opt.suffix = value; continue;
            }
            fprintf(stderr, "cp: unrecognized option '%s'\nTry 'cp --help' for more information.\n", arg); free(operands); return 1;
        }
        if (!done && arg[0] == '-' && arg[1] != '\0') {
            for (size_t j = 1; arg[j] != '\0'; ++j) {
                char c = arg[j];
                if (c == 'a') { opt.recursive = 1; opt.preserve = 1; opt.dereference = 0; }
                else if (c == 'R' || c == 'r') opt.recursive = 1;
                else if (c == 'f') opt.force = 1;
                else if (c == 'i') { opt.interactive = 1; opt.no_clobber = 0; }
                else if (c == 'n') { opt.no_clobber = 1; opt.interactive = 0; }
                else if (c == 'v') opt.verbose = 1;
                else if (c == 'p') opt.preserve = 1;
                else if (c == 'P') opt.dereference = 0;
                else if (c == 'L') opt.dereference = 1;
                else if (c == 'H') opt.dereference = 2;
                else if (c == 'u') opt.update = 1;
                else if (c == 'l') opt.hardlink = 1;
                else if (c == 's') opt.symlink = 1;
                else if (c == 'b') opt.backup = 1;
                else if (c == 'T') opt.no_target_dir = 1;
                else if (c == 'd') { opt.dereference = 0; opt.preserve = 1; }
                else if (c == 'S') {
                    const char *v = arg[j + 1] ? arg + j + 1 : (i + 1 < argc ? (char *)argv[++i] : NULL);
                    if (v == NULL) { fputs("cp: option requires an argument -- 'S'\nTry 'cp --help' for more information.\n", stderr); free(operands); return 1; }
                    opt.suffix = v; break;
                }
                else if (c == 't') {
                    const char *v = arg[j + 1] ? arg + j + 1 : (i + 1 < argc ? (char *)argv[++i] : NULL);
                    if (v == NULL) { fputs("cp: option requires an argument -- 't'\nTry 'cp --help' for more information.\n", stderr); free(operands); return 1; }
                    target_dir = v; break;
                } else { fprintf(stderr, "cp: invalid option -- '%c'\nTry 'cp --help' for more information.\n", c); free(operands); return 1; }
            }
            continue;
        }
        operands[count++] = (char *)arg;
    }
    if (count == 0) { fputs("cp: missing file operand\nTry 'cp --help' for more information.\n", stderr); free(operands); return 1; }
    if (target_dir != NULL) {
        struct stat info;
        int target_stat = stat(target_dir, &info);
        if (target_stat != 0 || !S_ISDIR(info.st_mode)) {
            fputs("cp: target directory ", stderr); zov_quote_shell(stderr, target_dir);
            fprintf(stderr, ": %s\n", target_stat != 0 ? strerror(errno) : strerror(ENOTDIR));
            free(operands); return 1;
        }
        int status = 0;
        for (int i = 0; i < count; ++i) {
            char *dest = zov_cp_destination(operands[i], target_dir, opt.parents);
            if (dest == NULL) { status = 1; continue; }
            opt.target_dir = target_dir;
            if (zov_cp_copy_one(operands[i], dest, &opt, 1) != 0) status = 1;
            free(dest);
        }
        free(operands); return status;
    }
    if (count < 2) { fputs("cp: missing destination file operand after ", stderr); zov_quote_shell(stderr, operands[0]); fputs("\nTry 'cp --help' for more information.\n", stderr); free(operands); return 1; }
    const char *dest = operands[count - 1]; struct stat dst_info;
    int is_dir = !opt.no_target_dir && stat(dest, &dst_info) == 0 && S_ISDIR(dst_info.st_mode);
    if (count > 2 && !is_dir) {
        int target_stat = stat(dest, &dst_info);
        fputs("cp: target ", stderr); zov_quote_shell(stderr, dest);
        fprintf(stderr, ": %s\n", target_stat != 0 ? strerror(errno) : strerror(ENOTDIR));
        free(operands); return 1;
    }
    if (count == 2 && opt.no_target_dir && stat(dest, &dst_info) == 0 && S_ISDIR(dst_info.st_mode)) {
        struct stat src_info;
        if (stat(operands[0], &src_info) == 0 && !S_ISDIR(src_info.st_mode)) {
            fputs("cp: cannot overwrite directory ", stderr); zov_quote_shell(stderr, dest);
            fputs(" with non-directory ", stderr); zov_quote_shell(stderr, operands[0]); fputc('\n', stderr);
            free(operands); return 1;
        }
    }
    int status = 0;
    for (int i = 0; i < count - 1; ++i) {
        char *out = is_dir ? zov_cp_destination(operands[i], dest, opt.parents) : strdup((char *)dest);
        if (out == NULL) { status = 1; continue; }
        if (zov_cp_copy_one(operands[i], out, &opt, 1) != 0) status = 1;
        free(out);
    }
    free(operands); return status;
}

#endif
