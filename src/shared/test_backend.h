#ifndef ZOVUTILS_TEST_BACKEND_H
#define ZOVUTILS_TEST_BACKEND_H

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static inline int zov_test_file(const char *op, const char *path, const char *extra) {
    struct stat st;
    int rc = stat(path, &st);
    if (strcmp(op, "-L") == 0 || strcmp(op, "-h") == 0) {
        return lstat(path, &st) == 0 && S_ISLNK(st.st_mode);
    }
    if (strcmp(op, "-e") == 0 || strcmp(op, "-a") == 0) return rc == 0;
    if (strcmp(op, "-f") == 0) return rc == 0 && S_ISREG(st.st_mode);
    if (strcmp(op, "-d") == 0) return rc == 0 && S_ISDIR(st.st_mode);
    if (strcmp(op, "-b") == 0) return rc == 0 && S_ISBLK(st.st_mode);
    if (strcmp(op, "-c") == 0) return rc == 0 && S_ISCHR(st.st_mode);
    if (strcmp(op, "-p") == 0) return rc == 0 && S_ISFIFO(st.st_mode);
    if (strcmp(op, "-S") == 0) return rc == 0 && S_ISSOCK(st.st_mode);
    if (strcmp(op, "-r") == 0) return access(path, R_OK) == 0;
    if (strcmp(op, "-w") == 0) return access(path, W_OK) == 0;
    if (strcmp(op, "-x") == 0) return access(path, X_OK) == 0;
    if (strcmp(op, "-s") == 0) return rc == 0 && st.st_size > 0;
    if (strcmp(op, "-O") == 0) return rc == 0 && st.st_uid == getuid();
    if (strcmp(op, "-G") == 0) return rc == 0 && st.st_gid == getgid();
    if (strcmp(op, "-t") == 0) return isatty(atoi(path));
    if (strcmp(op, "-N") == 0) return rc == 0 && st.st_mtime > st.st_atime;
    if (strcmp(op, "-ef") == 0 || strcmp(op, "-nt") == 0 || strcmp(op, "-ot") == 0) {
        struct stat other;
        if (rc != 0 || stat(extra, &other) != 0) return 0;
        if (strcmp(op, "-ef") == 0)
            return st.st_dev == other.st_dev && st.st_ino == other.st_ino;
        if (strcmp(op, "-nt") == 0) return st.st_mtime > other.st_mtime;
        return st.st_mtime < other.st_mtime;
    }
    return 0;
}

static inline int zov_test_integer(const char *op, const char *a, const char *b,
                                   int *valid) {
    char *ea = NULL, *eb = NULL;
    errno = 0;
    long long x = strtoll(a, &ea, 10);
    int bad_a = errno == ERANGE || ea == a || *ea != '\0';
    errno = 0;
    long long y = strtoll(b, &eb, 10);
    int bad_b = errno == ERANGE || eb == b || *eb != '\0';
    if (bad_a || bad_b) { *valid = bad_a ? -1 : -2; return 0; }
    *valid = 1;
    if (strcmp(op, "-eq") == 0) return x == y;
    if (strcmp(op, "-ne") == 0) return x != y;
    if (strcmp(op, "-gt") == 0) return x > y;
    if (strcmp(op, "-ge") == 0) return x >= y;
    if (strcmp(op, "-lt") == 0) return x < y;
    return x <= y;
}

static inline int zov_test_string(const char *op, const char *a, const char *b) {
    int cmp = strcmp(a, b);
    if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) return cmp == 0;
    if (strcmp(op, "!=") == 0) return cmp != 0;
    if (strcmp(op, "<") == 0) return cmp < 0;
    return cmp > 0;
}

#endif
