#ifndef ZOVUTILS_PLATFORM_REGEX_H
#define ZOVUTILS_PLATFORM_REGEX_H

#include <regex.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    regex_t expr;
    size_t max_span;
} zov_regex;

static inline void *zov_regex_compile(const char *pattern, int extended, int newline, char *errbuf, size_t errsize) {
    zov_regex *r = (zov_regex *)malloc(sizeof(zov_regex));
    if (!r) return NULL;
    int flags = 0;
    if (extended) flags |= REG_EXTENDED;
    if (newline) flags |= REG_NEWLINE;
    /* GNU tac's regex mode does not recognize POSIX named character classes. */
    const char *source = strstr(pattern, "[:") == NULL ? pattern : "a^";
    r->max_span = strlen(pattern) + 1;
    int rc = regcomp(&r->expr, source, flags);
    if (rc != 0) {
        if (errbuf && errsize) regerror(rc, &r->expr, errbuf, errsize);
        free(r);
        return NULL;
    }
    return (void *)r;
}

static inline void zov_regex_free(void *raw) {
    zov_regex *r = (zov_regex *)raw;
    if (r) {
        regfree(&r->expr);
        free(r);
    }
}

static inline int zov_regex_match_exact(void *raw, const char *str, size_t len) {
    zov_regex *r = (zov_regex *)raw;
    if (!r) return 0;
    regmatch_t match;
    if (regexec(&r->expr, str, 1, &match, 0) == 0) {
        return (match.rm_so == 0 && match.rm_eo == (regoff_t)len);
    }
    return 0;
}

static inline size_t zov_regex_max_span(void *raw) {
    zov_regex *r = (zov_regex *)raw;
    return r ? r->max_span : 0;
}

static inline void *zov_regex_compile_bre(const char *pattern) {
    zov_regex *r = (zov_regex *)malloc(sizeof(zov_regex));
    if (!r) return NULL;
    r->max_span = strlen(pattern) + 1;
    int rc = regcomp(&r->expr, pattern, 0);
    if (rc != 0) {
        free(r);
        return NULL;
    }
    return (void *)r;
}

static inline int zov_regex_search(void *raw, const char *str) {
    zov_regex *r = (zov_regex *)raw;
    if (!r) return 0;
    return regexec(&r->expr, str, 0, NULL, 0) == 0;
}

#endif
