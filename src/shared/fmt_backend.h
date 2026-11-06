#ifndef ZOVUTILS_FMT_BACKEND_H
#define ZOVUTILS_FMT_BACKEND_H

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

extern int wcwidth(wchar_t);

typedef struct {
    unsigned char *data;
    size_t length;
    size_t capacity;
} zov_fmt_line;

typedef struct {
    const unsigned char *data;
    size_t length;
    const unsigned char *gap;
    size_t gap_length;
    int sentence_gap;
} zov_fmt_word;

static inline int zov_fmt_read_line(FILE *fp, zov_fmt_line *line,
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

static inline size_t zov_fmt_char_width(const unsigned char *s, size_t n,
                                        size_t column, size_t *bytes) {
    if (s[0] == '\t') { *bytes = 1; return 8 - (column % 8); }
    if (s[0] < 0x80 || MB_CUR_MAX <= 1) { *bytes = 1; return s[0] == '\0' ? 0 : 1; }
    mbstate_t state;
    memset(&state, 0, sizeof(state));
    wchar_t wc;
    size_t rc = mbrtowc(&wc, (const char *)s, n, &state);
    if (rc == (size_t)-1 || rc == (size_t)-2 || rc == 0) { *bytes = 1; return 1; }
    *bytes = rc;
    int width = wcwidth(wc);
    return width < 0 ? 1 : (size_t)width;
}

static inline size_t zov_fmt_columns(const unsigned char *s, size_t n,
                                     size_t initial_column) {
    size_t column = initial_column;
    for (size_t i = 0; i < n;) {
        size_t bytes = 1;
        size_t width = zov_fmt_char_width(s + i, n - i, column, &bytes);
        column = width > SIZE_MAX - column ? SIZE_MAX : column + width;
        i += bytes;
    }
    return column - initial_column;
}

static inline int zov_fmt_space(unsigned char c) {
    return c == ' ' || c == '\t';
}

static inline size_t zov_fmt_indent(const zov_fmt_line *line) {
    size_t i = 0;
    while (i < line->length && zov_fmt_space(line->data[i])) ++i;
    return i;
}

static inline int zov_fmt_sentence(const unsigned char *word, size_t length) {
    if (length == 0) return 0;
    unsigned char c = word[length - 1];
    return c == '.' || c == '?' || c == '!';
}

static inline int zov_fmt_emit_prefix(FILE *out, const char *prefix,
                                      const unsigned char *indent,
                                      size_t indent_length, size_t *column) {
    size_t prefix_length = strlen(prefix);
    if (prefix_length && fwrite(prefix, 1, prefix_length, out) != prefix_length) return 0;
    if (indent_length && fwrite(indent, 1, indent_length, out) != indent_length) return 0;
    *column = zov_fmt_columns((const unsigned char *)prefix, prefix_length, 0) +
              zov_fmt_columns(indent, indent_length, prefix_length);
    return 1;
}

static inline int zov_fmt_emit_paragraph(zov_fmt_line *lines, size_t count,
                                         size_t width, size_t goal, int uniform,
                                         int crown, int tagged,
                                         int split_only, const char *prefix) {
    if (count == 0) return 1;
    size_t indent_first_length = zov_fmt_indent(&lines[0]);
    size_t indent_second_length = count > 1 ? zov_fmt_indent(&lines[1]) : indent_first_length;
    const unsigned char *indent_first = lines[0].data;
    const unsigned char *indent_hanging = crown || tagged ? lines[1 < count ? 1 : 0].data : indent_first;
    size_t indent_hanging_length = crown || tagged ? indent_second_length : indent_first_length;
    size_t column = 0;
    int have_word = 0;
    int previous_sentence = 0;
    if (!zov_fmt_emit_prefix(stdout, prefix, indent_first, indent_first_length, &column)) return 0;

    for (size_t li = 0; li < count; ++li) {
        size_t p = zov_fmt_indent(&lines[li]);
        int first_word = 1;
        while (p < lines[li].length) {
            size_t gap_start = p;
            while (p < lines[li].length && zov_fmt_space(lines[li].data[p])) ++p;
            size_t gap_length = p - gap_start;
            size_t word_start = p;
            while (p < lines[li].length && !zov_fmt_space(lines[li].data[p])) ++p;
            if (word_start == p) continue;
            size_t word_length = p - word_start;

            unsigned char sentence_spaces[2] = {' ', ' '};
            const unsigned char *gap = lines[li].data + gap_start;
            size_t use_gap = gap_length;
            if (li > 0 && first_word) {
                use_gap = have_word ? (previous_sentence ? 2 : 1) : 0;
                gap = sentence_spaces;
            } else if (uniform && have_word) {
                use_gap = previous_sentence ? 2 : 1;
                gap = sentence_spaces;
            }
            if (!have_word) use_gap = 0;
            size_t gap_columns = zov_fmt_columns(gap, use_gap, column);
            size_t word_columns = zov_fmt_columns(lines[li].data + word_start,
                                                  word_length, column + gap_columns);
            size_t target = width;
            size_t new_column = column > SIZE_MAX - gap_columns ? SIZE_MAX : column + gap_columns;
            new_column = new_column > SIZE_MAX - word_columns ? SIZE_MAX : new_column + word_columns;
            if (have_word && (gap_columns > SIZE_MAX - column ||
                word_columns > SIZE_MAX - column - gap_columns ||
                (split_only ? new_column > target :
                 (new_column >= target || (goal != 0 && column >= goal))))) {
                if (fputc('\n', stdout) == EOF) return 0;
                if (!zov_fmt_emit_prefix(stdout, prefix,
                        indent_hanging, indent_hanging_length,
                        &column)) return 0;
                use_gap = 0;
                gap_columns = 0;
            }
            if (use_gap && fwrite(gap, 1, use_gap, stdout) != use_gap) return 0;
            if (word_length && fwrite(lines[li].data + word_start, 1, word_length, stdout) != word_length) return 0;
            column = column > SIZE_MAX - gap_columns ? SIZE_MAX : column + gap_columns;
            column = column > SIZE_MAX - word_columns ? SIZE_MAX : column + word_columns;
            previous_sentence = zov_fmt_sentence(lines[li].data + word_start, word_length);
            have_word = 1;
            first_word = 0;
        }
    }
    return fputc('\n', stdout) != EOF;
}

static inline int zov_fmt_run(size_t file_count, signed char *const *paths,
                              size_t width, size_t goal, int uniform,
                              int split_only, int crown, int tagged,
                              const char *prefix) {
    (void)setlocale(LC_CTYPE, "");
    zov_fmt_line *paragraph = NULL;
    size_t paragraph_count = 0, paragraph_capacity = 0;
    int status = 0;
    for (size_t f = 0; f < file_count; ++f) {
        const char *path = (const char *)paths[f];
        FILE *fp = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
        if (fp == NULL) { fprintf(stderr, "fmt: cannot open '%s' for reading: %s\n", path, strerror(errno)); status = 1; continue; }
        int close_fp = fp != stdin;
        zov_fmt_line line = {0};
        for (;;) {
            int terminated = 0;
            int rc = zov_fmt_read_line(fp, &line, &terminated);
            if (rc == 0) break;
            if (rc < 0) { fprintf(stderr, "fmt: read error: %s\n", path); status = 1; break; }
            int matches_prefix = *prefix == '\0' ||
                (line.length >= strlen(prefix) && memcmp(line.data, prefix, strlen(prefix)) == 0);
            if (split_only || !matches_prefix) {
                if (paragraph_count) {
                    if (!zov_fmt_emit_paragraph(paragraph, paragraph_count, width, goal, uniform, crown, tagged, split_only, prefix)) status = 1;
                    for (size_t k = 0; k < paragraph_count; ++k) free(paragraph[k].data);
                    paragraph_count = 0;
                }
                if (!matches_prefix) {
                    if (line.length && fwrite(line.data, 1, line.length, stdout) != line.length) status = 1;
                    if (terminated && fputc('\n', stdout) == EOF) status = 1;
                } else {
                    size_t prefix_length = strlen(prefix);
                    unsigned char *content = line.data;
                    if (prefix_length != 0) content = line.data + prefix_length;
                    zov_fmt_line one = {content, line.length - prefix_length,
                                        line.length - prefix_length};
                    if (!zov_fmt_emit_paragraph(&one, 1, width, goal, uniform,
                                                crown, tagged, split_only, prefix)) status = 1;
                }
                continue;
            }
            size_t prefix_length = strlen(prefix);
            size_t content_start = prefix_length;
            size_t blank_start = content_start;
            while (blank_start < line.length && zov_fmt_space(line.data[blank_start])) ++blank_start;
            if (blank_start == line.length) {
                if (paragraph_count) {
                    if (!zov_fmt_emit_paragraph(paragraph, paragraph_count, width, goal, uniform, crown, tagged, split_only, prefix)) status = 1;
                    for (size_t k = 0; k < paragraph_count; ++k) free(paragraph[k].data);
                    paragraph_count = 0;
                }
                if (prefix_length && fwrite(prefix, 1, prefix_length, stdout) != prefix_length) status = 1;
                if (fputc('\n', stdout) == EOF) status = 1;
                continue;
            }
            if (paragraph_count == paragraph_capacity) {
                size_t next = paragraph_capacity == 0 ? 8 : paragraph_capacity * 2;
                if (next < paragraph_capacity || next > SIZE_MAX / sizeof(*paragraph)) { status = 1; break; }
                zov_fmt_line *grown = (zov_fmt_line *)realloc(paragraph, next * sizeof(*paragraph));
                if (grown == NULL) { status = 1; break; }
                paragraph = grown; paragraph_capacity = next;
            }
            zov_fmt_line *dst = &paragraph[paragraph_count++];
            dst->length = line.length - content_start;
            dst->capacity = dst->length;
            dst->data = dst->length ? (unsigned char *)malloc(dst->length) : NULL;
            if (dst->length && dst->data == NULL) { status = 1; --paragraph_count; break; }
            if (dst->length) memcpy(dst->data, line.data + content_start, dst->length);
        }
        if (paragraph_count) {
            if (!zov_fmt_emit_paragraph(paragraph, paragraph_count, width, goal, uniform, crown, tagged, split_only, prefix)) status = 1;
            for (size_t i = 0; i < paragraph_count; ++i) free(paragraph[i].data);
            paragraph_count = 0;
        }
        free(line.data);
        if (close_fp && fclose(fp) != 0) status = 1;
    }
    free(paragraph);
    if (fflush(stdout) != 0) status = 1;
    return status;
}

#endif
