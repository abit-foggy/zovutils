#ifndef ZOVUTILS_COMM_BACKEND_H
#define ZOVUTILS_COMM_BACKEND_H

#include <errno.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int zov_comm_read(FILE *stream, unsigned char terminator,
                                unsigned char **data, size_t *length) {
    size_t size = 0, capacity = 0;
    unsigned char *buffer = NULL;
    for (;;) {
        int byte = fgetc(stream);
        if (byte == EOF || byte == terminator) {
            if (byte == EOF && ferror(stream)) { free(buffer); return -1; }
            if (byte == EOF && size == 0) { free(buffer); *data = NULL; *length = 0; return 0; }
            *data = buffer; *length = size; return 1;
        }
        if (size == capacity) {
            size_t next = capacity == 0 ? 256 : capacity * 2;
            if (next < capacity) { free(buffer); return -1; }
            unsigned char *grown = (unsigned char *)realloc(buffer, next);
            if (grown == NULL) { free(buffer); return -1; }
            buffer = grown; capacity = next;
        }
        buffer[size++] = (unsigned char)byte;
    }
}

static inline int zov_comm_compare(const unsigned char *a, size_t alen,
                                   const unsigned char *b, size_t blen) {
    size_t n = alen < blen ? alen : blen;
    for (size_t i = 0; i < n; ++i) if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return alen < blen ? -1 : alen > blen ? 1 : 0;
}

static inline int zov_comm_delimit(FILE *out, const char *delimiter,
                                   unsigned count) {
    size_t length = strlen(delimiter);
    if (length == 0) {
        for (unsigned i = 0; i < count; ++i) if (fputc(0, out) == EOF) return 0;
        return 1;
    }
    for (unsigned i = 0; i < count; ++i)
        if (fwrite(delimiter, 1, length, out) != length) return 0;
    return 1;
}

static inline int zov_comm_emit(const unsigned char *data, size_t length,
                                unsigned column, const int enabled[3],
                                const char *delimiter, unsigned char terminator) {
    unsigned leading = 0;
    for (unsigned i = 0; i + 1 < column; ++i) if (enabled[i]) ++leading;
    return zov_comm_delimit(stdout, delimiter, leading) &&
           (length == 0 || fwrite(data, 1, length, stdout) == length) &&
           fputc(terminator, stdout) != EOF;
}

static inline int zov_comm_total(uint64_t one, uint64_t two, uint64_t both,
                                 const char *delimiter, unsigned char terminator) {
    char counts[3][32];
    snprintf(counts[0], sizeof(counts[0]), "%llu", (unsigned long long)one);
    snprintf(counts[1], sizeof(counts[1]), "%llu", (unsigned long long)two);
    snprintf(counts[2], sizeof(counts[2]), "%llu", (unsigned long long)both);
    for (unsigned i = 0; i < 3; ++i) {
        if (i != 0 && !zov_comm_delimit(stdout, delimiter, 1)) return 0;
        size_t length = strlen(counts[i]);
        if (fwrite(counts[i], 1, length, stdout) != length) return 0;
    }
    return zov_comm_delimit(stdout, delimiter, 1) &&
           fwrite("total", 1, 5, stdout) == 5 && fputc(terminator, stdout) != EOF;
}

static inline int zov_comm_run(const char *path1, const char *path2,
                               int col1, int col2, int col3,
                               const char *delimiter, unsigned char terminator,
                               int check_order, int total) {
    (void)setlocale(LC_ALL, "");
    FILE *one = strcmp(path1, "-") == 0 ? stdin : fopen(path1, "rb");
    if (one == NULL) { fprintf(stderr, "comm: %s: %s\n", path1, strerror(errno)); return 1; }
    FILE *two = strcmp(path2, "-") == 0 ? stdin : fopen(path2, "rb");
    if (two == NULL) { if (one != stdin) fclose(one); fprintf(stderr, "comm: %s: %s\n", path2, strerror(errno)); return 1; }

    int enabled[3] = { col1, col2, col3 };
    unsigned char *a = NULL, *b = NULL, *previous_a = NULL, *previous_b = NULL;
    size_t alen = 0, blen = 0, previous_alen = 0, previous_blen = 0;
    uint64_t only_a = 0, only_b = 0, common = 0;
    size_t line_a = 0, line_b = 0;
    int validate_a, validate_b;
    int have_previous_a = 0, have_previous_b = 0;
    int has_a = zov_comm_read(one, terminator, &a, &alen);
    int has_b = zov_comm_read(two, terminator, &b, &blen);
    validate_a = has_a; validate_b = has_b;
    int status = 0;
    if (has_a < 0 || has_b < 0) { fprintf(stderr, "comm: read error\n"); status = 1; goto done; }
    while (has_a || has_b) {
        if (has_a && validate_a) {
            ++line_a;
            validate_a = 0;
            if (check_order && have_previous_a && zov_comm_compare(previous_a, previous_alen, a, alen) > 0) {
                fprintf(stderr, "comm: file 1 is not in sorted order\n"); status = 1; goto done;
            }
        }
        if (has_b && validate_b) {
            ++line_b;
            validate_b = 0;
            if (check_order && have_previous_b && zov_comm_compare(previous_b, previous_blen, b, blen) > 0) {
                fprintf(stderr, "comm: file 2 is not in sorted order\n"); status = 1; goto done;
            }
        }
        int cmp = !has_a ? 1 : !has_b ? -1 : zov_comm_compare(a, alen, b, blen);
        if (cmp < 0) {
            if (enabled[0] && !zov_comm_emit(a, alen, 1, enabled, delimiter, terminator)) { status = 1; goto done; }
            ++only_a;
            free(previous_a); previous_a = a; previous_alen = alen; a = NULL; alen = 0;
            have_previous_a = 1;
            has_a = zov_comm_read(one, terminator, &a, &alen);
            validate_a = has_a;
            if (has_a < 0) { fprintf(stderr, "comm: read error\n"); status = 1; goto done; }
        } else if (cmp > 0) {
            if (enabled[1] && !zov_comm_emit(b, blen, 2, enabled, delimiter, terminator)) { status = 1; goto done; }
            ++only_b;
            free(previous_b); previous_b = b; previous_blen = blen; b = NULL; blen = 0;
            have_previous_b = 1;
            has_b = zov_comm_read(two, terminator, &b, &blen);
            validate_b = has_b;
            if (has_b < 0) { fprintf(stderr, "comm: read error\n"); status = 1; goto done; }
        } else {
            if (enabled[2] && !zov_comm_emit(a, alen, 3, enabled, delimiter, terminator)) { status = 1; goto done; }
            ++common;
            free(previous_a); previous_a = a; previous_alen = alen; a = NULL; alen = 0;
            free(previous_b); previous_b = b; previous_blen = blen; b = NULL; blen = 0;
            have_previous_a = have_previous_b = 1;
            has_a = zov_comm_read(one, terminator, &a, &alen);
            has_b = zov_comm_read(two, terminator, &b, &blen);
            validate_a = has_a; validate_b = has_b;
            if (has_a < 0 || has_b < 0) { fprintf(stderr, "comm: read error\n"); status = 1; goto done; }
        }
    }
    if (total && !zov_comm_total(only_a, only_b, common, delimiter, terminator)) status = 1;
    if (fflush(stdout) != 0) status = 1;
    if (strcmp(path1, "-") == 0 && strcmp(path2, "-") == 0) {
        fprintf(stderr, "comm: -\n"); status = 1;
    }
done:
    free(a); free(b); free(previous_a); free(previous_b);
    if (one != stdin) fclose(one);
    if (two != stdin) fclose(two);
    return status;
}

#endif
