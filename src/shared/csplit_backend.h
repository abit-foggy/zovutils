#ifndef ZOVUTILS_CSPLIT_BACKEND_H
#define ZOVUTILS_CSPLIT_BACKEND_H

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct { size_t offset, length; } zov_csplit_line;
typedef struct { char *text; char delimiter; long offset; int numeric; unsigned long number; size_t expression_length; } zov_csplit_pattern;
typedef struct { char *data; size_t length, capacity; } zov_csplit_bytes;
typedef struct { char **paths; size_t count, capacity; } zov_csplit_paths;

static inline int zov_csplit_append(zov_csplit_bytes *b, unsigned char c) {
    if (b->length == b->capacity) {
        size_t next = b->capacity == 0 ? 4096 : b->capacity * 2;
        if (next < b->capacity) return 0;
        char *grown = (char *)realloc(b->data, next);
        if (grown == NULL) return 0;
        b->data = grown; b->capacity = next;
    }
    b->data[b->length++] = (char)c;
    return 1;
}

static inline int zov_csplit_load(FILE *fp, unsigned char terminator,
                                  zov_csplit_bytes *bytes,
                                  zov_csplit_line **lines, size_t *count) {
    zov_csplit_bytes input = {0};
    size_t line_count = 0, line_capacity = 0, line_start = 0;
    zov_csplit_line *items = NULL;
    for (;;) {
        int c = fgetc(fp);
        if (c == EOF) {
            if (ferror(fp)) goto fail;
            if (input.length > line_start) {
                if (line_count == line_capacity) {
                    size_t next = line_capacity == 0 ? 256 : line_capacity * 2;
                    zov_csplit_line *p = (zov_csplit_line *)realloc(items, next * sizeof(*items));
                    if (p == NULL) goto fail;
                    items = p; line_capacity = next;
                }
                items[line_count++] = (zov_csplit_line){line_start, input.length - line_start};
            }
            break;
        }
        if (!zov_csplit_append(&input, (unsigned char)c)) goto fail;
        if ((unsigned char)c == terminator) {
            if (line_count == line_capacity) {
                size_t next = line_capacity == 0 ? 256 : line_capacity * 2;
                if (next < line_capacity || next > SIZE_MAX / sizeof(*items)) goto fail;
                zov_csplit_line *p = (zov_csplit_line *)realloc(items, next * sizeof(*items));
                if (p == NULL) goto fail;
                items = p; line_capacity = next;
            }
            items[line_count++] = (zov_csplit_line){line_start, input.length - line_start};
            line_start = input.length;
        }
    }
    *bytes = input; *lines = items; *count = line_count; return 1;
fail:
    free(input.data); free(items); return 0;
}

static inline int zov_csplit_parse_unsigned(const char *s, unsigned long *out) {
    if (*s == '\0') return 0;
    unsigned long n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        if (*p < '0' || *p > '9') return 0;
        unsigned d = *p - '0';
        if (n > (ULONG_MAX - d) / 10) return 0;
        n = n * 10 + d;
    }
    *out = n; return 1;
}

static inline int zov_csplit_parse_pattern(const char *text, zov_csplit_pattern *out) {
    memset(out, 0, sizeof(*out)); out->text = (char *)text;
    if (text[0] == '/' || text[0] == '%') {
        char delim = text[0]; size_t end = 1;
        while (text[end] != '\0') {
            if (text[end] == '\\' && text[end + 1] != '\0') { end += 2; continue; }
            if (text[end] == delim) break;
            ++end;
        }
        if (text[end] != delim) return 0;
        out->delimiter = delim;
        out->expression_length = end - 1;
        char *rest = (char *)text + end + 1;
        if (*rest != '\0') {
            char *tail = rest; int sign = 1;
            if (*tail == '+') ++tail;
            else if (*tail == '-') { sign = -1; ++tail; }
            unsigned long magnitude;
            if (!zov_csplit_parse_unsigned(tail, &magnitude) || magnitude > LONG_MAX) return 0;
            out->offset = sign * (long)magnitude;
        }
        return 1;
    }
    out->numeric = 1;
    return zov_csplit_parse_unsigned(text, &out->number);
}

static inline int zov_csplit_regex_match(const zov_csplit_pattern *pattern,
                                         const char *line, size_t length) {
    size_t expression_length = pattern->expression_length;
    char *expression = (char *)malloc(expression_length + 1);
    char *text = (char *)malloc(length + 1);
    if (expression == NULL || text == NULL) { free(expression); free(text); return -1; }
    memcpy(expression, pattern->text + 1, expression_length); expression[expression_length] = '\0';
    memcpy(text, line, length); text[length] = '\0';
    regex_t regex; int rc = regcomp(&regex, expression, 0);
    free(expression);
    if (rc != 0) { free(text); return -2; }
    rc = regexec(&regex, text, 0, NULL, 0);
    regfree(&regex); free(text);
    return rc == 0 ? 1 : rc == REG_NOMATCH ? 0 : -2;
}

static inline int zov_csplit_find(const zov_csplit_pattern *pattern,
                                  const zov_csplit_bytes *bytes,
                                  const zov_csplit_line *lines, size_t count,
                                  size_t start, size_t *match) {
    for (size_t i = start; i < count; ++i) {
        size_t n = lines[i].length;
        if (n != 0 && bytes->data[lines[i].offset + n - 1] == '\n') --n;
        int found = zov_csplit_regex_match(pattern, bytes->data + lines[i].offset, n);
        if (found < 0) return found;
        if (found) { *match = i; return 1; }
    }
    return 0;
}

static inline int zov_csplit_path_push(zov_csplit_paths *paths, char *path) {
    if (paths->count == paths->capacity) {
        size_t next = paths->capacity == 0 ? 16 : paths->capacity * 2;
        if (next < paths->capacity || next > SIZE_MAX / sizeof(*paths->paths)) return 0;
        char **grown = (char **)realloc(paths->paths, next * sizeof(*grown));
        if (grown == NULL) return 0;
        paths->paths = grown; paths->capacity = next;
    }
    paths->paths[paths->count++] = path; return 1;
}

static inline int zov_csplit_suffix_conversion(const char *format, int *is_unsigned) {
    unsigned conversions = 0;
    for (const char *p = format; *p; ++p) {
        if (*p != '%') continue;
        ++p;
        if (*p == '%') continue;
        while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0') ++p;
        while (*p >= '0' && *p <= '9') ++p;
        if (*p == '.') { ++p; while (*p >= '0' && *p <= '9') ++p; }
        if (*p == 'd' || *p == 'i') { *is_unsigned = 0; ++conversions; }
        else if (*p == 'u' || *p == 'o' || *p == 'x' || *p == 'X') { *is_unsigned = 1; ++conversions; }
        else return 0;
    }
    return conversions == 1;
}

static inline int zov_csplit_write_piece(const char *prefix, const char *suffix_format,
                                         unsigned digits, size_t index,
                                         const zov_csplit_bytes *bytes,
                                         const zov_csplit_line *lines,
                                         size_t first, size_t end, const unsigned char *suppressed,
                                         int elide_empty, int silent,
                                         zov_csplit_paths *created) {
    size_t length = 0;
    for (size_t i = first; i < end; ++i) if (suppressed == NULL || !suppressed[i]) length += lines[i].length;
    if (length == 0 && elide_empty) return 1;
    char suffix[256];
    if (suffix_format != NULL) {
        int is_unsigned = 0;
        if (!zov_csplit_suffix_conversion(suffix_format, &is_unsigned)) return 0;
        int n = is_unsigned ? snprintf(suffix, sizeof(suffix), suffix_format, (unsigned)index) :
                             snprintf(suffix, sizeof(suffix), suffix_format, (int)index);
        if (n < 0 || (size_t)n >= sizeof(suffix)) return 0;
    } else {
        int n = snprintf(suffix, sizeof(suffix), "%0*zu", (int)digits, index);
        if (n < 0 || (size_t)n >= sizeof(suffix)) return 0;
    }
    size_t path_length = strlen(prefix) + strlen(suffix) + 1;
    char *path = (char *)malloc(path_length);
    if (path == NULL) return 0;
    snprintf(path, path_length, "%s%s", prefix, suffix);
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) { fprintf(stderr, "csplit: %s: %s\n", path, strerror(errno)); free(path); return 0; }
    int ok = 1;
    for (size_t i = first; i < end; ++i) if ((suppressed == NULL || !suppressed[i]) && lines[i].length != 0)
        if (fwrite(bytes->data + lines[i].offset, 1, lines[i].length, fp) != lines[i].length) { ok = 0; break; }
    if (fclose(fp) != 0) ok = 0;
    if (!zov_csplit_path_push(created, path)) { unlink(path); free(path); return 0; }
    if (!ok) { fprintf(stderr, "csplit: write error\n"); return 0; }
    if (!silent) printf("%zu\n", length);
    return 1;
}

static inline int zov_csplit_run(const char *path, const char *prefix,
                                 const char *suffix_format, unsigned digits,
                                 int keep_files, int silent, int suppress_matched,
                                 int elide_empty, unsigned char terminator,
                                 const void *pattern_args_raw, size_t pattern_count) {
    (void)setlocale(LC_ALL, "");
    const char *const *pattern_args = (const char *const *)pattern_args_raw;
    if (suffix_format != NULL && *suffix_format == '\0') suffix_format = NULL;
    FILE *fp = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
    if (fp == NULL) { fprintf(stderr, "csplit: %s: %s\n", path, strerror(errno)); return 1; }
    zov_csplit_bytes bytes = {0}; zov_csplit_line *lines = NULL; size_t line_count = 0;
    int loaded = zov_csplit_load(fp, terminator, &bytes, &lines, &line_count);
    if (fp != stdin) fclose(fp);
    if (!loaded) { fprintf(stderr, "csplit: read error: %s\n", path); return 1; }
    unsigned char *suppressed = line_count == 0 ? NULL : (unsigned char *)calloc(line_count, 1);
    if (line_count != 0 && suppressed == NULL) { free(lines); free(bytes.data); fprintf(stderr, "csplit: memory exhausted\n"); return 1; }
    zov_csplit_paths created = {0};
    size_t cursor = 0, output_index = 0; long previous_number = 0;
    int status = 0;
    zov_csplit_pattern previous = {0}; int have_previous = 0;
    size_t previous_search_start = 0; int previous_has_match = 0;
    size_t next_pattern_search_start = 0; int use_next_pattern_search = 0;
    for (size_t pi = 0; pi < pattern_count && status == 0; ++pi) {
        const char *arg = pattern_args[pi];
        if (arg[0] == '{') {
            size_t len = strlen(arg); int star = len == 3 && arg[1] == '*' && arg[2] == '}';
            if (!have_previous || len < 3 || arg[len - 1] != '}') { fprintf(stderr, "csplit: invalid repeat count: '%s'\n", arg); status = 1; break; }
            unsigned long repeat = 0;
            if (!star) {
                char *count = (char *)malloc(len - 1);
                if (count == NULL) { status = 1; break; }
                memcpy(count, arg + 1, len - 2); count[len - 2] = '\0';
                if (!zov_csplit_parse_unsigned(count, &repeat)) { fprintf(stderr, "csplit: invalid repeat count: '%s'\n", arg); free(count); status = 1; break; }
                free(count);
            }
            if (!star && repeat == 0) continue;
            unsigned long done = 0;
            size_t search_start = previous_has_match ? previous_search_start : cursor;
            do {
                size_t boundary = cursor; long matched = -1;
                if (previous.numeric) {
                    fprintf(stderr, "csplit: '%s': line number out of range on repetition 1\n", previous.text);
                    status = 1; break;
                } else {
                    size_t found = 0; int match = zov_csplit_find(&previous, &bytes, lines, line_count, search_start, &found);
                    if (match == -2) { fprintf(stderr, "csplit: invalid regular expression: '%s'\n", previous.text); status = 1; break; }
                    if (match < 0) { fprintf(stderr, "csplit: memory exhausted\n"); status = 1; break; }
                    if (!match) { if (star) break; fprintf(stderr, "csplit: '%s': match not found\n", previous.text); status = 1; break; }
                    long b = (long)found + previous.offset;
                    if (b < 0 || (unsigned long)b > line_count) { fprintf(stderr, "csplit: '%s': line number out of range\n", previous.text); status = 1; break; }
                    boundary = (size_t)b; matched = (long)found;
                    search_start = found + 1;
                    previous_search_start = search_start; previous_has_match = 1;
                    if (previous.delimiter == '%') {
                        cursor = boundary; next_pattern_search_start = search_start;
                        use_next_pattern_search = 1; ++done; continue;
                    }
                }
                if (suppress_matched && matched >= 0) suppressed[matched] = 1;
                if (!zov_csplit_write_piece(prefix, suffix_format, digits, output_index, &bytes, lines,
                        cursor, boundary, suppressed, elide_empty, silent, &created)) { status = 1; break; }
                if (created.count > output_index) ++output_index;
                cursor = boundary;
                ++done;
            } while (star || done < repeat);
            if (status != 0) break;
            continue;
        }
        if (!zov_csplit_parse_pattern(arg, &previous)) { fprintf(stderr, "csplit: invalid pattern: '%s'\n", arg); status = 1; break; }
        have_previous = 1;
        size_t boundary = cursor; long matched = -1;
        if (previous.numeric) {
            unsigned long number = previous.number;
            if (number == 0 || number > line_count + 1) { fprintf(stderr, "csplit: '%s': line number out of range\n", arg); status = 1; break; }
            if ((long)number < previous_number) { fprintf(stderr, "csplit: line number '%s' is smaller than preceding line number, %ld\n", arg, previous_number); status = 1; break; }
            previous_number = (long)number; boundary = (size_t)number - 1;
        } else {
            size_t search_from = use_next_pattern_search ? next_pattern_search_start : cursor;
            use_next_pattern_search = 0;
            size_t found = 0; int match = zov_csplit_find(&previous, &bytes, lines, line_count, search_from, &found);
            if (match == -2) { fprintf(stderr, "csplit: invalid regular expression: '%s'\n", arg); status = 1; break; }
            if (match < 0) { fprintf(stderr, "csplit: memory exhausted\n"); status = 1; break; }
            if (!match) { fprintf(stderr, "csplit: '%s': match not found\n", arg); status = 1; break; }
            long b = (long)found + previous.offset;
            if (b < 0 || (unsigned long)b > line_count) { fprintf(stderr, "csplit: '%s': line number out of range\n", arg); status = 1; break; }
            boundary = (size_t)b; matched = (long)found;
            previous_search_start = found + 1; previous_has_match = 1;
            if (previous.delimiter == '%') { cursor = boundary; next_pattern_search_start = found + 1; use_next_pattern_search = 1; continue; }
        }
        if (suppress_matched && matched >= 0) suppressed[matched] = 1;
        if (!zov_csplit_write_piece(prefix, suffix_format, digits, output_index, &bytes, lines,
                cursor, boundary, suppressed, elide_empty, silent, &created)) { status = 1; break; }
        if (created.count > output_index) ++output_index;
        cursor = boundary;
    }
    if (status == 0 && !zov_csplit_write_piece(prefix, suffix_format, digits, output_index,
            &bytes, lines, cursor, line_count, suppressed, elide_empty, silent, &created)) status = 1;
    if (status != 0 && !keep_files) for (size_t i = 0; i < created.count; ++i) unlink(created.paths[i]);
    for (size_t i = 0; i < created.count; ++i) free(created.paths[i]);
    free(created.paths); free(suppressed); free(lines); free(bytes.data);
    if (fflush(stdout) != 0) status = 1;
    return status;
}

#endif
