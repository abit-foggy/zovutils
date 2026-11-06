#ifndef ZOVUTILS_EXPAND_BACKEND_H
#define ZOVUTILS_EXPAND_BACKEND_H

#include <errno.h>
#include <ctype.h>
#include <locale.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

extern int wcwidth(wchar_t);

typedef struct {
    size_t stops[256];
    size_t count;
    size_t repeat;
} zov_expand_tabs;

static inline int zov_expand_parse_tabs(const char *text, zov_expand_tabs *tabs,
                                       int *error_code) {
    *error_code = 3;
    memset(tabs, 0, sizeof(*tabs));
    if (text == NULL || *text == '\0') return 0;
    char copy[4096];
    size_t text_length = strlen(text);
    if (text_length >= sizeof(copy)) return 0;
    memcpy(copy, text, text_length + 1);
    size_t repeat_override = 0;
    char *slash = strchr(copy, '/');
    if (slash != NULL) {
        if (strchr(slash + 1, '/') != NULL || slash == copy || slash[-1] != ',' || slash[1] == '\0') return 0;
        char *end = NULL;
        errno = 0;
        unsigned long long n = strtoull(slash + 1, &end, 10);
        if (errno != 0 || end == slash + 1 || *end != '\0' || n > (unsigned long long)SIZE_MAX) return 0;
        repeat_override = (size_t)n;
        slash[-1] = '\0';
    }
    const char *p = copy;
    int last_relative = 0;
    while (*p != '\0') {
        int relative = *p == '+';
        last_relative = relative;
        if (relative) ++p;
        if (*p < '0' || *p > '9' || tabs->count == 256) return 0;
        char *end = NULL;
        errno = 0;
        unsigned long long n = strtoull(p, &end, 10);
        if (errno != 0 || end == p || n > (unsigned long long)SIZE_MAX) { *error_code = 5; return 0; }
        if (n == 0) { *error_code = 2; return 0; }
        size_t stop = (size_t)n;
        if (relative) {
            if (tabs->count == 0 || stop > SIZE_MAX - tabs->stops[tabs->count - 1]) return 0;
            stop += tabs->stops[tabs->count - 1];
        }
        if (tabs->count != 0 && stop <= tabs->stops[tabs->count - 1]) { *error_code = 4; return 0; }
        tabs->stops[tabs->count++] = stop;
        p = end;
        if (*p == ',') {
            ++p;
            if (*p == '\0') break;
        } else if (*p != '\0') return 0;
    }
    if (repeat_override != 0) tabs->repeat = repeat_override;
    else if (tabs->count == 1) tabs->repeat = tabs->stops[0];
    else if (last_relative) tabs->repeat = tabs->stops[tabs->count - 1] - tabs->stops[tabs->count - 2];
    else tabs->repeat = 1;
    return tabs->repeat != 0;
}

static inline size_t zov_expand_next_tab(const zov_expand_tabs *tabs, size_t column) {
    for (size_t i = 0; i < tabs->count; ++i)
        if (tabs->stops[i] > column) return tabs->stops[i];
    size_t last = tabs->stops[tabs->count - 1];
    size_t delta = column - last;
    size_t steps = delta / tabs->repeat + 1;
    if (steps > (SIZE_MAX - last) / tabs->repeat) return SIZE_MAX;
    return last + steps * tabs->repeat;
}

static inline void zov_expand_report_tab_error(const char *utility,
                                               const char *tab_text,
                                               int error_code) {
    if (error_code == 2) fprintf(stderr, "%s: tab size cannot be 0\n", utility);
    else if (error_code == 4) fprintf(stderr, "%s: tab sizes must be ascending\n", utility);
    else if (error_code == 5) fprintf(stderr, "%s: tab size too large\n", utility);
    else fprintf(stderr, "%s: tab size contains invalid character(s): '%s'\n", utility, tab_text);
}

static inline int zov_expand_write_unit(FILE *out, const unsigned char *bytes, size_t n) {
    return n == 0 || fwrite(bytes, 1, n, out) == n;
}

static inline size_t zov_expand_read_unit(FILE *fp, unsigned char bytes[MB_LEN_MAX]) {
    int first = fgetc(fp);
    if (first == EOF) return 0;
    bytes[0] = (unsigned char)first;
    if (bytes[0] < 0x80 || MB_CUR_MAX <= 1) return 1;

    mbstate_t initial;
    memset(&initial, 0, sizeof(initial));
    for (size_t n = 1; n < MB_LEN_MAX; ++n) {
        mbstate_t state = initial;
        wchar_t wc;
        size_t rc = mbrtowc(&wc, (const char *)bytes, n, &state);
        if (rc == (size_t)-2) {
            int next = fgetc(fp);
            if (next == EOF) return n;
            bytes[n] = (unsigned char)next;
            continue;
        }
        if (rc == (size_t)-1 || rc == 0) return n;
        return n;
    }
    return MB_LEN_MAX;
}

static inline int zov_expand_unit_width(const unsigned char *unit, size_t n) {
    if (unit[0] == '\0') return 0;
    if (unit[0] < 0x80 || MB_CUR_MAX <= 1) return 1;
    mbstate_t state;
    memset(&state, 0, sizeof(state));
    wchar_t wc;
    size_t rc = mbrtowc(&wc, (const char *)unit, n, &state);
    if (rc == (size_t)-1 || rc == (size_t)-2 || rc == 0) return 1;
    int width = wcwidth(wc);
    return width < 0 ? 1 : width;
}

static inline int zov_expand_stream(FILE *fp, const zov_expand_tabs *tabs,
                                    int initial_only) {
    size_t column = 0;
    int at_line_start = 1;
    unsigned char unit[MB_LEN_MAX];
    size_t next;
    while ((next = zov_expand_read_unit(fp, unit)) != 0) {
        int width = zov_expand_unit_width(unit, next);
        if (unit[0] == '\t' && (!initial_only || at_line_start)) {
            size_t stop = zov_expand_next_tab(tabs, column);
            size_t spaces = stop == SIZE_MAX ? tabs->repeat : stop - column;
            for (size_t i = 0; i < spaces; ++i)
                if (fputc(' ', stdout) == EOF) return 1;
            column = stop;
            continue;
        }
        if (!zov_expand_write_unit(stdout, unit, next)) return 1;
        if (unit[0] == '\n') { column = 0; at_line_start = 1; }
        else if (unit[0] == '\b') { if (column != 0) --column; at_line_start = 0; }
        else {
            if (width > 0 && column <= SIZE_MAX - (size_t)width) column += (size_t)width;
            if (!initial_only || !isblank(unit[0])) at_line_start = 0;
        }
    }
    return ferror(fp) || fflush(stdout) != 0;
}

static inline int zov_expand_run(size_t file_count, signed char *const *paths,
                                 const char *tab_text, int initial_only) {
    (void)setlocale(LC_CTYPE, "");
    zov_expand_tabs tabs;
    int tabs_error = 0;
    if (!zov_expand_parse_tabs(tab_text, &tabs, &tabs_error)) {
        zov_expand_report_tab_error("expand", tab_text, tabs_error);
        return 1;
    }
    int status = 0;
    for (size_t i = 0; i < file_count; ++i) {
        const char *path = (const char *)paths[i];
        FILE *fp = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
        if (fp == NULL) {
            fprintf(stderr, "expand: %s: %s\n", path, strerror(errno));
            status = 1;
            continue;
        }
        int close_fp = fp != stdin;
        if (zov_expand_stream(fp, &tabs, initial_only)) status = 1;
        if (close_fp && fclose(fp) != 0) status = 1;
    }
    if (fflush(stdout) != 0) status = 1;
    return status;
}

#endif
