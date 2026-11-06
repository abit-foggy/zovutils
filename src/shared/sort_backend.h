#ifndef ZOVUTILS_SORT_BACKEND_H
#define ZOVUTILS_SORT_BACKEND_H

#include <ctype.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned char *data;
    size_t length;
    size_t capacity;
    size_t ordinal;
} zov_sort_line;

static int zov_sort_reverse;
static int zov_sort_unique;
static int zov_sort_ignore_blanks;
static int zov_sort_dictionary;
static int zov_sort_fold;
static int zov_sort_numeric;
static int zov_sort_general_numeric;
static int zov_sort_month;
static int zov_sort_stable;
static int zov_sort_ignore_nonprinting;
static const char *zov_sort_keydef;
static int zov_sort_field_separator;

static inline int zov_sort_blank(unsigned char c) { return c == ' ' || c == '\t'; }

static inline size_t zov_sort_field_start(const zov_sort_line *line, size_t field,
                                          int separator) {
    size_t i = 0;
    size_t current = 1;
    if (separator == 0) {
        while (i < line->length && zov_sort_blank(line->data[i])) ++i;
    }
    while (current < field && i < line->length) {
        if (separator == 0) {
            while (i < line->length && !zov_sort_blank(line->data[i])) ++i;
            while (i < line->length && zov_sort_blank(line->data[i])) ++i;
        } else {
            while (i < line->length && line->data[i] != (unsigned char)separator) ++i;
            if (i < line->length) ++i;
        }
        ++current;
    }
    return i;
}

static inline size_t zov_sort_field_end(const zov_sort_line *line, size_t field,
                                        int separator) {
    size_t i = zov_sort_field_start(line, field, separator);
    if (separator == 0) {
        while (i < line->length && !zov_sort_blank(line->data[i])) ++i;
    } else {
        while (i < line->length && line->data[i] != (unsigned char)separator) ++i;
    }
    return i;
}

static inline size_t zov_sort_number(const char **p, size_t fallback) {
    if (**p < '0' || **p > '9') return fallback;
    size_t value = 0;
    while (**p >= '0' && **p <= '9') {
        unsigned digit = (unsigned)(**p - '0');
        if (value > (SIZE_MAX - digit) / 10) return fallback;
        value = value * 10 + digit;
        ++*p;
    }
    return value;
}

static inline void zov_sort_key(const zov_sort_line *line,
                                zov_sort_line *view) {
    *view = *line;
    if (zov_sort_keydef == NULL || *zov_sort_keydef == '\0') return;
    const char *p = zov_sort_keydef;
    size_t start_field = zov_sort_number(&p, 1);
    size_t start_char = 1;
    if (*p == '.') { ++p; start_char = zov_sort_number(&p, 1); }
    size_t start = zov_sort_field_start(line, start_field, zov_sort_field_separator);
    size_t field_end = zov_sort_field_end(line, start_field, zov_sort_field_separator);
    if (start_char > 1 && start <= field_end) {
        size_t offset = start_char - 1;
        start = offset > field_end - start ? field_end : start + offset;
    }
    size_t end = line->length;
    if (*p == ',') {
        ++p;
        size_t end_field = zov_sort_number(&p, start_field);
        size_t end_char = 0;
        if (*p == '.') { ++p; end_char = zov_sort_number(&p, 0); }
        end = zov_sort_field_end(line, end_field, zov_sort_field_separator);
        if (end_char > 0) {
            size_t end_start = zov_sort_field_start(line, end_field, zov_sort_field_separator);
            size_t offset = end_char;
            end = offset > end - end_start ? end : end_start + offset;
        }
    }
    if (end < start) end = start;
    view->data = line->data + start;
    view->length = end - start;
}

static inline int zov_sort_month_value(const unsigned char *s, size_t n) {
    static const char *months[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                    "jul", "aug", "sep", "oct", "nov", "dec"};
    if (n < 3) return 0;
    for (int i = 0; i < 3; ++i) {
        int a = tolower(s[i]);
        for (int m = 0; m < 12; ++m) {
            if (a == months[m][i] &&
                tolower(s[1]) == months[m][1] &&
                tolower(s[2]) == months[m][2]) return m + 1;
        }
    }
    return 0;
}

static inline int zov_sort_compare_text(const zov_sort_line *a,
                                       const zov_sort_line *b) {
    size_t ai = 0, bi = 0;
    if (zov_sort_ignore_blanks) {
        while (ai < a->length && (a->data[ai] == ' ' || a->data[ai] == '\t')) ++ai;
        while (bi < b->length && (b->data[bi] == ' ' || b->data[bi] == '\t')) ++bi;
    }
    for (;;) {
        while (ai < a->length && zov_sort_dictionary &&
               !isalnum(a->data[ai]) && a->data[ai] != ' ' && a->data[ai] != '\t') ++ai;
        while (bi < b->length && zov_sort_dictionary &&
               !isalnum(b->data[bi]) && b->data[bi] != ' ' && b->data[bi] != '\t') ++bi;
        while (ai < a->length && zov_sort_ignore_nonprinting && !isprint(a->data[ai])) ++ai;
        while (bi < b->length && zov_sort_ignore_nonprinting && !isprint(b->data[bi])) ++bi;
        if (ai == a->length || bi == b->length) break;
        int ac = a->data[ai++], bc = b->data[bi++];
        if (zov_sort_fold) { ac = tolower((unsigned char)ac); bc = tolower((unsigned char)bc); }
        if (ac != bc) return ac < bc ? -1 : 1;
    }
    if (ai == a->length && bi == b->length) return 0;
    return ai == a->length ? -1 : 1;
}

static inline int zov_sort_primary(const zov_sort_line *a,
                                   const zov_sort_line *b) {
    zov_sort_line av, bv;
    zov_sort_key(a, &av);
    zov_sort_key(b, &bv);
    a = &av; b = &bv;
    int result = 0;
    if (zov_sort_numeric || zov_sort_general_numeric) {
        char *as = (char *)malloc(a->length + 1);
        char *bs = (char *)malloc(b->length + 1);
        if (as == NULL || bs == NULL) {
            free(as); free(bs);
            result = 0;
        } else {
            memcpy(as, a->data, a->length); as[a->length] = '\0';
            memcpy(bs, b->data, b->length); bs[b->length] = '\0';
            char *ae = NULL, *be = NULL;
            double av = strtod(as, &ae), bv = strtod(bs, &be);
            if (isnan(av) && !isnan(bv)) result = -1;
            else if (!isnan(av) && isnan(bv)) result = 1;
            else if (av < bv) result = -1;
            else if (av > bv) result = 1;
            else result = 0;
            free(as); free(bs);
        }
    } else if (zov_sort_month) {
        int av = zov_sort_month_value(a->data, a->length);
        int bv = zov_sort_month_value(b->data, b->length);
        result = av < bv ? -1 : av > bv ? 1 : 0;
    } else {
        result = zov_sort_compare_text(a, b);
    }
    return result;
}

static inline int zov_sort_compare(const void *left, const void *right) {
    const zov_sort_line *a = (const zov_sort_line *)left;
    const zov_sort_line *b = (const zov_sort_line *)right;
    int result = zov_sort_primary(a, b);
    if (result == 0 && !zov_sort_stable && !zov_sort_unique) {
        size_t n = a->length < b->length ? a->length : b->length;
        result = n ? memcmp(a->data, b->data, n) : 0;
        if (result == 0) result = a->length < b->length ? -1 : a->length > b->length ? 1 : 0;
        if (result < 0) result = -1;
        else if (result > 0) result = 1;
    }
    if (zov_sort_reverse) result = -result;
    if (result == 0 && (zov_sort_stable || zov_sort_unique))
        result = a->ordinal < b->ordinal ? -1 : a->ordinal > b->ordinal ? 1 : 0;
    return result;
}

static inline int zov_sort_add_line(zov_sort_line **lines, size_t *count,
                                    size_t *capacity, const unsigned char *data,
                                    size_t length) {
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 128;
        if (next < *capacity || next > SIZE_MAX / sizeof(**lines)) return 0;
        zov_sort_line *grown = (zov_sort_line *)realloc(*lines, next * sizeof(**lines));
        if (grown == NULL) return 0;
        *lines = grown;
        *capacity = next;
    }
    unsigned char *copy = (unsigned char *)malloc(length + 1);
    if (copy == NULL) return 0;
    if (length) memcpy(copy, data, length);
    copy[length] = 0;
    (*lines)[*count].data = copy;
    (*lines)[*count].length = length;
    (*lines)[*count].capacity = length + 1;
    (*lines)[*count].ordinal = *count;
    ++*count;
    return 1;
}

static inline int zov_sort_read(FILE *fp, unsigned char term,
                                zov_sort_line **lines, size_t *count,
                                size_t *line_capacity, unsigned char **buffer,
                                size_t *length, size_t *capacity) {
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if ((unsigned char)c == term) {
            int ok = zov_sort_add_line(lines, count, line_capacity, *buffer, *length);
            *length = 0;
            return ok ? 1 : -1;
        }
        if (*length == *capacity) {
            size_t next = *capacity ? *capacity * 2 : 4096;
            if (next < *capacity) return -1;
            unsigned char *grown = (unsigned char *)realloc(*buffer, next);
            if (grown == NULL) return -1;
            *buffer = grown; *capacity = next;
        }
        (*buffer)[(*length)++] = (unsigned char)c;
    }
    if (ferror(fp)) return -1;
    if (*length > 0) {
        int ok = zov_sort_add_line(lines, count, line_capacity, *buffer, *length);
        *length = 0;
        return ok ? 1 : -1;
    }
    return 0;
}

static inline int zov_sort_run(size_t file_count, const char *const *paths,
                               int reverse, int unique, int ignore_blanks,
                               int dictionary, int fold, int numeric,
                               int general_numeric, int month, int stable,
                               int zero_terminated, const char *keydef,
                               int field_separator, int ignore_nonprinting) {
    (void)setlocale(LC_CTYPE, "");
    zov_sort_reverse = reverse; zov_sort_unique = unique;
    zov_sort_ignore_blanks = ignore_blanks; zov_sort_dictionary = dictionary;
    zov_sort_fold = fold; zov_sort_numeric = numeric;
    zov_sort_general_numeric = general_numeric; zov_sort_month = month;
    zov_sort_stable = stable;
    zov_sort_keydef = keydef;
    zov_sort_field_separator = field_separator;
    zov_sort_ignore_nonprinting = ignore_nonprinting;

    zov_sort_line *lines = NULL;
    size_t count = 0, line_capacity = 0;
    unsigned char *buffer = NULL;
    size_t length = 0, capacity = 0;
    int status = 0;
    unsigned char term = zero_terminated ? 0 : '\n';
    for (size_t f = 0; f < file_count; ++f) {
        FILE *fp = strcmp(paths[f], "-") == 0 ? stdin : fopen(paths[f], "rb");
        if (fp == NULL) { fprintf(stderr, "sort: %s: %s\n", paths[f], strerror(errno)); status = 2; continue; }
        int close_fp = fp != stdin;
        int rc;
        do { rc = zov_sort_read(fp, term, &lines, &count, &line_capacity,
                                &buffer, &length, &capacity); }
        while (rc == 1);
        if (rc < 0) { fprintf(stderr, "sort: read failed: %s\n", paths[f]); status = 2; }
        if (close_fp) fclose(fp);
    }
    free(buffer);
    qsort(lines, count, sizeof(*lines), zov_sort_compare);
    for (size_t i = 0; i < count; ++i) {
        if (zov_sort_unique && i > 0 && zov_sort_primary(&lines[i - 1], &lines[i]) == 0) continue;
        if (lines[i].length && fwrite(lines[i].data, 1, lines[i].length, stdout) != lines[i].length) { status = 2; break; }
        if (fwrite(&term, 1, 1, stdout) != 1) { status = 2; break; }
    }
    for (size_t i = 0; i < count; ++i) free(lines[i].data);
    free(lines);
    if (fflush(stdout) != 0) status = 2;
    return status;
}

#endif
