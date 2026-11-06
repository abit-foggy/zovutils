#ifndef ZOVUTILS_TAC_BACKEND_H
#define ZOVUTILS_TAC_BACKEND_H

#include <errno.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { size_t start, length; } zov_tac_span;
typedef struct { regex_t expression; size_t max_span; } zov_tac_regex;
typedef struct { unsigned char *data; size_t length, capacity; } zov_tac_buffer;

static inline int zov_tac_append(zov_tac_buffer *buffer, unsigned char byte) {
    if (buffer->length == buffer->capacity) {
        size_t next = buffer->capacity == 0 ? 4096 : buffer->capacity * 2;
        if (next < buffer->capacity) return 0;
        unsigned char *grown = (unsigned char *)realloc(buffer->data, next);
        if (grown == NULL) return 0;
        buffer->data = grown;
        buffer->capacity = next;
    }
    buffer->data[buffer->length++] = byte;
    return 1;
}

static inline int zov_tac_read(FILE *fp, zov_tac_buffer *buffer) {
    for (;;) {
        int byte = fgetc(fp);
        if (byte == EOF) return ferror(fp) ? 0 : 1;
        if (!zov_tac_append(buffer, (unsigned char)byte)) return 0;
    }
}

static inline int zov_tac_span_push(zov_tac_span **spans, size_t *count,
                                    size_t *capacity, size_t start, size_t length) {
    if (*count == *capacity) {
        size_t next = *capacity == 0 ? 128 : *capacity * 2;
        if (next < *capacity || next > (size_t)-1 / sizeof(**spans)) return 0;
        zov_tac_span *grown = (zov_tac_span *)realloc(*spans, next * sizeof(**spans));
        if (grown == NULL) return 0;
        *spans = grown;
        *capacity = next;
    }
    (*spans)[(*count)++] = (zov_tac_span){start, length};
    return 1;
}

static inline size_t zov_tac_find(const unsigned char *data, size_t length,
                                  size_t start, const unsigned char *separator,
                                  size_t separator_length) {
    if (separator_length == 0 || start > length || separator_length > length - start) return length;
    for (size_t i = start; i <= length - separator_length; ++i)
        if (memcmp(data + i, separator, separator_length) == 0) return i;
    return length;
}

static inline int zov_tac_write_regex_reversed(FILE *fp, const unsigned char *data,
                                               size_t length, int before, zov_tac_regex *regex) {
    if (length == 0) return 1;
    char *candidate = (char *)malloc(length + 1);
    if (candidate == NULL) return 0;
    size_t edge = length, end = length;
    while (end > 0) {
        size_t match_start = end, match_end = end;
        size_t max_length = end < regex->max_span ? end : regex->max_span;
        /* GNU tac searches backward and uses the shortest separator at each end. */
        for (size_t span = 1; span <= max_length; ++span) {
            size_t start = end - span;
            memcpy(candidate, data + start, span);
            for (size_t i = 0; i < span; ++i) if (candidate[i] == '\0') candidate[i] = '\1';
            candidate[span] = '\0';
            regmatch_t match;
            if (regexec(&regex->expression, candidate, 1, &match, 0) == 0 &&
                match.rm_so == 0 && match.rm_eo == (regoff_t)span) {
                match_start = start;
                match_end = end;
                break;
            }
        }
        if (match_start == end) { --end; continue; }
        size_t start = before ? match_start : match_end;
        size_t stop = before ? edge : edge;
        if (stop > start && fwrite(data + start, 1, stop - start, fp) != stop - start) {
            free(candidate);
            return 0;
        }
        edge = before ? match_start : match_end;
        end = match_start;
    }
    if (edge != 0 && fwrite(data, 1, edge, fp) != edge) {
        free(candidate);
        return 0;
    }
    free(candidate);
    return !ferror(fp);
}

static inline int zov_tac_write_reversed(FILE *fp, const unsigned char *data, size_t length,
                                         const unsigned char *separator, size_t separator_length,
                                         int before, zov_tac_regex *regex, int use_regex) {
    if (use_regex) return zov_tac_write_regex_reversed(fp, data, length, before, regex);
    zov_tac_span *spans = NULL;
    size_t count = 0, capacity = 0, cursor = 0, segment_start = 0;
    while (cursor < length) {
        size_t match_start = zov_tac_find(data, length, cursor, separator, separator_length);
        if (match_start == length) break;
        size_t match_end = match_start + separator_length;
        if (before) {
            if (!zov_tac_span_push(&spans, &count, &capacity, segment_start, match_start - segment_start)) goto fail;
            segment_start = match_start;
        } else {
            if (!zov_tac_span_push(&spans, &count, &capacity, segment_start, match_end - segment_start)) goto fail;
            segment_start = match_end;
        }
        cursor = match_end;
    }
    if (segment_start < length || count == 0) {
        if (!zov_tac_span_push(&spans, &count, &capacity, segment_start, length - segment_start)) goto fail;
    }
    while (count > 0) {
        zov_tac_span span = spans[--count];
        if (span.length != 0 && fwrite(data + span.start, 1, span.length, fp) != span.length) goto fail;
    }
    free(spans);
    return !ferror(fp);
fail:
    free(spans);
    return 0;
}

static inline int zov_tac_file_inner(const char *path, const unsigned char *separator,
                                     size_t separator_length, int before, void *regex,
                                     int use_regex) {
    FILE *fp = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "tac: failed to open '%s' for reading: %s\n", path, strerror(errno));
        return 1;
    }
    zov_tac_buffer buffer = {0};
    int ok = zov_tac_read(fp, &buffer);
    if (fp != stdin && fclose(fp) != 0) ok = 0;
    if (!ok) {
        fprintf(stderr, "tac: read error: %s\n", path);
        free(buffer.data);
        return 1;
    }
    ok = zov_tac_write_reversed(stdout, buffer.data, buffer.length, separator,
                                separator_length, before, (zov_tac_regex *)regex, use_regex);
    free(buffer.data);
    if (!ok) {
        fprintf(stderr, "tac: write error\n");
        return 1;
    }
    return 0;
}

static inline int zov_tac_file_plain(const char *path, const unsigned char *separator,
                                     size_t separator_length, int before) {
    return zov_tac_file_inner(path, separator, separator_length, before, NULL, 0);
}

static inline int zov_tac_file_regex(const char *path, const unsigned char *separator,
                                     size_t separator_length, int before, void *regex) {
    return zov_tac_file_inner(path, separator, separator_length, before, regex, 1);
}

static inline void *zov_tac_regex_init(const char *pattern, char *error, size_t error_size) {
    zov_tac_regex *compiled = (zov_tac_regex *)malloc(sizeof(*compiled));
    if (compiled == NULL) return NULL;
    /* GNU tac's regex mode does not recognize POSIX named character classes. */
    const char *source = strstr(pattern, "[:") == NULL ? pattern : "a^";
    compiled->max_span = strlen(pattern) + 1;
    int code = regcomp(&compiled->expression, source, REG_EXTENDED | REG_NEWLINE);
    if (code != 0) {
        regerror(code, &compiled->expression, error, error_size);
        free(compiled);
        return NULL;
    }
    return (void *)compiled;
}

static inline void zov_tac_regex_free(void *raw) {
    zov_tac_regex *compiled = (zov_tac_regex *)raw;
    if (compiled != NULL) { regfree(&compiled->expression); free(compiled); }
}

#endif
