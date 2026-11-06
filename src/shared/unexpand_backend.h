#ifndef ZOVUTILS_UNEXPAND_BACKEND_H
#define ZOVUTILS_UNEXPAND_BACKEND_H

#include "expand_backend.h"

static inline int zov_unexpand_emit_spaces(size_t *column, size_t spaces,
                                           const zov_expand_tabs *tabs) {
    if (spaces > SIZE_MAX - *column) return 0;
    size_t end = *column + spaces;
    while (zov_expand_next_tab(tabs, *column) <= end) {
        size_t stop = zov_expand_next_tab(tabs, *column);
        if (fputc('\t', stdout) == EOF) return 0;
        *column = stop;
    }
    while (*column < end) {
        if (fputc(' ', stdout) == EOF) return 0;
        ++*column;
    }
    return 1;
}

static inline int zov_unexpand_stream(FILE *fp, const zov_expand_tabs *tabs,
                                      int all_blanks) {
    size_t column = 0;
    size_t pending_spaces = 0;
    int at_line_start = 1;
    unsigned char unit[MB_LEN_MAX];
    size_t n;
    while ((n = zov_expand_read_unit(fp, unit)) != 0) {
        if (unit[0] == ' ' && (all_blanks || at_line_start)) {
            if (pending_spaces == SIZE_MAX) return 1;
            ++pending_spaces;
            continue;
        }
        if (pending_spaces != 0) {
            if (!zov_unexpand_emit_spaces(&column, pending_spaces, tabs)) return 1;
            pending_spaces = 0;
        }
        if (unit[0] == '\t') {
            if (fputc('\t', stdout) == EOF) return 1;
            column = zov_expand_next_tab(tabs, column);
            continue;
        }
        if (!zov_expand_write_unit(stdout, unit, n)) return 1;
        if (unit[0] == '\n') { column = 0; at_line_start = 1; }
        else if (unit[0] == '\b') { if (column != 0) --column; at_line_start = 0; }
        else {
            int width = zov_expand_unit_width(unit, n);
            if (width > 0 && column <= SIZE_MAX - (size_t)width) column += (size_t)width;
            if (!isblank(unit[0])) at_line_start = 0;
        }
    }
    if (pending_spaces != 0 && !zov_unexpand_emit_spaces(&column, pending_spaces, tabs)) return 1;
    return ferror(fp) || fflush(stdout) != 0;
}

static inline int zov_unexpand_run(size_t file_count, signed char *const *paths,
                                   const char *tab_text, int all_blanks) {
    (void)setlocale(LC_CTYPE, "");
    zov_expand_tabs tabs;
    int tabs_error = 0;
    if (!zov_expand_parse_tabs(tab_text, &tabs, &tabs_error)) {
        zov_expand_report_tab_error("unexpand", tab_text, tabs_error);
        return 1;
    }
    int status = 0;
    for (size_t i = 0; i < file_count; ++i) {
        const char *path = (const char *)paths[i];
        FILE *fp = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
        if (fp == NULL) {
            fprintf(stderr, "unexpand: %s: %s\n", path, strerror(errno));
            status = 1;
            continue;
        }
        int close_fp = fp != stdin;
        if (zov_unexpand_stream(fp, &tabs, all_blanks)) status = 1;
        if (close_fp && fclose(fp) != 0) status = 1;
    }
    if (fflush(stdout) != 0) status = 1;
    return status;
}

#endif
