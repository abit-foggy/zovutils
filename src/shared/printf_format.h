#ifndef ZOVUTILS_PRINTF_FORMAT_H
#define ZOVUTILS_PRINTF_FORMAT_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static inline void zov_printf_append_digits(char *buffer, size_t *length,
                                            unsigned long long value) {
    char digits[32];
    size_t count = 0;
    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0 && count < sizeof digits);
    while (count != 0) buffer[(*length)++] = digits[--count];
}

static inline long zov_printf_star_value(const char *argument, int has_argument,
                                         int *status) {
    if (!has_argument) return 0;
    char *end = NULL;
    errno = 0;
    long value = strtol(argument, &end, 0);
    if (end == argument) *status = 1;
    else if (*end != '\0') *status = 2;
    else if (errno == ERANGE) *status = 3;
    return value;
}

/* Render one already-parsed C conversion with an argument from argv. */
static inline char *zov_printf_render(const char *spec, const char *argument,
                                     int has_argument, const char *width_argument,
                                     int has_width, const char *precision_argument,
                                     int has_precision, size_t *length,
                                     int *conversion_status) {
    size_t spec_length = strlen(spec);
    if (spec_length < 2 || spec_length >= 128) {
        *conversion_status = 4;
        return NULL;
    }
    char conversion = spec[spec_length - 1];
    *conversion_status = 0;
    int integer = conversion == 'd' || conversion == 'i' || conversion == 'o' ||
                  conversion == 'u' || conversion == 'x' || conversion == 'X';
    int floating = conversion == 'f' || conversion == 'F' || conversion == 'e' ||
                   conversion == 'E' || conversion == 'g' || conversion == 'G';
    if (!integer && !floating && conversion != 'c' && conversion != 's') {
        *conversion_status = 4;
        return NULL;
    }

    char normalized[128];
    size_t out = 0;
    int after_dot = 0;
    for (size_t i = 0; i + 1 < spec_length; ++i) {
        char ch = spec[i];
        if (ch == '.') {
            normalized[out++] = ch;
            after_dot = 1;
            continue;
        }
        if (ch == '*') {
            int star_status = 0;
            long value = after_dot
                ? zov_printf_star_value(precision_argument, has_precision, &star_status)
                : zov_printf_star_value(width_argument, has_width, &star_status);
            if (star_status != 0 && *conversion_status == 0)
                *conversion_status = star_status + (after_dot ? 7 : 4);
            if (after_dot && value < 0) {
                if (out != 0 && normalized[out - 1] == '.') --out;
            } else {
                if (!after_dot && value < 0) {
                    normalized[out++] = '-';
                    value = -value;
                }
                zov_printf_append_digits(normalized, &out, (unsigned long long)value);
            }
            after_dot = 0;
            continue;
        }
        if (ch == 'h' || ch == 'l' || ch == 'j' || ch == 'z' || ch == 't' || ch == 'L') {
            continue;
        }
        normalized[out++] = ch;
    }
    if (integer) {
        normalized[out++] = 'l';
        normalized[out++] = 'l';
    } else if (floating) {
        normalized[out++] = 'L';
    }
    normalized[out++] = conversion;
    normalized[out] = '\0';

    const char *arg = argument == NULL ? "" : argument;
    int needed = -1;
    if (conversion == 's') {
        needed = snprintf(NULL, 0, normalized, arg);
    } else if (conversion == 'c') {
        unsigned char value = has_argument && arg[0] != '\0' ? (unsigned char)arg[0] : 0;
        needed = snprintf(NULL, 0, normalized, (int)value);
    } else if (floating) {
        char *end = NULL;
        errno = 0;
        long double value = has_argument ? strtold(arg, &end) : 0.0L;
        if (has_argument && end == arg && *conversion_status == 0) *conversion_status = 1;
        else if (has_argument && *end != '\0' && *conversion_status == 0) *conversion_status = 2;
        else if (errno == ERANGE && *conversion_status == 0) *conversion_status = 3;
        needed = snprintf(NULL, 0, normalized, value);
    } else if (conversion == 'd' || conversion == 'i') {
        char *end = NULL;
        errno = 0;
        long long value = has_argument ? strtoll(arg, &end, 0) : 0;
        if (has_argument && end == arg && *conversion_status == 0) *conversion_status = 1;
        else if (has_argument && *end != '\0' && *conversion_status == 0) *conversion_status = 2;
        else if (errno == ERANGE && *conversion_status == 0) *conversion_status = 3;
        needed = snprintf(NULL, 0, normalized, value);
    } else {
        char *end = NULL;
        errno = 0;
        unsigned long long value = has_argument ? strtoull(arg, &end, 0) : 0;
        if (has_argument && end == arg && *conversion_status == 0) *conversion_status = 1;
        else if (has_argument && *end != '\0' && *conversion_status == 0) *conversion_status = 2;
        else if (errno == ERANGE && *conversion_status == 0) *conversion_status = 3;
        needed = snprintf(NULL, 0, normalized, value);
    }
    if (needed < 0) return NULL;
    char *buffer = (char *)malloc((size_t)needed + 1);
    if (buffer == NULL) return NULL;
    if (conversion == 's') {
        snprintf(buffer, (size_t)needed + 1, normalized, arg);
    } else if (conversion == 'c') {
        unsigned char value = has_argument && arg[0] != '\0' ? (unsigned char)arg[0] : 0;
        snprintf(buffer, (size_t)needed + 1, normalized, (int)value);
    } else if (floating) {
        char *end = NULL;
        long double value = has_argument ? strtold(arg, &end) : 0.0L;
        snprintf(buffer, (size_t)needed + 1, normalized, value);
    } else if (conversion == 'd' || conversion == 'i') {
        long long value = has_argument ? strtoll(arg, NULL, 0) : 0;
        snprintf(buffer, (size_t)needed + 1, normalized, value);
    } else {
        unsigned long long value = has_argument ? strtoull(arg, NULL, 0) : 0;
        snprintf(buffer, (size_t)needed + 1, normalized, value);
    }
    *length = (size_t)needed;
    return buffer;
}

#endif
