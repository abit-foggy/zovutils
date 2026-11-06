#ifndef ZOVUTILS_JOIN_BACKEND_H
#define ZOVUTILS_JOIN_BACKEND_H

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { unsigned char *data; size_t length; } zov_join_record;
typedef struct { zov_join_record *records; size_t count; size_t capacity; } zov_join_input;
typedef struct { size_t start; size_t length; } zov_join_field;

static inline int zov_join_push_record(zov_join_input *input,
                                       unsigned char *data, size_t length) {
    if (input->count == input->capacity) {
        size_t capacity = input->capacity == 0 ? 64 : input->capacity * 2;
        if (capacity < input->capacity || capacity > SIZE_MAX / sizeof(*input->records)) return 0;
        zov_join_record *records = (zov_join_record *)realloc(input->records,
                                                             capacity * sizeof(*records));
        if (records == NULL) return 0;
        input->records = records;
        input->capacity = capacity;
    }
    input->records[input->count].data = data;
    input->records[input->count].length = length;
    ++input->count;
    return 1;
}

static inline int zov_join_read(FILE *fp, unsigned char terminator,
                                zov_join_input *input) {
    unsigned char *line = NULL;
    size_t length = 0, capacity = 0;
    for (;;) {
        int c = fgetc(fp);
        if (c == EOF || c == terminator) {
            if (c == EOF && ferror(fp)) { free(line); return 0; }
            if (length != 0 || c == terminator) {
                if (!zov_join_push_record(input, line, length)) { free(line); return 0; }
                line = NULL; length = 0; capacity = 0;
            }
            if (c == EOF) break;
            continue;
        }
        if (length == capacity) {
            size_t next = capacity == 0 ? 256 : capacity * 2;
            if (next < capacity) { free(line); return 0; }
            unsigned char *grown = (unsigned char *)realloc(line, next);
            if (grown == NULL) { free(line); return 0; }
            line = grown; capacity = next;
        }
        line[length++] = (unsigned char)c;
    }
    free(line);
    return 1;
}

static inline void zov_join_free(zov_join_input *input) {
    for (size_t i = 0; i < input->count; ++i) free(input->records[i].data);
    free(input->records);
    memset(input, 0, sizeof(*input));
}

static inline size_t zov_join_fields(const zov_join_record *record,
                                     int delimiter, zov_join_field *fields,
                                     size_t capacity) {
    size_t count = 0, i = 0;
    if (delimiter >= 0) {
        for (;;) {
            size_t start = i;
            while (i < record->length && record->data[i] != (unsigned char)delimiter) ++i;
            if (count < capacity) { fields[count].start = start; fields[count].length = i - start; }
            ++count;
            if (i == record->length) break;
            ++i;
        }
    } else {
        while (i < record->length) {
            while (i < record->length && (record->data[i] == ' ' || record->data[i] == '\t')) ++i;
            if (i == record->length) break;
            size_t start = i;
            while (i < record->length && record->data[i] != ' ' && record->data[i] != '\t') ++i;
            if (count < capacity) { fields[count].start = start; fields[count].length = i - start; }
            ++count;
        }
    }
    return count;
}

static inline int zov_join_compare(const zov_join_record *a,
                                   const zov_join_field *af, size_t ac,
                                   size_t a_field,
                                   const zov_join_record *b,
                                   const zov_join_field *bf, size_t bc,
                                   size_t b_field, int ignore_case) {
    size_t ai = a_field <= ac ? af[a_field - 1].start : a->length;
    size_t al = a_field <= ac ? af[a_field - 1].length : 0;
    size_t bi = b_field <= bc ? bf[b_field - 1].start : b->length;
    size_t bl = b_field <= bc ? bf[b_field - 1].length : 0;
    size_t n = al < bl ? al : bl;
    for (size_t i = 0; i < n; ++i) {
        unsigned char x = a->data[ai + i], y = b->data[bi + i];
        if (ignore_case) { x = (unsigned char)tolower(x); y = (unsigned char)tolower(y); }
        if (x != y) return x < y ? -1 : 1;
    }
    return al < bl ? -1 : al > bl ? 1 : 0;
}

static inline int zov_join_write_field(const zov_join_record *record,
                                       const zov_join_field *fields,
                                       size_t count, size_t field_number,
                                       const char *empty_replacement,
                                       int use_empty) {
    if (field_number == 0 || field_number > count) {
        if (use_empty && *empty_replacement != '\0')
            return fwrite(empty_replacement, 1, strlen(empty_replacement), stdout) == strlen(empty_replacement);
        return 1;
    }
    size_t start = fields[field_number - 1].start;
    size_t length = fields[field_number - 1].length;
    if (length == 0 && use_empty && *empty_replacement != '\0') {
        size_t n = strlen(empty_replacement);
        return fwrite(empty_replacement, 1, n, stdout) == n;
    }
    return length == 0 || fwrite(record->data + start, 1, length, stdout) == length;
}

static inline int zov_join_write_output(const zov_join_record *a,
                                        const zov_join_field *af, size_t ac,
                                        const zov_join_record *b,
                                        const zov_join_field *bf, size_t bc,
                                        size_t field1, size_t field2,
                                        const char *format, int delimiter,
                                        unsigned char separator, unsigned char terminator,
                                        const char *empty_replacement,
                                        int use_empty, int matched) {
    int first = 1;
    if (format != NULL) {
        size_t p = 0, n = strlen(format);
        while (p < n) {
            while (p < n && (format[p] == ',' || format[p] == ' ' || format[p] == '\t')) ++p;
            if (p == n) break;
            size_t q = p;
            while (q < n && format[q] != ',' && format[q] != ' ' && format[q] != '\t') ++q;
            if (!first && fputc(separator, stdout) == EOF) return 0;
            if (q - p == 1 && format[p] == '0') {
                const zov_join_record *rec = matched || a != NULL ? a : b;
                const zov_join_field *fields = matched || a != NULL ? af : bf;
                size_t count = matched || a != NULL ? ac : bc;
                size_t field = matched || a != NULL ? field1 : field2;
                if (!zov_join_write_field(rec, fields, count, field, empty_replacement, use_empty)) return 0;
            } else {
                size_t file = 0, field = 0;
                size_t dot = p;
                while (dot < q && format[dot] != '.') ++dot;
                if (dot == p || dot + 1 >= q) return 0;
                for (size_t k = p; k < dot; ++k) {
                    if (format[k] < '0' || format[k] > '9') return 0;
                    file = file * 10 + (size_t)(format[k] - '0');
                }
                for (size_t k = dot + 1; k < q; ++k) {
                    if (format[k] < '0' || format[k] > '9') return 0;
                    field = field * 10 + (size_t)(format[k] - '0');
                }
                const zov_join_record *rec = file == 1 ? a : b;
                const zov_join_field *fields = file == 1 ? af : bf;
                size_t count = file == 1 ? ac : bc;
                if (rec == NULL) {
                    if (use_empty && *empty_replacement != '\0') {
                        size_t replacement_length = strlen(empty_replacement);
                        if (fwrite(empty_replacement, 1, replacement_length, stdout) != replacement_length) return 0;
                    }
                } else if (!zov_join_write_field(rec, fields, count, field,
                                                 empty_replacement, use_empty)) return 0;
            }
            first = 0; p = q;
        }
    } else {
        const zov_join_record *key_record = a != NULL ? a : b;
        const zov_join_field *key_fields = a != NULL ? af : bf;
        size_t key_count = a != NULL ? ac : bc;
        size_t key_number = a != NULL ? field1 : field2;
        if (!zov_join_write_field(key_record, key_fields, key_count, key_number,
                                  empty_replacement, use_empty)) return 0;
        first = 0;
        if (a != NULL) for (size_t i = 1; i <= ac; ++i) if (i != field1) {
            if (fputc(separator, stdout) == EOF || !zov_join_write_field(a, af, ac, i, empty_replacement, use_empty)) return 0;
        }
        if (b != NULL) for (size_t i = 1; i <= bc; ++i) if (i != field2) {
            if (fputc(separator, stdout) == EOF || !zov_join_write_field(b, bf, bc, i, empty_replacement, use_empty)) return 0;
        }
    }
    (void)delimiter;
    (void)first;
    return fputc(terminator, stdout) != EOF;
}

static inline int zov_join_run(const char *path1, const char *path2,
                               size_t field1, size_t field2, int delimiter,
                               unsigned char separator, unsigned char terminator,
                               int ignore_case, int unpaired1, int unpaired2,
                               int suppress_joined, int header, int check_order,
                               const char *empty_replacement, const char *format) {
    if (format != NULL && *format == '\0') format = NULL;
    (void)setlocale(LC_ALL, "");
    zov_join_input a = {0}, b = {0};
    FILE *fa = strcmp(path1, "-") == 0 ? stdin : fopen(path1, "rb");
    if (fa == NULL) { fprintf(stderr, "join: %s: %s\n", path1, strerror(errno)); return 1; }
    FILE *fb = strcmp(path2, "-") == 0 ? stdin : fopen(path2, "rb");
    if (fb == NULL) { if (fa != stdin) fclose(fa); fprintf(stderr, "join: %s: %s\n", path2, strerror(errno)); return 1; }
    int ok1 = zov_join_read(fa, terminator, &a);
    int ok2 = zov_join_read(fb, terminator, &b);
    if (fa != stdin) fclose(fa);
    if (fb != stdin) fclose(fb);
    if (!ok1 || !ok2) { fprintf(stderr, "join: memory exhausted\n"); zov_join_free(&a); zov_join_free(&b); return 1; }

    int status = 0;
    size_t start_a = 0, start_b = 0;
    if (header && a.count != 0 && b.count != 0) {
        zov_join_field af[4096], bf[4096];
        size_t ac = zov_join_fields(&a.records[0], delimiter, af, 4096);
        size_t bc = zov_join_fields(&b.records[0], delimiter, bf, 4096);
        if (!zov_join_write_output(&a.records[0], af, ac, &b.records[0], bf, bc,
                field1, field2, format, delimiter, separator, terminator,
                empty_replacement, *empty_replacement != '\0', 1)) status = 1;
        start_a = 1; start_b = 1;
    }
    size_t i = start_a, j = start_b;
    zov_join_field *fields_a = (zov_join_field *)malloc(4096 * sizeof(*fields_a));
    zov_join_field *fields_b = (zov_join_field *)malloc(4096 * sizeof(*fields_b));
    if (fields_a == NULL || fields_b == NULL) { free(fields_a); free(fields_b); zov_join_free(&a); zov_join_free(&b); return 1; }
    if (check_order) {
        zov_join_field previous_fields[4096], current_fields[4096];
        for (size_t k = start_a + 1; k < a.count; ++k) {
            size_t pn = zov_join_fields(&a.records[k - 1], delimiter, previous_fields, 4096);
            size_t cn = zov_join_fields(&a.records[k], delimiter, current_fields, 4096);
            if (pn > 4096 || cn > 4096 || zov_join_compare(&a.records[k - 1], previous_fields, pn, field1,
                    &a.records[k], current_fields, cn, field1, ignore_case) > 0) {
                fprintf(stderr, "join: %s:%zu: is not sorted: ", path1, k + 1);
                fwrite(a.records[k].data, 1, a.records[k].length, stderr); fputc('\n', stderr);
                free(fields_a); free(fields_b); zov_join_free(&a); zov_join_free(&b); return 1;
            }
        }
        for (size_t k = start_b + 1; k < b.count; ++k) {
            size_t pn = zov_join_fields(&b.records[k - 1], delimiter, previous_fields, 4096);
            size_t cn = zov_join_fields(&b.records[k], delimiter, current_fields, 4096);
            if (pn > 4096 || cn > 4096 || zov_join_compare(&b.records[k - 1], previous_fields, pn, field2,
                    &b.records[k], current_fields, cn, field2, ignore_case) > 0) {
                fprintf(stderr, "join: %s:%zu: is not sorted: ", path2, k + 1);
                fwrite(b.records[k].data, 1, b.records[k].length, stderr); fputc('\n', stderr);
                free(fields_a); free(fields_b); zov_join_free(&a); zov_join_free(&b); return 1;
            }
        }
    }
    while (i < a.count && j < b.count) {
        zov_join_record *ra = &a.records[i], *rb = &b.records[j];
        size_t ac = zov_join_fields(ra, delimiter, fields_a, 4096);
        size_t bc = zov_join_fields(rb, delimiter, fields_b, 4096);
        if (ac > 4096 || bc > 4096) { status = 1; break; }
        int cmp = zov_join_compare(ra, fields_a, ac, field1, rb, fields_b, bc, field2, ignore_case);
        if (cmp < 0) {
            if (unpaired1 && !zov_join_write_output(ra, fields_a, ac, NULL, NULL, 0,
                    field1, field2, format, delimiter, separator, terminator, empty_replacement,
                    *empty_replacement != '\0', 0)) status = 1;
            ++i; continue;
        }
        if (cmp > 0) {
            if (unpaired2 && !zov_join_write_output(NULL, NULL, 0, rb, fields_b, bc,
                    field1, field2, format, delimiter, separator, terminator, empty_replacement,
                    *empty_replacement != '\0', 0)) status = 1;
            ++j; continue;
        }
        size_t end_a = i + 1, end_b = j + 1;
        while (end_a < a.count) {
            zov_join_field f[4096]; size_t n = zov_join_fields(&a.records[end_a], delimiter, f, 4096);
            if (n > 4096) { status = 1; break; }
            if (zov_join_compare(ra, fields_a, ac, field1, &a.records[end_a], f, n, field1, ignore_case) != 0) break;
            ++end_a;
        }
        while (end_b < b.count) {
            zov_join_field f[4096]; size_t n = zov_join_fields(&b.records[end_b], delimiter, f, 4096);
            if (n > 4096) { status = 1; break; }
            if (zov_join_compare(rb, fields_b, bc, field2, &b.records[end_b], f, n, field2, ignore_case) != 0) break;
            ++end_b;
        }
        if (!suppress_joined) for (size_t x = i; x < end_a; ++x) for (size_t y = j; y < end_b; ++y) {
            zov_join_field xf[4096], yf[4096];
            size_t xc = zov_join_fields(&a.records[x], delimiter, xf, 4096);
            size_t yc = zov_join_fields(&b.records[y], delimiter, yf, 4096);
            if (!zov_join_write_output(&a.records[x], xf, xc, &b.records[y], yf, yc,
                    field1, field2, format, delimiter, separator, terminator,
                    empty_replacement, *empty_replacement != '\0', 1)) status = 1;
        }
        i = end_a; j = end_b;
    }
    while (i < a.count) {
        size_t ac = zov_join_fields(&a.records[i], delimiter, fields_a, 4096);
        if (unpaired1 && !zov_join_write_output(&a.records[i], fields_a, ac, NULL, NULL, 0,
                field1, field2, format, delimiter, separator, terminator, empty_replacement,
                *empty_replacement != '\0', 0)) status = 1;
        ++i;
    }
    while (j < b.count) {
        size_t bc = zov_join_fields(&b.records[j], delimiter, fields_b, 4096);
        if (unpaired2 && !zov_join_write_output(NULL, NULL, 0, &b.records[j], fields_b, bc,
                field1, field2, format, delimiter, separator, terminator, empty_replacement,
                *empty_replacement != '\0', 0)) status = 1;
        ++j;
    }
    free(fields_a); free(fields_b);
    zov_join_free(&a); zov_join_free(&b);
    if (fflush(stdout) != 0) status = 1;
    return status;
}

#endif
