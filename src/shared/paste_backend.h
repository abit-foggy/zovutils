#ifndef ZOVUTILS_PASTE_BACKEND_H
#define ZOVUTILS_PASTE_BACKEND_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *fp;
    int close_fp;
    int active;
    unsigned char *line;
    size_t length;
    size_t capacity;
} zov_paste_input;

static inline int zov_paste_readline(zov_paste_input *in, unsigned char term) {
    in->length = 0;
    int c;
    while ((c = fgetc(in->fp)) != EOF) {
        if ((unsigned char)c == term) return 1;
        if (in->length == in->capacity) {
            size_t next = in->capacity ? in->capacity * 2 : 4096;
            if (next < in->capacity) return -1;
            unsigned char *grown = (unsigned char *)realloc(in->line, next);
            if (grown == NULL) return -1;
            in->line = grown;
            in->capacity = next;
        }
        in->line[in->length++] = (unsigned char)c;
    }
    if (ferror(in->fp)) return -1;
    return in->length > 0 ? 1 : 0;
}

static inline unsigned char *zov_paste_decode_delimiters(const char *text,
                                                          size_t *length) {
    size_t n = strlen(text);
    unsigned char *out = (unsigned char *)malloc(n + 1);
    if (out == NULL) return NULL;
    size_t used = 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\\' && i + 1 < n) {
            c = (unsigned char)text[++i];
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
            else if (c == 'b') c = '\b';
            else if (c == 'r') c = '\r';
            else if (c == 'f') c = '\f';
            else if (c == 'v') c = '\v';
            else if (c >= '0' && c <= '7') {
                unsigned value = c - '0';
                for (int k = 0; k < 2 && i + 1 < n && text[i + 1] >= '0' && text[i + 1] <= '7'; ++k)
                    value = value * 8 + (unsigned)(text[++i] - '0');
                c = (unsigned char)value;
            }
        }
        out[used++] = c;
    }
    *length = used;
    return out;
}

static inline int zov_paste_write(const void *data, size_t length) {
    return length == 0 || fwrite(data, 1, length, stdout) == length;
}

static inline int zov_paste_delimiter(const unsigned char *delims, size_t n,
                                      size_t *index) {
    if (n == 0) return 1;
    unsigned char c = delims[*index % n];
    ++*index;
    return zov_paste_write(&c, 1);
}

static inline int zov_paste_run(size_t count, const char *const *paths,
                                const char *delimiter_text, int serial,
                                int zero_terminated) {
    size_t delimiter_count = 0;
    unsigned char *delimiters = zov_paste_decode_delimiters(delimiter_text,
                                                             &delimiter_count);
    if (delimiters == NULL) return 1;
    zov_paste_input *inputs = (zov_paste_input *)calloc(count, sizeof(*inputs));
    if (inputs == NULL) { free(delimiters); return 1; }
    unsigned char term = zero_terminated ? 0 : '\n';
    int status = 0;
    for (size_t i = 0; i < count; ++i) {
        if (strcmp(paths[i], "-") == 0) {
            inputs[i].fp = stdin;
        } else {
            inputs[i].fp = fopen(paths[i], "rb");
            inputs[i].close_fp = 1;
            if (inputs[i].fp == NULL) {
                fprintf(stderr, "paste: %s: %s\n", paths[i], strerror(errno));
                status = 1;
                continue;
            }
        }
        inputs[i].active = 1;
    }
    if (status != 0) goto done;

    if (!serial) {
        for (;;) {
            int any = 0;
            for (size_t i = 0; i < count; ++i) {
                if (!inputs[i].active) continue;
                int rc = zov_paste_readline(&inputs[i], term);
                if (rc < 0) { status = 1; inputs[i].active = 0; continue; }
                if (rc == 0) { inputs[i].active = 0; continue; }
                any = 1;
            }
            if (!any) break;
            size_t delimiter_index = 0;
            for (size_t i = 0; i < count; ++i) {
                if (i > 0 && !zov_paste_delimiter(delimiters, delimiter_count,
                                                   &delimiter_index)) { status = 1; goto done; }
                if (inputs[i].length > 0 &&
                    !zov_paste_write(inputs[i].line, inputs[i].length)) { status = 1; goto done; }
            }
            if (!zov_paste_write(&term, 1)) { status = 1; break; }
        }
    } else {
        for (size_t i = 0; i < count; ++i) {
            size_t delimiter_index = 0;
            int first = 1;
            while (inputs[i].active) {
                int rc = zov_paste_readline(&inputs[i], term);
                if (rc < 0) { status = 1; break; }
                if (rc == 0) { inputs[i].active = 0; break; }
                if (!first && !zov_paste_delimiter(delimiters, delimiter_count,
                                                    &delimiter_index)) { status = 1; goto done; }
                if (!zov_paste_write(inputs[i].line, inputs[i].length)) { status = 1; goto done; }
                first = 0;
            }
            if (!first && !zov_paste_write(&term, 1)) { status = 1; goto done; }
        }
    }

done:
    for (size_t i = 0; i < count; ++i) {
        if (inputs[i].close_fp && inputs[i].fp != NULL) fclose(inputs[i].fp);
        free(inputs[i].line);
    }
    free(inputs);
    free(delimiters);
    if (fflush(stdout) != 0) status = 1;
    return status;
}

#endif
