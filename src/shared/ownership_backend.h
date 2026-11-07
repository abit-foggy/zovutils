#ifndef ZOVUTILS_OWNERSHIP_BACKEND_H
#define ZOVUTILS_OWNERSHIP_BACKEND_H

#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "platform_quote.h"

typedef struct {
    int recursive, force, verbose, changes_only, no_dereference;
    int traverse_mode, traverse_mode_set, dereference_set, preserve_root;
    int from_uid_set, from_gid_set;
    uid_t from_uid; gid_t from_gid;
} zov_owner_options;

typedef struct { uid_t uid; gid_t gid; int set_uid; int set_gid; } zov_owner_spec;

static inline int zov_owner_numeric(const char *text, unsigned long *number) {
    if (*text == '\0') return 0;
    unsigned long value = 0;
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9') return 0;
        unsigned digit = (unsigned)(*p - '0');
        if (value > (ULONG_MAX - digit) / 10) return -1;
        value = value * 10 + digit;
    }
    *number = value; return 1;
}

static inline int zov_owner_parse_user(const char *text, uid_t *uid) {
    unsigned long value; int numeric = zov_owner_numeric(text, &value);
    if (numeric < 0) return 0;
    if (numeric) {
        if (value > (unsigned long)(uid_t)-1) return 0;
        *uid = (uid_t)value; return 1;
    }
    struct passwd *entry = getpwnam(text);
    if (entry == NULL) return 0;
    *uid = entry->pw_uid; return 1;
}

static inline int zov_owner_parse_group(const char *text, gid_t *gid) {
    unsigned long value; int numeric = zov_owner_numeric(text, &value);
    if (numeric < 0) return 0;
    if (numeric) {
        if (value > (unsigned long)(gid_t)-1) return 0;
        *gid = (gid_t)value; return 1;
    }
    struct group *entry = getgrnam(text);
    if (entry == NULL) return 0;
    *gid = entry->gr_gid; return 1;
}

static inline int zov_owner_parse_spec(const char *name, const char *text, int group_only,
                                       zov_owner_spec *spec, int diagnose) {
    spec->uid = (uid_t)-1; spec->gid = (gid_t)-1; spec->set_uid = 0; spec->set_gid = 0;
    char *copy = strdup(text);
    if (copy == NULL) return 0;
    char *colon = strchr(copy, ':');
    if (colon != NULL && strchr(colon + 1, ':') != NULL) { free(copy); return 0; }
    if (group_only) {
        const char *group = colon == NULL ? copy : colon + 1;
        if (*group == '\0' || !zov_owner_parse_group(group, &spec->gid)) {
            if (diagnose) { fprintf(stderr, "%s: invalid group: ", name); zov_quote_shell(stderr, text); fputc('\n', stderr); }
            free(copy); return 0;
        }
        spec->set_gid = 1; free(copy); return 1;
    }
    if (colon != NULL) *colon++ = '\0';
    if (*copy != '\0') {
        if (!zov_owner_parse_user(copy, &spec->uid)) {
            if (diagnose) { fprintf(stderr, "%s: invalid user: ", name); zov_quote_shell(stderr, text); fputc('\n', stderr); }
            free(copy); return 0;
        }
        spec->set_uid = 1;
    }
    if (colon != NULL && *colon != '\0') {
        if (!zov_owner_parse_group(colon, &spec->gid)) {
            if (diagnose) { fprintf(stderr, "%s: invalid group: ", name); zov_quote_shell(stderr, text); fputc('\n', stderr); }
            free(copy); return 0;
        }
        spec->set_gid = 1;
    }
    if (!spec->set_uid && !spec->set_gid) {
        if (diagnose) { fprintf(stderr, "%s: invalid spec: ", name); zov_quote_shell(stderr, text); fputc('\n', stderr); }
        free(copy); return 0;
    }
    free(copy); return 1;
}

static inline void zov_owner_access_error(const char *name, const char *path, int error, int force) {
    if (force) return;
    fprintf(stderr, "%s: cannot access ", name); zov_quote_shell(stderr, path);
    fprintf(stderr, ": %s\n", strerror(error));
}

static inline void zov_owner_change_error(const char *name, const char *path, int error, int force) {
    if (force) return;
    fprintf(stderr, "%s: changing ownership of ", name); zov_quote_shell(stderr, path);
    fprintf(stderr, ": %s\n", strerror(error));
}

static inline int zov_owner_matches(const struct stat *info, const zov_owner_options *opt) {
    return (!opt->from_uid_set || info->st_uid == opt->from_uid)
        && (!opt->from_gid_set || info->st_gid == opt->from_gid);
}

static inline void zov_owner_print_id(FILE *stream, uid_t uid, int user_id) {
    if (user_id) {
        struct passwd *entry = getpwuid(uid);
        if (entry != NULL) { fputs(entry->pw_name, stream); return; }
    } else {
        struct group *entry = getgrgid((gid_t)uid);
        if (entry != NULL) { fputs(entry->gr_name, stream); return; }
    }
    fprintf(stream, "%lu", (unsigned long)uid);
}

static inline void zov_owner_verbose(const char *name, const char *path,
                                     const struct stat *old_info,
                                     const zov_owner_spec *target,
                                     int group_only, int changed) {
    zov_owner_spec desired = *target;
    if (!desired.set_uid) { desired.uid = old_info->st_uid; desired.set_uid = 1; }
    if (!desired.set_gid) { desired.gid = old_info->st_gid; desired.set_gid = 1; }
    if (group_only) fputs("group of ", stdout);
    else fputs("ownership of ", stdout);
    zov_quote_shell(stdout, path);
    if (changed) fputs(" changed from ", stdout);
    else fputs(" retained as ", stdout);
    if (!group_only && (target->set_uid || target->set_gid)) {
        zov_owner_print_id(stdout, old_info->st_uid, 1);
        if (target->set_gid) { fputc(':', stdout); zov_owner_print_id(stdout, old_info->st_gid, 0); }
        if (changed) {
            fputs(" to ", stdout);
            zov_owner_print_id(stdout, desired.uid, 1);
            if (target->set_gid) { fputc(':', stdout); zov_owner_print_id(stdout, desired.gid, 0); }
        }
    } else if (group_only) {
        zov_owner_print_id(stdout, old_info->st_gid, 0);
        if (changed) { fputs(" to ", stdout); zov_owner_print_id(stdout, desired.gid, 0); }
    }
    fputc('\n', stdout);
    (void)name;
}

static inline int zov_owner_apply(const char *name, const char *path,
                                  const zov_owner_spec *target,
                                  const zov_owner_options *opt, int follow) {
    struct stat info;
    if ((follow ? stat(path, &info) : lstat(path, &info)) != 0) {
        zov_owner_access_error(name, path, errno, opt->force); return 1;
    }
    if (!zov_owner_matches(&info, opt)) return 0;
    uid_t uid = target->set_uid ? target->uid : (uid_t)-1;
    gid_t gid = target->set_gid ? target->gid : (gid_t)-1;
    int changed = (target->set_uid && info.st_uid != target->uid)
               || (target->set_gid && info.st_gid != target->gid);
    if (!changed) {
        if (opt->verbose && !opt->changes_only) zov_owner_verbose(name, path, &info, target, strcmp(name, "chgrp") == 0, 0);
        return 0;
    }
    int result = follow ? chown(path, uid, gid) : lchown(path, uid, gid);
    if (result != 0) { zov_owner_change_error(name, path, errno, opt->force); return 1; }
    if (opt->verbose && (!opt->changes_only || changed))
        zov_owner_verbose(name, path, &info, target, strcmp(name, "chgrp") == 0, changed);
    return 0;
}

static inline int zov_owner_path(const char *name, const char *path,
                                 const zov_owner_spec *target,
                                 const zov_owner_options *opt, int command_line) {
    struct stat link_info, info;
    if (lstat(path, &link_info) != 0) {
        zov_owner_access_error(name, path, errno, opt->force); return 1;
    }
    if (S_ISLNK(link_info.st_mode)) {
        if (opt->no_dereference || (opt->recursive && !command_line && opt->traverse_mode != 1)
            || (opt->recursive && command_line && opt->traverse_mode == 2))
            return zov_owner_apply(name, path, target, opt, 0);
    }
    if (stat(path, &info) != 0) return zov_owner_apply(name, path, target, opt, 1);
    if (opt->recursive && opt->preserve_root) {
        struct stat root_info;
        if (stat("/", &root_info) == 0 && info.st_dev == root_info.st_dev && info.st_ino == root_info.st_ino) {
            fprintf(stderr, "%s: it is dangerous to operate recursively on ", name); zov_quote_shell(stderr, path);
            fprintf(stderr, "\n%s: use --no-preserve-root to override this failsafe\n", name);
            return 1;
        }
    }
    if (!opt->recursive || !S_ISDIR(info.st_mode)) return zov_owner_apply(name, path, target, opt, 1);
    int status = 0; DIR *dir = opendir(path);
    if (dir == NULL) { zov_owner_access_error(name, path, errno, opt->force); return 1; }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        size_t a = strlen(path), b = strlen(entry->d_name); int slash = a > 0 && path[a - 1] != '/';
        char *child = (char *)malloc(a + b + (size_t)slash + 1);
        if (child == NULL) { status = 1; continue; }
        memcpy(child, path, a); if (slash) child[a++] = '/'; memcpy(child + a, entry->d_name, b + 1);
        if (zov_owner_path(name, child, target, opt, 0) != 0) status = 1;
        free(child);
    }
    if (closedir(dir) != 0) status = 1;
    if (zov_owner_apply(name, path, target, opt, 1) != 0) status = 1;
    return status;
}

static inline int zov_owner_long(const char *arg, const char *name, size_t len) {
    return len > 2 && len <= strlen(name) && strncmp(arg, name, len) == 0;
}

static inline void zov_chown_help(void) {
    fputs("Usage: chown [OPTION]... [OWNER][:[GROUP]] FILE...\n", stdout);
    fputs("  or:  chown [OPTION]... --reference=RFILE FILE...\n", stdout);
    fputs("Change the owner and/or group of each FILE to OWNER and/or GROUP.\n", stdout);
    fputs("With --reference, change the owner and group of each FILE to those of RFILE.\n", stdout);
    fputs("\n", stdout);
    fputs("  -c, --changes\n", stdout);
    fputs("         like verbose but report only when a change is made\n", stdout);
    fputs("  -f, --silent, --quiet\n", stdout);
    fputs("         suppress most error messages\n", stdout);
    fputs("  -v, --verbose\n", stdout);
    fputs("         output a diagnostic for every file processed\n", stdout);
    fputs("      --dereference\n", stdout);
    fputs("         affect the referent of each symbolic link (this is\n", stdout);
    fputs("         the default), rather than the symbolic link itself\n", stdout);
    fputs("  -h, --no-dereference\n", stdout);
    fputs("         affect symbolic links instead of any referenced file;\n", stdout);
    fputs("         useful only on systems that can change the ownership of a symlink\n", stdout);
    fputs("      --from=CURRENT_OWNER:CURRENT_GROUP\n", stdout);
    fputs("         change the ownership of each file only if its\n", stdout);
    fputs("         current owner and/or group match those specified here.\n", stdout);
    fputs("         Either may be omitted, in which case a match\n", stdout);
    fputs("         is not required for the omitted attribute\n", stdout);
    fputs("      --no-preserve-root\n", stdout);
    fputs("         do not treat '/' specially (the default)\n", stdout);
    fputs("      --preserve-root\n", stdout);
    fputs("         fail to operate recursively on '/'\n", stdout);
    fputs("      --reference=RFILE\n", stdout);
    fputs("         use RFILE's ownership rather than specifying values.\n", stdout);
    fputs("         RFILE is always dereferenced if a symbolic link.\n", stdout);
    fputs("  -R, --recursive\n", stdout);
    fputs("         operate on files and directories recursively\n", stdout);
    fputs("\n", stdout);
    fputs("The following options modify how a hierarchy is traversed when the -R\n", stdout);
    fputs("option is also specified.  If more than one is specified, only the final\n", stdout);
    fputs("one takes effect. -P is the default.\n", stdout);
    fputs("\n", stdout);
    fputs("  -H\n", stdout);
    fputs("         if a command line argument is a symlink to a directory, traverse it\n", stdout);
    fputs("  -L\n", stdout);
    fputs("         traverse every symbolic link to a directory encountered\n", stdout);
    fputs("  -P\n", stdout);
    fputs("         do not traverse any symbolic links\n", stdout);
    fputs("\n", stdout);
    fputs("      --help\n", stdout);
    fputs("         display this help and exit\n", stdout);
    fputs("      --version\n", stdout);
    fputs("         output version information and exit\n", stdout);
    fputs("\n", stdout);
    fputs("Owner is unchanged if missing.  Group is unchanged if missing, but changed\n", stdout);
    fputs("to login group if implied by a ':' following a symbolic OWNER.\n", stdout);
    fputs("OWNER and GROUP may be numeric as well as symbolic.\n", stdout);
    fputs("\n", stdout);
    fputs("Examples:\n", stdout);
    fputs("  chown root /u        Change the owner of /u to \"root\".\n", stdout);
    fputs("  chown root:staff /u  Likewise, but also change its group to \"staff\".\n", stdout);
    fputs("  chown -hR root /u    Change the owner of /u and subfiles to \"root\".\n", stdout);
    fputs("\n", stdout);
    fputs("Report bugs to: bug-coreutils@gnu.org\n", stdout);
    fputs("GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n", stdout);
    fputs("General help using GNU software: <https://www.gnu.org/gethelp/>\n", stdout);
    fputs("Report any translation bugs to <https://translationproject.org/team/>\n", stdout);
    fputs("Full documentation <https://www.gnu.org/software/coreutils/chown>\n", stdout);
    fputs("or available locally via: info '(coreutils) chown invocation'\n", stdout);
}
static inline void zov_chgrp_help(void) {
    fputs("Usage: chgrp [OPTION]... GROUP FILE...\n", stdout);
    fputs("  or:  chgrp [OPTION]... --reference=RFILE FILE...\n", stdout);
    fputs("Change the group of each FILE to GROUP.\n", stdout);
    fputs("With --reference, change the group of each FILE to that of RFILE.\n", stdout);
    fputs("\n", stdout);
    fputs("  -c, --changes\n", stdout);
    fputs("         like verbose but report only when a change is made\n", stdout);
    fputs("  -f, --silent, --quiet\n", stdout);
    fputs("         suppress most error messages\n", stdout);
    fputs("  -v, --verbose\n", stdout);
    fputs("         output a diagnostic for every file processed\n", stdout);
    fputs("      --dereference\n", stdout);
    fputs("         affect the referent of each symbolic link (this is\n", stdout);
    fputs("         the default), rather than the symbolic link itself\n", stdout);
    fputs("  -h, --no-dereference\n", stdout);
    fputs("         affect symbolic links instead of any referenced file;\n", stdout);
    fputs("         useful only on systems that can change the ownership of a symlink\n", stdout);
    fputs("      --from=CURRENT_OWNER:CURRENT_GROUP\n", stdout);
    fputs("         change the ownership of each file only if its\n", stdout);
    fputs("         current owner and/or group match those specified here.\n", stdout);
    fputs("         Either may be omitted, in which case a match\n", stdout);
    fputs("         is not required for the omitted attribute\n", stdout);
    fputs("      --no-preserve-root\n", stdout);
    fputs("         do not treat '/' specially (the default)\n", stdout);
    fputs("      --preserve-root\n", stdout);
    fputs("         fail to operate recursively on '/'\n", stdout);
    fputs("      --reference=RFILE\n", stdout);
    fputs("         use RFILE's ownership rather than specifying values.\n", stdout);
    fputs("         RFILE is always dereferenced if a symbolic link.\n", stdout);
    fputs("  -R, --recursive\n", stdout);
    fputs("         operate on files and directories recursively\n", stdout);
    fputs("\n", stdout);
    fputs("The following options modify how a hierarchy is traversed when the -R\n", stdout);
    fputs("option is also specified.  If more than one is specified, only the final\n", stdout);
    fputs("one takes effect. -P is the default.\n", stdout);
    fputs("\n", stdout);
    fputs("  -H\n", stdout);
    fputs("         if a command line argument is a symlink to a directory, traverse it\n", stdout);
    fputs("  -L\n", stdout);
    fputs("         traverse every symbolic link to a directory encountered\n", stdout);
    fputs("  -P\n", stdout);
    fputs("         do not traverse any symbolic links\n", stdout);
    fputs("\n", stdout);
    fputs("      --help\n", stdout);
    fputs("         display this help and exit\n", stdout);
    fputs("      --version\n", stdout);
    fputs("         output version information and exit\n", stdout);
    fputs("\n", stdout);
    fputs("Examples:\n", stdout);
    fputs("  chgrp staff /u      Change the group of /u to \"staff\".\n", stdout);
    fputs("  chgrp -hR staff /u  Change the group of /u and subfiles to \"staff\".\n", stdout);
    fputs("\n", stdout);
    fputs("Report bugs to: bug-coreutils@gnu.org\n", stdout);
    fputs("GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n", stdout);
    fputs("General help using GNU software: <https://www.gnu.org/gethelp/>\n", stdout);
    fputs("Report any translation bugs to <https://translationproject.org/team/>\n", stdout);
    fputs("Full documentation <https://www.gnu.org/software/coreutils/chgrp>\n", stdout);
    fputs("or available locally via: info '(coreutils) chgrp invocation'\n", stdout);
}

static inline int zov_ownership_run(int argc, int8_t **argv, int group_only) {
    const char *name = group_only ? "chgrp" : "chown";
    zov_owner_options opt = {0}; opt.traverse_mode = 2;
    zov_owner_spec from = {0}, target = {0}; int from_given = 0;
    const char *reference = NULL;
    char **operands = (char **)malloc((size_t)(argc + 1) * sizeof(*operands));
    if (operands == NULL) return 1;
    int count = 0, done = 0;
    for (int i = 0; i < argc; ++i) {
        const char *arg = (char *)argv[i];
        if (!done && strcmp(arg, "--") == 0) { done = 1; continue; }
        if (!done && strncmp(arg, "--", 2) == 0) {
            const char *eq = strchr(arg, '='); size_t len = eq ? (size_t)(eq - arg) : strlen(arg);
            const char *value = eq ? eq + 1 : NULL;
            if (zov_owner_long(arg, "--help", len)) { if (value != NULL) { fprintf(stderr, "%s: option '--help' doesn't allow an argument\nTry '%s --help' for more information.\n", name, name); free(operands); return 1; } if (group_only) zov_chgrp_help(); else zov_chown_help(); free(operands); return 0; }
            if (zov_owner_long(arg, "--version", len)) { printf("%s (zovutils) 0.6.0\n", name); free(operands); return 0; }
            if (zov_owner_long(arg, "--recursive", len)) { opt.recursive = 1; continue; }
            if (zov_owner_long(arg, "--verbose", len)) { opt.verbose = 1; continue; }
            if (zov_owner_long(arg, "--changes", len)) { opt.verbose = opt.changes_only = 1; continue; }
            if (zov_owner_long(arg, "--silent", len) || zov_owner_long(arg, "--quiet", len)) { opt.force = 1; continue; }
            if (zov_owner_long(arg, "--no-dereference", len)) { opt.no_dereference = 1; continue; }
            if (zov_owner_long(arg, "--preserve-root", len)) { opt.preserve_root = 1; continue; }
            if (zov_owner_long(arg, "--no-preserve-root", len)) { opt.preserve_root = 0; continue; }
            if (zov_owner_long(arg, "--dereference", len)) { opt.no_dereference = 0; opt.dereference_set = 1; continue; }
            if (zov_owner_long(arg, "--reference", len)) {
                if (value == NULL && i + 1 < argc) value = (char *)argv[++i];
                if (value == NULL) { fprintf(stderr, "%s: option '--reference' requires an argument\nTry '%s --help' for more information.\n", name, name); free(operands); return 1; }
                reference = value; continue;
            }
            if (!group_only && zov_owner_long(arg, "--from", len)) {
                if (value == NULL && i + 1 < argc) value = (char *)argv[++i];
                if (value == NULL) { fputs("chown: option '--from' requires an argument\nTry 'chown --help' for more information.\n", stderr); free(operands); return 1; }
                if (*value != '\0') {
                    if (!zov_owner_parse_spec(name, value, 0, &from, 0)) { fprintf(stderr, "chown: invalid spec: "); zov_quote_shell(stderr, value); fputs("\n", stderr); free(operands); return 1; }
                    from_given = 1;
                }
                continue;
            }
            fprintf(stderr, "%s: unrecognized option '%s'\nTry '%s --help' for more information.\n", name, arg, name); free(operands); return 1;
        }
        if (!done && arg[0] == '-' && arg[1] != '\0') {
            for (size_t j = 1; arg[j] != '\0'; ++j) {
                char c = arg[j];
                if (c == 'R') opt.recursive = 1;
                else if (c == 'f') opt.force = 1;
                else if (c == 'v') opt.verbose = 1;
                else if (c == 'c') opt.verbose = opt.changes_only = 1;
                else if (c == 'h') opt.no_dereference = 1;
                else if (c == 'H') { opt.traverse_mode = 0; opt.traverse_mode_set = 1; }
                else if (c == 'L') { opt.traverse_mode = 1; opt.traverse_mode_set = 1; }
                else if (c == 'P') { opt.traverse_mode = 2; opt.traverse_mode_set = 1; }
                else { fprintf(stderr, "%s: invalid option -- '%c'\nTry '%s --help' for more information.\n", name, c, name); free(operands); return 1; }
            }
            continue;
        }
        operands[count++] = (char *)arg;
    }
    int first_file = 1;
    if (reference != NULL) {
        struct stat info;
        if (stat(reference, &info) != 0) {
            fprintf(stderr, "%s: failed to get attributes of ", name); zov_quote_shell(stderr, reference);
            fprintf(stderr, ": %s\n", strerror(errno)); free(operands); return 1;
        }
        target.uid = info.st_uid; target.gid = info.st_gid; target.set_uid = !group_only; target.set_gid = 1;
        first_file = 0;
    } else {
        if (count == 0) { fprintf(stderr, "%s: missing operand\nTry '%s --help' for more information.\n", name, name); free(operands); return 1; }
        if (!zov_owner_parse_spec(name, operands[0], group_only, &target, 1)) { free(operands); return 1; }
    }
    if (count <= first_file) {
        if (reference == NULL && count == 1) {
            fprintf(stderr, "%s: missing operand after ", name); zov_quote_shell(stderr, operands[0]); fputc('\n', stderr);
        } else fprintf(stderr, "%s: missing operand\n", name);
        fprintf(stderr, "Try '%s --help' for more information.\n", name); free(operands); return 1;
    }
    if (opt.recursive && opt.dereference_set && !opt.traverse_mode_set) {
        fprintf(stderr, "%s: -R --dereference requires either -H or -L\n", name); free(operands); return 1;
    }
    if (from_given) { opt.from_uid_set = from.set_uid; opt.from_gid_set = from.set_gid; opt.from_uid = from.uid; opt.from_gid = from.gid; }
    int status = 0;
    for (int i = first_file; i < count; ++i)
        if (zov_owner_path(name, operands[i], &target, &opt, 1) != 0) status = 1;
    free(operands); return status;
}

#endif
