#ifndef ZOVUTILS_PLATFORM_REGEX_H
#define ZOVUTILS_PLATFORM_REGEX_H

#include <errno.h>
#include <regex.h>
#include <stdint.h>
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

static inline int zov_regex_match(
    const char *pattern,
    const char *text,
    int64_t *matched_len,
    char *submatch_buf,
    size_t submatch_sz
) {
    regex_t rx;
    if (regcomp(&rx, pattern, 0) != 0) {
        return -1;
    }
    regmatch_t m[2];
    int rc = regexec(&rx, text, 2, m, 0);
    regfree(&rx);
    if (rc != 0) {
        return 0; // no match
    }
    if (m[1].rm_so >= 0 && submatch_buf && submatch_sz > 0) {
        size_t n = (size_t)(m[1].rm_eo - m[1].rm_so);
        if (n >= submatch_sz) n = submatch_sz - 1;
        memcpy(submatch_buf, text + m[1].rm_so, n);
        submatch_buf[n] = '\0';
        return 2; // matched group 1
    }
    if (matched_len) {
        *matched_len = (int64_t)m[0].rm_eo;
    }
    return 1;
}

static inline int zov_regex_line_match(const char *pattern, const char *line, size_t length) {
    char *text = (char *)malloc(length + 1);
    if (!text) return -1;
    memcpy(text, line, length);
    text[length] = '\0';
    regex_t regex;
    int rc = regcomp(&regex, pattern, 0);
    if (rc != 0) {
        free(text);
        return -2;
    }
    rc = regexec(&regex, text, 0, NULL, 0);
    regfree(&regex);
    free(text);
    return rc == 0 ? 1 : rc == REG_NOMATCH ? 0 : -2;
}

static inline int zov_csplit_format_suffix(const char *format, unsigned digits, size_t index, char *suffix_out, size_t suffix_sz) {
    if (format && *format) {
        int is_unsigned = 0;
        unsigned conversions = 0;
        for (const char *p = format; *p; ++p) {
            if (*p != '%') continue;
            ++p;
            if (*p == '%') continue;
            while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') ++p;
            while (*p >= '0' && *p <= '9') ++p;
            if (*p == '.') { ++p; while (*p >= '0' && *p <= '9') ++p; }
            if (*p == 'd' || *p == 'i') { is_unsigned = 0; ++conversions; }
            else if (*p == 'u' || *p == 'o' || *p == 'x' || *p == 'X') { is_unsigned = 1; ++conversions; }
            else return 0;
        }
        if (conversions != 1) return 0;
        int n = is_unsigned ? snprintf(suffix_out, suffix_sz, format, (unsigned)index)
                            : snprintf(suffix_out, suffix_sz, format, (int)index);
        return n >= 0 && (size_t)n < suffix_sz;
    }
    int n = snprintf(suffix_out, suffix_sz, "%0*zu", (int)digits, index);
    return n >= 0 && (size_t)n < suffix_sz;
}

static inline void *zov_regex_compile_extended(const char *pattern) {
    regex_t *r = (regex_t *)malloc(sizeof(regex_t));
    if (!r) return NULL;
    if (regcomp(r, pattern, REG_EXTENDED) != 0) {
        free(r);
        return NULL;
    }
    return (void *)r;
}

static inline void zov_regex_free_compiled(void *raw) {
    if (!raw) return;
    regex_t *r = (regex_t *)raw;
    regfree(r);
    free(r);
}

static inline int zov_regex_exec_range(void *raw, const char *str, int64_t *so, int64_t *eo) {
    if (!raw) return 0;
    regex_t *r = (regex_t *)raw;
    regmatch_t m;
    if (regexec(r, str, 1, &m, 0) == 0) {
        if (so) *so = (int64_t)m.rm_so;
        if (eo) *eo = (int64_t)m.rm_eo;
        return 1;
    }
    return 0;
}

#endif
