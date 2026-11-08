#ifndef ZOVUTILS_PLATFORM_REGEX_H
#define ZOVUTILS_PLATFORM_REGEX_H

#include <errno.h>
#include <regex.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
    return 1; // matched length
}

#endif
