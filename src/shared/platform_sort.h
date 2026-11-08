#ifndef ZOVUTILS_PLATFORM_SORT_H
#define ZOVUTILS_PLATFORM_SORT_H

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static inline int zov_sort_compare_general_numeric(const unsigned char *a, size_t a_len,
                                                   const unsigned char *b, size_t b_len) {
    char buf_a[128];
    char buf_b[128];
    char *as = (a_len < sizeof(buf_a)) ? buf_a : (char *)malloc(a_len + 1);
    char *bs = (b_len < sizeof(buf_b)) ? buf_b : (char *)malloc(b_len + 1);
    if (!as || !bs) {
        if (as && as != buf_a) free(as);
        if (bs && bs != buf_b) free(bs);
        return 0;
    }
    if (a_len) memcpy(as, a, a_len);
    as[a_len] = '\0';
    if (b_len) memcpy(bs, b, b_len);
    bs[b_len] = '\0';

    char *ae = NULL, *be = NULL;
    double av = strtod(as, &ae);
    double bv = strtod(bs, &be);
    int result = 0;
    if (isnan(av) && !isnan(bv)) result = -1;
    else if (!isnan(av) && isnan(bv)) result = 1;
    else if (av < bv) result = -1;
    else if (av > bv) result = 1;
    else result = 0;

    if (as != buf_a) free(as);
    if (bs != buf_b) free(bs);
    return result;
}

#endif
