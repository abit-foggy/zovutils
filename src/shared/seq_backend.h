#ifndef ZOVUTILS_SEQ_BACKEND_H
#define ZOVUTILS_SEQ_BACKEND_H

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int zov_seq_parse_number(const char *text, double *value) {
    char *end = NULL;
    errno = 0;
    double parsed = strtod(text, &end);
    if (end == text || *end != '\0') return 1;
    if (isnan(parsed)) return 2;
    if (errno == ERANGE) return 3;
    *value = parsed;
    return 0;
}

static inline int zov_seq_validate_format(const char *format, int *bad_conversion) {
    int conversions = 0;
    *bad_conversion = 0;
    for (size_t i = 0; format[i] != '\0'; ++i) {
        if (format[i] != '%') continue;
        ++i;
        if (format[i] == '\0') {
            *bad_conversion = (int)'\0';
            return 4;
        }
        if (format[i] == '%') continue;
        ++conversions;
        while (format[i] != '\0' && strchr("-+ #0'", format[i]) != NULL) ++i;
        while (format[i] >= '0' && format[i] <= '9') ++i;
        if (format[i] == '$') {
            *bad_conversion = (int)'$';
            return 2;
        }
        if (format[i] == '.') {
            ++i;
            while (format[i] >= '0' && format[i] <= '9') ++i;
        }
        if (format[i] == 'L' || format[i] == 'l') ++i;
        if (format[i] == '\0' || strchr("aAeEfFgG", format[i]) == NULL) {
            *bad_conversion = (unsigned char)format[i];
            return 2;
        }
        if (conversions > 1) return 3;
    }
    return conversions == 0 ? 1 : 0;
}

static inline char *zov_seq_normalize_format(const char *format) {
    size_t length = strlen(format);
    char *result = (char *)malloc(length + 1);
    if (result == NULL) return NULL;
    size_t out = 0;
    for (size_t i = 0; i < length; ++i) {
        result[out++] = format[i];
        if (format[i] != '%') continue;
        if (i + 1 < length && format[i + 1] == '%') {
            result[out++] = format[++i];
            continue;
        }
        ++i;
        while (i < length && strchr("-+ #0'", format[i]) != NULL) result[out++] = format[i++];
        while (i < length && format[i] >= '0' && format[i] <= '9') result[out++] = format[i++];
        if (i < length && format[i] == '.') {
            result[out++] = format[i++];
            while (i < length && format[i] >= '0' && format[i] <= '9') result[out++] = format[i++];
        }
        if (i < length && (format[i] == 'L' || format[i] == 'l')) ++i;
        if (i < length) result[out++] = format[i];
    }
    result[out] = '\0';
    return result;
}

static inline char *zov_seq_render_format(const char *format, double value) {
    int needed = snprintf(NULL, 0, format, value);
    if (needed < 0) return NULL;
    char *result = (char *)malloc((size_t)needed + 1);
    if (result == NULL) return NULL;
    snprintf(result, (size_t)needed + 1, format, value);
    return result;
}

static inline char *zov_seq_render_default(double value, int fixed, int precision,
                                           int width) {
    const char *format = fixed ? (width > 0 ? "%0*.*f" : "%.*f")
                               : (width > 0 ? "%0*.*g" : "%.*g");
    int needed = width > 0
        ? snprintf(NULL, 0, format, width, precision, value)
        : snprintf(NULL, 0, format, precision, value);
    if (needed < 0) return NULL;
    char *result = (char *)malloc((size_t)needed + 1);
    if (result == NULL) return NULL;
    if (width > 0) snprintf(result, (size_t)needed + 1, format, width, precision, value);
    else snprintf(result, (size_t)needed + 1, format, precision, value);
    return result;
}

static inline int zov_seq_default_width(double first, double last, int fixed,
                                        int precision) {
    char *a = zov_seq_render_default(first, fixed, precision, 0);
    char *b = zov_seq_render_default(last, fixed, precision, 0);
    if (a == NULL || b == NULL) {
        free(a);
        free(b);
        return 0;
    }
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    free(a);
    free(b);
    size_t width = alen > blen ? alen : blen;
    return width > (size_t)2147483647 ? 2147483647 : (int)width;
}

#endif
