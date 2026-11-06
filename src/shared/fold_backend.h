#ifndef ZOVUTILS_FOLD_BACKEND_H
#define ZOVUTILS_FOLD_BACKEND_H

#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

extern int wcwidth(wchar_t);

typedef struct {
    size_t byte_start;
    size_t byte_length;
    size_t width;
    unsigned char blank;
    unsigned char tab;
    unsigned char backspace;
} zov_fold_unit;

typedef struct {
    unsigned char *data;
    size_t length;
    size_t capacity;
} zov_fold_line;

static inline int zov_fold_read_line(FILE *fp, zov_fold_line *line,
                                     int *terminated) {
    line->length = 0;
    *terminated = 0;
    for (;;) {
        int c = fgetc(fp);
        if (c == EOF) return ferror(fp) ? -1 : line->length != 0 ? 1 : 0;
        if (c == '\n') { *terminated = 1; return 1; }
        if (line->length == line->capacity) {
            size_t capacity = line->capacity == 0 ? 256 : line->capacity * 2;
            if (capacity < line->capacity) return -1;
            unsigned char *data = (unsigned char *)realloc(line->data, capacity);
            if (data == NULL) return -1;
            line->data = data;
            line->capacity = capacity;
        }
        line->data[line->length++] = (unsigned char)c;
    }
}

static inline int zov_fold_make_units(const unsigned char *line, size_t length,
                                      int byte_mode, int character_mode,
                                      zov_fold_unit **units_out,
                                      size_t *count_out) {
    if (length > SIZE_MAX / sizeof(zov_fold_unit)) return 0;
    zov_fold_unit *units = length == 0 ? NULL :
        (zov_fold_unit *)malloc(length * sizeof(*units));
    if (length != 0 && units == NULL) return 0;
    size_t count = 0;
    size_t offset = 0;
    mbstate_t state;
    memset(&state, 0, sizeof(state));
    while (offset < length) {
        size_t bytes = 1;
        size_t width = 1;
        unsigned char c = line[offset];
        if (c >= 0x80 && MB_CUR_MAX > 1) {
            wchar_t wc;
            size_t rc = mbrtowc(&wc, (const char *)line + offset, length - offset, &state);
            if (rc != (size_t)-1 && rc != (size_t)-2 && rc != 0) {
                bytes = rc;
                int w = wcwidth(wc);
                width = w < 0 ? 1 : (size_t)w;
            } else {
                memset(&state, 0, sizeof(state));
            }
        } else {
            wchar_t wc = (wchar_t)c;
            if (c == 0 || c == '\b') width = 0;
            else if (c == '\t') width = 1;
            else if (!byte_mode && !character_mode && MB_CUR_MAX > 1) {
                int w = wcwidth(wc);
                width = w < 0 ? 1 : (size_t)w;
            }
        }
        if (byte_mode) width = bytes;
        else if (character_mode && c >= 0x80 && bytes > 1) width = 1;
        units[count].byte_start = offset;
        units[count].byte_length = bytes;
        units[count].width = width;
        units[count].blank = c == ' ' || c == '\t';
        units[count].tab = c == '\t';
        units[count].backspace = c == '\b';
        ++count;
        offset += bytes;
    }
    *units_out = units;
    *count_out = count;
    return 1;
}

static inline int zov_fold_emit_segment(const zov_fold_line *line,
                                        const zov_fold_unit *units,
                                        size_t first, size_t after) {
    if (first == after) return 1;
    size_t begin = units[first].byte_start;
    size_t end = units[after - 1].byte_start + units[after - 1].byte_length;
    return fwrite(line->data + begin, 1, end - begin, stdout) == end - begin;
}

static inline int zov_fold_process_line(zov_fold_line *line, size_t width,
                                        int bytes, int characters, int spaces) {
    zov_fold_unit *units = NULL;
    size_t count = 0;
    if (!zov_fold_make_units(line->data, line->length, bytes, characters,
                             &units, &count)) return 1;
    size_t start = 0;
    int status = 0;
    while (start < count) {
        size_t column = 0;
        size_t last_blank = SIZE_MAX;
        size_t i = start;
        for (; i < count; ++i) {
            size_t cost = units[i].width;
            if (units[i].tab && !bytes && !characters) cost = 8 - (column % 8);
            if (units[i].backspace) cost = column == 0 ? 0 : (size_t)-1;
            size_t after_column = units[i].backspace ? (column == 0 ? 0 : column - 1) :
                                  cost > SIZE_MAX - column ? SIZE_MAX : column + cost;
            if (after_column > width && i > start) break;
            column = after_column;
            if (units[i].blank) last_blank = i;
            if (column > width && i == start) { ++i; break; }
        }
        if (i >= count) {
            if (!zov_fold_emit_segment(line, units, start, count)) status = 1;
            break;
        }
        size_t end = i;
        if (spaces && last_blank != SIZE_MAX && last_blank >= start && last_blank < i)
            end = last_blank + 1;
        if (end == start) end = i == start ? i + 1 : i;
        if (!zov_fold_emit_segment(line, units, start, end)) status = 1;
        if (end < count && fputc('\n', stdout) == EOF) status = 1;
        start = end;
    }
    free(units);
    return status;
}

static inline int zov_fold_run(size_t file_count, signed char *const *paths,
                               size_t width, int bytes, int characters,
                               int spaces) {
    (void)setlocale(LC_CTYPE, "");
    int status = 0;
    zov_fold_line line = {0};
    for (size_t f = 0; f < file_count; ++f) {
        const char *path = (const char *)paths[f];
        FILE *fp = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
        if (fp == NULL) {
            fprintf(stderr, "fold: %s: %s\n", path, strerror(errno));
            status = 1;
            continue;
        }
        int close_fp = fp != stdin;
        for (;;) {
            int terminated = 0;
            int rc = zov_fold_read_line(fp, &line, &terminated);
            if (rc == 0) break;
            if (rc < 0) { fprintf(stderr, "fold: read error: %s\n", path); status = 1; break; }
            if (zov_fold_process_line(&line, width, bytes, characters, spaces)) status = 1;
            if (terminated && fputc('\n', stdout) == EOF) status = 1;
        }
        if (close_fp && fclose(fp) != 0) status = 1;
    }
    free(line.data);
    if (fflush(stdout) != 0) status = 1;
    return status;
}

#endif
