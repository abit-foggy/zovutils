#ifndef ZOVUTILS_PLATFORM_OD_H
#define ZOVUTILS_PLATFORM_OD_H

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline float zov_od_half_float(uint16_t bits) {
    unsigned sign = (bits >> 15) & 1;
    unsigned exponent = (bits >> 10) & 31;
    unsigned fraction = bits & 1023;
    uint32_t raw;
    if (exponent == 0) {
        if (fraction == 0) raw = sign << 31;
        else {
            int e = -14;
            while ((fraction & 1024) == 0) { fraction <<= 1; --e; }
            fraction &= 1023;
            raw = (sign << 31) | ((uint32_t)(e + 127) << 23) | (fraction << 13);
        }
    } else if (exponent == 31) raw = (sign << 31) | 0x7f800000u | (fraction << 13);
    else raw = (sign << 31) | ((exponent + 112) << 23) | (fraction << 13);
    float value; memcpy(&value, &raw, sizeof(value)); return value;
}

static inline void zov_od_float_text(float value, char *out, size_t cap) {
    uint32_t original; memcpy(&original, &value, sizeof(original));
    for (int precision = 1; precision <= 9; ++precision) {
        char candidate[64];
        snprintf(candidate, sizeof(candidate), "%.*g", precision, (double)value);
        char *end = NULL; float parsed = strtof(candidate, &end); uint32_t bits;
        memcpy(&bits, &parsed, sizeof(bits));
        if (end != candidate && *end == '\0' && bits == original) {
            snprintf(out, cap, "%s", candidate); return;
        }
    }
    snprintf(out, cap, "%.9g", (double)value);
}

static inline void zov_od_double_text(double value, char *out, size_t cap) {
    uint64_t original; memcpy(&original, &value, sizeof(original));
    for (int precision = 1; precision <= 17; ++precision) {
        char candidate[96];
        snprintf(candidate, sizeof(candidate), "%.*g", precision, value);
        char *end = NULL; double parsed = strtod(candidate, &end); uint64_t bits;
        memcpy(&bits, &parsed, sizeof(bits));
        if (end != candidate && *end == '\0' && bits == original) {
            snprintf(out, cap, "%s", candidate); return;
        }
    }
    snprintf(out, cap, "%.17g", value);
}

static inline void zov_od_long_double_text(long double value, char *out, size_t cap) {
    int max_precision = LDBL_DIG + 3;
    if (max_precision > 36) max_precision = 36;
    for (int precision = 1; precision <= max_precision; ++precision) {
        char candidate[128];
        snprintf(candidate, sizeof(candidate), "%.*Lg", precision, value);
        char *end = NULL;
        long double parsed = strtold(candidate, &end);
        if (end != candidate && *end == '\0' &&
            (parsed == value || (isnan(parsed) && isnan(value)))) {
            snprintf(out, cap, "%s", candidate);
            return;
        }
    }
    snprintf(out, cap, "%.*Lg", max_precision, value);
}

static inline void zov_od_format_half(uint16_t bits, int is_bfloat, char *out, size_t cap) {
    float f;
    if (is_bfloat) {
        uint32_t raw = (uint32_t)bits << 16;
        memcpy(&f, &raw, sizeof(f));
    } else {
        f = zov_od_half_float(bits);
    }
    zov_od_float_text(f, out, cap);
}

static inline void zov_od_format_float_bits(uint32_t bits, char *out, size_t cap) {
    float f;
    memcpy(&f, &bits, sizeof(f));
    zov_od_float_text(f, out, cap);
}

static inline void zov_od_format_double_bits(uint64_t bits, char *out, size_t cap) {
    double f;
    memcpy(&f, &bits, sizeof(f));
    zov_od_double_text(f, out, cap);
}

static inline void zov_od_format_long_double_bytes(const unsigned char *bytes, size_t available, int big_endian, char *out, size_t cap) {
    unsigned char ordered[sizeof(long double)] = {0};
    size_t copied = available < sizeof(long double) ? available : sizeof(long double);
    for (size_t i = 0; i < copied; ++i)
        ordered[big_endian ? sizeof(long double) - copied + i : i] =
            bytes[big_endian ? copied - i - 1 : i];
    long double f = 0;
    memcpy(&f, ordered, sizeof(f));
    zov_od_long_double_text(f, out, cap);
}

#endif
