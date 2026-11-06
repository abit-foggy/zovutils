#ifndef ZOVUTILS_UNIQ_BACKEND_H
#define ZOVUTILS_UNIQ_BACKEND_H

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int zov_uniq_parse_count(const char *text, size_t *value) {
    char *end = NULL;
    errno = 0;
    unsigned long long n = strtoull(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE || text[0] == '-') return 1;
    if (n > (unsigned long long)(size_t)-1) return 1;
    *value = (size_t)n;
    return 0;
}

typedef struct { unsigned char *data; size_t length; } zov_uniq_line;

static inline int zov_uniq_read(FILE *fp, unsigned char term,
                                zov_uniq_line **lines, size_t *count,
                                size_t *capacity, unsigned char **buffer,
                                size_t *length, size_t *buffer_capacity) {
    int c;
    while ((c = fgetc(fp)) != EOF) {
        if ((unsigned char)c == term) {
            if (*count == *capacity) {
                size_t next = *capacity ? *capacity * 2 : 128;
                if (next < *capacity || next > (size_t)-1 / sizeof(**lines)) return -1;
                zov_uniq_line *grown = (zov_uniq_line *)realloc(*lines, next * sizeof(**lines));
                if (grown == NULL) return -1;
                *lines = grown; *capacity = next;
            }
            unsigned char *copy = (unsigned char *)malloc(*length + 1);
            if (copy == NULL) return -1;
            if (*length) memcpy(copy, *buffer, *length);
            copy[*length] = 0;
            (*lines)[*count].data = copy; (*lines)[*count].length = *length;
            ++*count; *length = 0;
            return 1;
        }
        if (*length == *buffer_capacity) {
            size_t next = *buffer_capacity ? *buffer_capacity * 2 : 4096;
            if (next < *buffer_capacity) return -1;
            unsigned char *grown = (unsigned char *)realloc(*buffer, next);
            if (grown == NULL) return -1;
            *buffer = grown; *buffer_capacity = next;
        }
        (*buffer)[(*length)++] = (unsigned char)c;
    }
    if (ferror(fp)) return -1;
    if (*length == 0) return 0;
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 128;
        if (next < *capacity || next > (size_t)-1 / sizeof(**lines)) return -1;
        zov_uniq_line *grown = (zov_uniq_line *)realloc(*lines, next * sizeof(**lines));
        if (grown == NULL) return -1;
        *lines = grown; *capacity = next;
    }
    unsigned char *copy = (unsigned char *)malloc(*length + 1);
    if (copy == NULL) return -1;
    memcpy(copy, *buffer, *length); copy[*length] = 0;
    (*lines)[*count].data = copy; (*lines)[*count].length = *length;
    ++*count; *length = 0;
    return 0;
}

static inline size_t zov_uniq_offset(const zov_uniq_line *line,
                                     size_t fields, size_t chars) {
    size_t i = 0;
    while (fields > 0) {
        while (i < line->length && (line->data[i] == ' ' || line->data[i] == '\t')) ++i;
        while (i < line->length && line->data[i] != ' ' && line->data[i] != '\t') ++i;
        --fields;
    }
    size_t remain = line->length - i;
    return i + (chars < remain ? chars : remain);
}

static inline int zov_uniq_equal(const zov_uniq_line *a,
                                 const zov_uniq_line *b, int ignore_case,
                                 size_t skip_fields, size_t skip_chars,
                                 size_t check_chars) {
    size_t ai = zov_uniq_offset(a, skip_fields, skip_chars);
    size_t bi = zov_uniq_offset(b, skip_fields, skip_chars);
    size_t an = a->length - ai, bn = b->length - bi;
    if (check_chars != 0 && check_chars < an) an = check_chars;
    if (check_chars != 0 && check_chars < bn) bn = check_chars;
    size_t n = an < bn ? an : bn;
    for (size_t i = 0; i < n; ++i) {
        int ac = a->data[ai + i], bc = b->data[bi + i];
        if (ignore_case) { ac = tolower((unsigned char)ac); bc = tolower((unsigned char)bc); }
        if (ac != bc) return 0;
    }
    return an == bn;
}

static inline int zov_uniq_emit(FILE *out, const zov_uniq_line *line,
                                unsigned char term, size_t count,
                                int count_mode) {
    if (count_mode && fprintf(out, "%7zu ", count) < 0) return 0;
    if (line->length && fwrite(line->data, 1, line->length, out) != line->length) return 0;
    return fwrite(&term, 1, 1, out) == 1;
}

static inline int zov_uniq_run(size_t file_count, const char *const *paths,
                               const char *output_path, int has_output_path,
                               int count_mode,
                               int duplicates, int only_unique, int all_repeated,
                               int ignore_case, size_t skip_fields,
                               size_t skip_chars, size_t check_chars,
                               int zero_terminated) {
    zov_uniq_line *lines = NULL;
    size_t count = 0, capacity = 0;
    unsigned char *buffer = NULL;
    size_t length = 0, buffer_capacity = 0;
    unsigned char term = zero_terminated ? 0 : '\n';
    int status = 0;
    for (size_t f = 0; f < file_count; ++f) {
        FILE *fp = strcmp(paths[f], "-") == 0 ? stdin : fopen(paths[f], "rb");
        if (fp == NULL) { fprintf(stderr, "uniq: %s: %s\n", paths[f], strerror(errno)); status = 1; continue; }
        int close_fp = fp != stdin;
        int rc;
        do { rc = zov_uniq_read(fp, term, &lines, &count, &capacity,
                                &buffer, &length, &buffer_capacity); }
        while (rc == 1);
        if (rc < 0) { fprintf(stderr, "uniq: read error: %s\n", paths[f]); status = 1; }
        if (close_fp) fclose(fp);
    }
    free(buffer);
    FILE *out = stdout;
    int close_out = 0;
    if (has_output_path) {
        out = fopen(output_path, "wb");
        if (out == NULL) { fprintf(stderr, "uniq: %s: %s\n", output_path, strerror(errno)); status = 1; goto cleanup; }
        close_out = 1;
    }
    for (size_t i = 0; i < count;) {
        size_t end = i + 1;
        while (end < count && zov_uniq_equal(&lines[i], &lines[end], ignore_case,
                skip_fields, skip_chars, check_chars)) ++end;
        size_t group_count = end - i;
        int emit_group = (!duplicates && !only_unique && !all_repeated) ||
                         (duplicates && group_count > 1) ||
                         (only_unique && group_count == 1) ||
                         (all_repeated && group_count > 1);
        if (emit_group) {
            if (all_repeated && group_count > 1) {
                for (size_t j = i; j < end; ++j)
                    if (!zov_uniq_emit(out, &lines[j], term, 1, 0)) { status = 1; goto done; }
            } else if (!zov_uniq_emit(out, &lines[i], term, group_count, count_mode)) {
                status = 1; goto done;
            }
        }
        i = end;
    }
done:
    if (fflush(out) != 0) status = 1;
    if (close_out && fclose(out) != 0) status = 1;
cleanup:
    for (size_t i = 0; i < count; ++i) free(lines[i].data);
    free(lines);
    return status;
}

#endif
