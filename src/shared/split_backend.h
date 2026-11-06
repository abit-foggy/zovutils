#ifndef ZOVUTILS_SPLIT_BACKEND_H
#define ZOVUTILS_SPLIT_BACKEND_H

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct { size_t offset, length; } zov_split_record;
typedef struct { unsigned char *data; size_t length, capacity; } zov_split_buffer;

static inline int zov_split_append(zov_split_buffer *buffer, unsigned char byte) {
    if (buffer->length == buffer->capacity) {
        size_t next = buffer->capacity == 0 ? 4096 : buffer->capacity * 2;
        if (next < buffer->capacity) return 0;
        unsigned char *grown = (unsigned char *)realloc(buffer->data, next);
        if (grown == NULL) return 0;
        buffer->data = grown; buffer->capacity = next;
    }
    buffer->data[buffer->length++] = byte; return 1;
}

static inline int zov_split_load(FILE *fp, zov_split_buffer *buffer) {
    for (;;) {
        int c = fgetc(fp);
        if (c == EOF) return ferror(fp) ? 0 : 1;
        if (!zov_split_append(buffer, (unsigned char)c)) return 0;
    }
}

static inline int zov_split_records(const zov_split_buffer *buffer, unsigned char separator,
                                    zov_split_record **records, size_t *count) {
    size_t n = 0, cap = 0, start = 0;
    zov_split_record *items = NULL;
    for (size_t i = 0; i < buffer->length; ++i) if (buffer->data[i] == separator) {
        if (n == cap) {
            size_t next = cap == 0 ? 256 : cap * 2;
            if (next < cap || next > SIZE_MAX / sizeof(*items)) { free(items); return 0; }
            zov_split_record *grown = (zov_split_record *)realloc(items, next * sizeof(*items));
            if (grown == NULL) { free(items); return 0; }
            items = grown; cap = next;
        }
        items[n++] = (zov_split_record){start, i + 1 - start}; start = i + 1;
    }
    if (start < buffer->length) {
        if (n == cap) {
            size_t next = cap == 0 ? 256 : cap * 2;
            if (next < cap || next > SIZE_MAX / sizeof(*items)) { free(items); return 0; }
            zov_split_record *grown = (zov_split_record *)realloc(items, next * sizeof(*items));
            if (grown == NULL) { free(items); return 0; }
            items = grown; cap = next;
        }
        items[n++] = (zov_split_record){start, buffer->length - start};
    }
    *records = items; *count = n; return 1;
}

static inline int zov_split_suffix(char *out, size_t cap, size_t index,
                                   unsigned width, int mode, unsigned long from) {
    if (mode == 1) {
        if (from > SIZE_MAX - index) return 0;
        int n = snprintf(out, cap, "%0*zu", (int)width, (size_t)from + index);
        return n >= 0 && (size_t)n < cap;
    }
    if (mode == 2) {
        if (from > SIZE_MAX - index) return 0;
        int n = snprintf(out, cap, "%0*zx", (int)width, index + (size_t)from);
        return n >= 0 && (size_t)n < cap;
    }
    if (width == 0 || width > 32) return 0;
    size_t limit = 1;
    for (unsigned i = 0; i < width; ++i) { if (limit > SIZE_MAX / 26) { limit = SIZE_MAX; break; } limit *= 26; }
    if (index >= limit) return 0;
    for (unsigned i = 0; i < width; ++i) { out[width - i - 1] = (char)('a' + index % 26); index /= 26; }
    out[width] = '\0'; return 1;
}

static inline int zov_split_write_file(const char *prefix, const char *additional,
                                       unsigned width, int suffix_mode, unsigned long from,
                                       size_t index, const unsigned char *data, size_t length,
                                       int verbose, const char *filter) {
    char suffix[64];
    if (!zov_split_suffix(suffix, sizeof(suffix), index, width, suffix_mode, from)) {
        fprintf(stderr, "split: output file suffixes exhausted\n"); return 0;
    }
    size_t need = strlen(prefix) + strlen(suffix) + strlen(additional) + 1;
    char *name = (char *)malloc(need);
    if (name == NULL) { fprintf(stderr, "split: memory exhausted\n"); return 0; }
    snprintf(name, need, "%s%s%s", prefix, suffix, additional);
    if (filter != NULL) {
        if (verbose) printf("executing with FILE=%s\n", name);
        if (setenv("FILE", name, 1) != 0) { fprintf(stderr, "split: cannot set FILE environment variable\n"); free(name); return 0; }
        FILE *pipe = popen(filter, "w");
        if (pipe == NULL) { fprintf(stderr, "split: cannot run filter\n"); free(name); return 0; }
        int ok = length == 0 || fwrite(data, 1, length, pipe) == length;
        int result = pclose(pipe);
        free(name);
        if (!ok || result != 0) { fprintf(stderr, "split: filter command failed\n"); return 0; }
        return 1;
    }
    if (verbose) printf("creating file '%s'\n", name);
    FILE *fp = fopen(name, "wb");
    if (fp == NULL) { fprintf(stderr, "split: %s: %s\n", name, strerror(errno)); free(name); return 0; }
    int ok = length == 0 || fwrite(data, 1, length, fp) == length;
    if (fclose(fp) != 0) ok = 0;
    if (!ok) fprintf(stderr, "split: write error\n");
    free(name); return ok;
}

static inline int zov_split_run(const char *input_path, const char *prefix,
                                const char *additional, size_t width, int suffix_mode,
                                unsigned long from, int verbose, unsigned char separator,
                                int mode, uint64_t amount, uint64_t selector,
                                int round_robin, int elide_empty, const char *filter) {
    if (filter != NULL && *filter == '\0') filter = NULL;
    FILE *fp = strcmp(input_path, "-") == 0 ? stdin : fopen(input_path, "rb");
    if (fp == NULL) { fprintf(stderr, "split: %s: %s\n", input_path, strerror(errno)); return 1; }
    zov_split_buffer buffer = {0};
    int loaded = zov_split_load(fp, &buffer);
    if (fp != stdin) fclose(fp);
    if (!loaded) { free(buffer.data); fprintf(stderr, "split: read error\n"); return 1; }
    int status = 0;
    if (mode == 0) {
        if (amount == 0) { free(buffer.data); return 1; }
        size_t chunk = (size_t)amount;
        if ((uint64_t)chunk != amount) { free(buffer.data); return 1; }
        size_t files = buffer.length == 0 ? 1 : (buffer.length + chunk - 1) / chunk;
        if (selector != 0) {
            if (selector <= files) {
                size_t start = (size_t)(selector - 1) * chunk;
                size_t n = buffer.length > start ? buffer.length - start : 0;
                if (n > chunk) n = chunk;
                if (n != 0 && fwrite(buffer.data + start, 1, n, stdout) != n) status = 1;
            }
        } else for (size_t i = 0; i < files; ++i) {
            size_t start = i * chunk, n = buffer.length > start ? buffer.length - start : 0;
            if (n > chunk) n = chunk;
            const unsigned char *part = buffer.data == NULL ? NULL : buffer.data + start;
            if (!zov_split_write_file(prefix, additional, (unsigned)width, suffix_mode, from, i,
                                      part, n, verbose, filter)) { status = 1; break; }
        }
    } else if (mode == 2) {
        size_t chunks = (size_t)amount;
        if ((uint64_t)chunks != amount || chunks == 0) status = 1;
        for (size_t f = 0; !status && f < chunks; ++f) {
            if (selector != 0 && selector != f + 1) continue;
            size_t begin = (buffer.length * f + chunks - 1) / chunks;
            size_t end = (buffer.length * (f + 1) + chunks - 1) / chunks;
            size_t n = end - begin;
            if (selector != 0) {
                if (n != 0 && fwrite(buffer.data + begin, 1, n, stdout) != n) status = 1;
            } else if (!(elide_empty && n == 0)) {
                const unsigned char *part = buffer.data == NULL ? NULL : buffer.data + begin;
                if (!zov_split_write_file(prefix, additional, (unsigned)width, suffix_mode, from, f, part, n, verbose, filter)) status = 1;
            }
        }
    } else {
        zov_split_record *records = NULL; size_t count = 0;
        if (!zov_split_records(&buffer, separator, &records, &count)) { free(buffer.data); return 1; }
        if (mode == 1) {
            size_t per = (size_t)amount;
            size_t files = count == 0 ? 1 : (count + per - 1) / per;
            if (selector != 0) {
                size_t start = (size_t)(selector - 1) * per;
                size_t end = start + per; if (end > count) end = count;
                for (size_t i = start; i < end; ++i)
                    if (fwrite(buffer.data + records[i].offset, 1, records[i].length, stdout) != records[i].length) status = 1;
            } else for (size_t f = 0; f < files; ++f) {
                size_t start = f * per, end = start + per; if (end > count) end = count;
                size_t begin_byte = start < count ? records[start].offset : buffer.length;
                size_t end_byte = end > start ? records[end - 1].offset + records[end - 1].length : begin_byte;
                const unsigned char *part = buffer.data == NULL ? NULL : buffer.data + begin_byte;
                if (!zov_split_write_file(prefix, additional, (unsigned)width, suffix_mode, from, f,
                                          part, end_byte - begin_byte, verbose, filter)) { status = 1; break; }
            }
        } else if (mode == 4) {
            size_t chunks = (size_t)amount;
            if (round_robin) {
                for (size_t f = 0; f < chunks; ++f) {
                    if (selector != 0 && selector != f + 1) continue;
                    size_t total = 0;
                    for (size_t r = f; r < count; r += chunks) total += records[r].length;
                    unsigned char *chunk = (unsigned char *)malloc(total == 0 ? 1 : total);
                    if (chunk == NULL) { status = 1; break; }
                    size_t used = 0;
                    for (size_t r = f; r < count; r += chunks) {
                        memcpy(chunk + used, buffer.data + records[r].offset, records[r].length); used += records[r].length;
                    }
                    if (selector != 0) { if (used != 0 && fwrite(chunk, 1, used, stdout) != used) status = 1; }
                    else if (!(elide_empty && used == 0) && !zov_split_write_file(prefix, additional, (unsigned)width, suffix_mode, from, f, chunk, used, verbose, filter)) status = 1;
                    free(chunk); if (status) break;
                }
            }
        } else if (mode == 5) {
            size_t chunks = (size_t)amount;
            for (size_t f = 0; f < chunks; ++f) {
                size_t start = (count * f + chunks - 1) / chunks, end = (count * (f + 1) + chunks - 1) / chunks;
                if (selector != 0 && selector != f + 1) continue;
                size_t begin_byte = start < count ? records[start].offset : buffer.length;
                size_t end_byte = end > start ? records[end - 1].offset + records[end - 1].length : begin_byte;
                size_t n = end_byte - begin_byte;
                if (selector != 0) { if (n != 0 && fwrite(buffer.data + begin_byte, 1, n, stdout) != n) status = 1; }
                else if (!(elide_empty && n == 0)) {
                    const unsigned char *part = buffer.data == NULL ? NULL : buffer.data + begin_byte;
                    if (!zov_split_write_file(prefix, additional, (unsigned)width, suffix_mode, from, f, part, n, verbose, filter)) { status = 1; break; }
                }
            }
        } else if (mode == 3) {
            size_t max_bytes = (size_t)amount, start = 0, files = 0;
            while (start < count || (count == 0 && files == 0)) {
                size_t end = start, bytes = 0;
                while (end < count && (bytes == 0 || bytes + records[end].length <= max_bytes)) {
                    if (bytes != 0 && bytes + records[end].length > max_bytes) break;
                    bytes += records[end].length; ++end;
                    if (bytes >= max_bytes) break;
                }
                size_t begin_byte = start < count ? records[start].offset : buffer.length;
                size_t end_byte = end > start ? records[end - 1].offset + records[end - 1].length : begin_byte;
                const unsigned char *part = buffer.data == NULL ? NULL : buffer.data + begin_byte;
                if (!zov_split_write_file(prefix, additional, (unsigned)width, suffix_mode, from, files++,
                                          part, end_byte - begin_byte, verbose, filter)) { status = 1; break; }
                if (end == start) break;
                start = end;
            }
        }
        free(records);
    }
    if (fflush(stdout) != 0) status = 1;
    free(buffer.data); return status;
}

#endif
