#ifndef ZOVUTILS_CUT_BACKEND_H
#define ZOVUTILS_CUT_BACKEND_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int zov_cut_parse_uint(const char **cursor, uint64_t *value) {
    const char *p = *cursor;
    if (*p < '0' || *p > '9') return 0;
    uint64_t n = 0;
    do {
        unsigned digit = (unsigned)(*p - '0');
        if (n > (UINT64_MAX - digit) / 10) return -1;
        n = n * 10 + digit;
        ++p;
    } while (*p >= '0' && *p <= '9');
    *cursor = p;
    *value = n;
    return 1;
}

static inline int zov_cut_validate_list(const char *list) {
    const char *p = list;
    if (*p == '\0') return 2;
    while (*p) {
        uint64_t first = 0, last = 0;
        int have_first = zov_cut_parse_uint(&p, &first);
        if (have_first < 0) return 2;
        if (*p == '-') {
            ++p;
            int have_last = zov_cut_parse_uint(&p, &last);
            if (have_last < 0 || (!have_first && !have_last)) return 0;
            if (have_first && first == 0) return 2;
            if (have_last && last == 0) return 2;
            if (have_first && have_last && last < first) return 3;
        } else {
            if (!have_first || first == 0) return 2;
        }
        if (*p == '\0') return 1;
        if (*p != ',') return 4;
        ++p;
        if (*p == '\0') return 2;
    }
    return 1;
}

static inline int zov_cut_selected(const char *list, uint64_t position) {
    const char *p = list;
    while (*p) {
        uint64_t first = 0, last = UINT64_MAX;
        int have_first = zov_cut_parse_uint(&p, &first);
        if (*p == '-') {
            ++p;
            int have_last = zov_cut_parse_uint(&p, &last);
            if (!have_first) first = 1;
            if (!have_last) last = UINT64_MAX;
        } else {
            last = first;
        }
        if (position >= first && position <= last) return 1;
        if (*p == ',') ++p;
    }
    return 0;
}

static inline int zov_cut_write(FILE *out, const void *data, size_t length) {
    return length == 0 || fwrite(data, 1, length, out) == length;
}

static inline int zov_cut_emit_line(FILE *out, const unsigned char *line,
                                    size_t length, int has_terminator,
                                    unsigned char terminator, const char *list,
                                    int mode, unsigned char delimiter,
                                    const char *output_delimiter, int has_output_delimiter,
                                    int complement, int suppress) {
    int wrote = 0;
    if (mode != 2) {
        for (size_t i = 0; i < length; ++i) {
            int selected = zov_cut_selected(list, (uint64_t)i + 1);
            if (complement) selected = !selected;
            if (selected && !zov_cut_write(out, line + i, 1)) return 0;
        }
    } else {
        size_t fields = 1;
        for (size_t i = 0; i < length; ++i) if (line[i] == delimiter) ++fields;
        int has_delimiter = fields > 1;
        if (!has_delimiter) {
            if (suppress) return 1;
            if (!zov_cut_write(out, line, length)) return 0;
            wrote = !suppress;
        } else {
            size_t start = 0;
            uint64_t index = 1;
            for (size_t i = 0; i <= length; ++i) {
                if (i != length && line[i] != delimiter) continue;
                int selected = zov_cut_selected(list, index);
                if (complement) selected = !selected;
                if (selected) {
                    if (wrote) {
                        if (has_output_delimiter) {
                            if (!zov_cut_write(out, output_delimiter, strlen(output_delimiter))) return 0;
                        } else if (!zov_cut_write(out, &delimiter, 1)) return 0;
                    }
                    if (!zov_cut_write(out, line + start, i - start)) return 0;
                    wrote = 1;
                }
                start = i + 1;
                ++index;
            }
        }
    }
    if (has_terminator && !zov_cut_write(out, &terminator, 1)) return 0;
    return 1;
}

static inline int zov_cut_process(const char *path, const char *list, int mode,
                                  int delimiter, const char *output_delimiter, int has_output_delimiter,
                                  int complement, int suppress, int zero_terminated) {
    FILE *in = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
    if (in == NULL) {
        fprintf(stderr, "cut: %s: %s\n", path, strerror(errno));
        return 1;
    }
    int close_input = in != stdin;
    unsigned char *line = NULL;
    size_t length = 0, capacity = 0;
    int status = 0;
    int c;
    unsigned char terminator = zero_terminated ? 0 : '\n';
    while ((c = fgetc(in)) != EOF) {
        if ((unsigned char)c == terminator) {
            if (!zov_cut_emit_line(stdout, line, length, 1, terminator, list, mode,
                                   (unsigned char)delimiter, output_delimiter,
                                   has_output_delimiter,
                                   complement, suppress)) { status = 1; break; }
            length = 0;
            continue;
        }
        if (length == capacity) {
            size_t next = capacity ? capacity * 2 : 4096;
            if (next < capacity) { status = 1; break; }
            unsigned char *grown = (unsigned char *)realloc(line, next);
            if (grown == NULL) { status = 1; break; }
            line = grown;
            capacity = next;
        }
        line[length++] = (unsigned char)c;
    }
    if (status == 0 && ferror(in)) {
        fprintf(stderr, "cut: read error: %s\n", strerror(errno));
        status = 1;
    }
    if (status == 0 && length != 0 &&
        !zov_cut_emit_line(stdout, line, length, 1, terminator, list, mode,
                           (unsigned char)delimiter, output_delimiter,
                           has_output_delimiter,
                           complement, suppress)) status = 1;
    free(line);
    if (close_input && fclose(in) != 0) status = 1;
    if (fflush(stdout) != 0) {
        fprintf(stderr, "cut: write error: %s\n", strerror(errno));
        status = 1;
    }
    return status;
}

#endif
