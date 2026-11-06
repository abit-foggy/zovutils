#ifndef ZOVUTILS_NL_BACKEND_H
#define ZOVUTILS_NL_BACKEND_H

#include <errno.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned char *data;
    size_t length;
    size_t capacity;
} zov_nl_line;

static inline int zov_nl_read_line(FILE *fp, zov_nl_line *line, int *terminated) {
    line->length = 0;
    *terminated = 0;
    for (;;) {
        int c = fgetc(fp);
        if (c == EOF) return line->length != 0 || ferror(fp) ? 1 : 0;
        if (c == '\n') { *terminated = 1; return 1; }
        if (line->length == line->capacity) {
            size_t next = line->capacity == 0 ? 256 : line->capacity * 2;
            if (next < line->capacity) return -1;
            unsigned char *grown = (unsigned char *)realloc(line->data, next);
            if (grown == NULL) return -1;
            line->data = grown;
            line->capacity = next;
        }
        line->data[line->length++] = (unsigned char)c;
    }
}

static inline int zov_nl_starts_section(const unsigned char *line, size_t length,
                                        const unsigned char *delimiter,
                                        size_t delimiter_length, size_t count) {
    if (delimiter_length == 0 || delimiter_length > SIZE_MAX / count ||
        length < delimiter_length * count) return 0;
    for (size_t i = 0; i < delimiter_length * count; ++i)
        if (line[i] != delimiter[i % delimiter_length]) return 0;
    return 1;
}

static inline int zov_nl_selected(char style, const unsigned char *line,
                                 size_t length, regex_t *regex,
                                 int regex_ready) {
    if (style == 'a') return 1;
    if (style == 't') return length != 0;
    if (style == 'n') return 0;
    if (style == 'p' && regex_ready) {
        char *text = (char *)malloc(length + 1);
        if (text == NULL) return 0;
        if (length != 0) memcpy(text, line, length);
        text[length] = '\0';
        int matched = regexec(regex, text, 0, NULL, 0) == 0;
        free(text);
        return matched;
    }
    return 0;
}

static inline int zov_nl_emit_number(uint64_t number, unsigned width,
                                     char format, const char *separator) {
    char text[64];
    snprintf(text, sizeof(text), "%llu", (unsigned long long)number);
    size_t length = strlen(text);
    size_t pad = width > length ? width - length : 0;
    if (format == 'l') {
        if (fputs(text, stdout) == EOF) return 0;
        while (pad-- != 0) if (fputc(' ', stdout) == EOF) return 0;
    } else {
        char fill = format == 'z' ? '0' : ' ';
        while (pad-- != 0) if (fputc(fill, stdout) == EOF) return 0;
        if (fputs(text, stdout) == EOF) return 0;
    }
    return fputs(separator, stdout) != EOF;
}

static inline int zov_nl_emit_blank(unsigned width) {
    for (unsigned i = 0; i <= width; ++i) if (fputc(' ', stdout) == EOF) return 0;
    return 1;
}

static inline int zov_nl_run(size_t file_count, signed char *const *paths,
                             char body_style, char header_style,
                             char footer_style, const char *body_pattern,
                             const char *header_pattern, const char *footer_pattern,
                             const char *delimiter_text, const char *separator,
                             uint64_t increment, uint64_t start_number,
                             uint64_t join_blank, unsigned width,
                             char number_format, int no_renumber) {
    regex_t regex[3];
    int ready[3] = {0, 0, 0};
    const char *patterns[3] = {header_pattern, body_pattern, footer_pattern};
    char styles[3] = {header_style, body_style, footer_style};
    for (int i = 0; i < 3; ++i) {
        if (styles[i] == 'p') {
            int rc = regcomp(&regex[i], patterns[i], 0);
            if (rc != 0) {
                fprintf(stderr, "nl: invalid regular expression: %s\n", patterns[i]);
                for (int j = 0; j < i; ++j) if (ready[j]) regfree(&regex[j]);
                return 1;
            }
            ready[i] = 1;
        }
    }

    unsigned char delimiter[4096];
    size_t delimiter_length = strlen(delimiter_text);
    if (delimiter_length == 1) {
        delimiter[0] = (unsigned char)delimiter_text[0];
        delimiter[1] = ':';
        delimiter_length = 2;
    } else if (delimiter_length > sizeof(delimiter)) {
        for (int i = 0; i < 3; ++i) if (ready[i]) regfree(&regex[i]);
        fprintf(stderr, "nl: section delimiter is too long\n");
        return 1;
    } else if (delimiter_length != 0) {
        memcpy(delimiter, delimiter_text, delimiter_length);
    }

    enum { NL_HEADER, NL_BODY, NL_FOOTER } section = NL_BODY;
    uint64_t number = start_number;
    uint64_t blank_run = 0;
    zov_nl_line line = {0};
    int status = 0;
    for (size_t f = 0; f < file_count; ++f) {
        const char *path = (const char *)paths[f];
        FILE *fp = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
        if (fp == NULL) {
            fprintf(stderr, "nl: %s: %s\n", path, strerror(errno));
            status = 1;
            continue;
        }
        int close_fp = fp != stdin;
        for (;;) {
            int terminated = 0;
            int rc = zov_nl_read_line(fp, &line, &terminated);
            if (rc == 0) break;
            if (rc < 0) { fprintf(stderr, "nl: read error: %s\n", path); status = 1; break; }

            size_t marker = 0;
            if (zov_nl_starts_section(line.data, line.length, delimiter, delimiter_length, 3)) {
                section = NL_FOOTER; marker = 1;
            } else if (zov_nl_starts_section(line.data, line.length, delimiter, delimiter_length, 2)) {
                section = NL_BODY; marker = 1;
            } else if (zov_nl_starts_section(line.data, line.length, delimiter, delimiter_length, 1)) {
                section = NL_HEADER; marker = 1;
            }
            if (marker) {
                if (fputc('\n', stdout) == EOF) status = 1;
                if (!no_renumber) number = start_number;
                blank_run = 0;
                continue;
            }

            int blank = line.length == 0;
            if (blank) ++blank_run; else blank_run = 0;
            int style_index = section == NL_HEADER ? 0 : section == NL_BODY ? 1 : 2;
            char style = styles[style_index];
            regex_t *compiled = ready[style_index] ? &regex[style_index] : NULL;
            int selected = zov_nl_selected(style, line.data, line.length,
                                            compiled, ready[style_index]);
            if (blank && join_blank > 1) selected = selected && blank_run % join_blank == 0;
            int ok = selected ? zov_nl_emit_number(number, width, number_format, separator)
                              : zov_nl_emit_blank(width);
            if (line.length != 0 && fwrite(line.data, 1, line.length, stdout) != line.length) ok = 0;
            if (fputc('\n', stdout) == EOF) ok = 0;
            if (!ok) status = 1;
            if (selected && UINT64_MAX - number >= increment) number += increment;
            if (!terminated && ferror(fp)) status = 1;
        }
        if (close_fp && fclose(fp) != 0) status = 1;
    }
    free(line.data);
    if (fflush(stdout) != 0) status = 1;
    for (int i = 0; i < 3; ++i) if (ready[i]) regfree(&regex[i]);
    return status;
}

#endif
