#ifndef ZOVUTILS_OD_BACKEND_H
#define ZOVUTILS_OD_BACKEND_H

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

typedef struct { unsigned char *data; size_t length, capacity; } zov_od_buffer;
typedef struct { char kind; size_t size; int strings; int bfloat; } zov_od_type;

static inline int zov_od_append(zov_od_buffer *b, unsigned char byte) {
    if (b->length == b->capacity) {
        size_t cap = b->capacity == 0 ? 4096 : b->capacity * 2;
        if (cap < b->capacity) return 0;
        unsigned char *p = (unsigned char *)realloc(b->data, cap);
        if (p == NULL) return 0;
        b->data = p; b->capacity = cap;
    }
    b->data[b->length++] = byte; return 1;
}

static inline int zov_od_add_type(zov_od_type *types, size_t *count, char kind, size_t size, int strings) {
    if (*count >= 16) return 0;
    if (size == 0 || size > 16) return 0;
    types[(*count)++] = (zov_od_type){kind, size, strings, 0}; return 1;
}

static inline int zov_od_parse_types(const char *spec, zov_od_type *types,
                                    size_t *count, char *error, size_t error_size) {
    const char *p = spec;
    while (*p != '\0') {
        char kind = *p++;
        if (!(kind == 'a' || kind == 'c' || kind == 'd' || kind == 'f' ||
              kind == 'o' || kind == 'u' || kind == 'x')) {
            snprintf(error, error_size, "od: invalid character '%c' in type string '%s'\n", kind, spec);
            return 0;
        }
        int integral = kind == 'd' || kind == 'o' || kind == 'u' || kind == 'x';
        int bfloat = 0;
        size_t size = (kind == 'a' || kind == 'c') ? 1 : (kind == 'f' ? sizeof(double) : sizeof(int));
        if (kind == 'a' || kind == 'c') {
            if (*p == 'z') ++p;
            else if (*p != '\0') {
                snprintf(error, error_size, "od: invalid character '%c' in type string '%s'\n", *p, spec);
                return 0;
            }
            if (!zov_od_add_type(types, count, kind, 1, 0)) {
                snprintf(error, error_size, "od: too many types in type string '%s'\n", spec);
                return 0;
            }
            continue;
        }
        if (integral && *p == 'C') { size = sizeof(char); ++p; }
        else if (integral && *p == 'S') { size = sizeof(short); ++p; }
        else if (integral && *p == 'I') { size = sizeof(int); ++p; }
        else if (integral && *p == 'L') { size = sizeof(long); ++p; }
        else if (kind == 'f' && *p == 'B') { size = 2; bfloat = 1; ++p; }
        else if (kind == 'f' && *p == 'H') { size = 2; ++p; }
        else if (kind == 'f' && *p == 'F') { size = sizeof(float); ++p; }
        else if (kind == 'f' && *p == 'D') { size = sizeof(double); ++p; }
        else if (kind == 'f' && *p == 'L') { size = sizeof(long double); ++p; }
        else if (*p >= '0' && *p <= '9') {
            size = 0;
            while (*p >= '0' && *p <= '9') {
                unsigned digit = (unsigned)(*p++ - '0');
                if (size > (SIZE_MAX - digit) / 10) size = SIZE_MAX;
                else size = size * 10 + digit;
            }
        } else if (*p != '\0' && *p != 'z') {
            snprintf(error, error_size, "od: invalid character '%c' in type string '%s'\n", *p, spec);
            return 0;
        }
        int supported = integral ? (size == 1 || size == 2 || size == 4 || size == 8)
                                 : (size == 2 || size == 4 || size == 8 ||
                                    (size == 16 && sizeof(long double) == 16));
        if (!supported) {
            snprintf(error, error_size,
                     "od: invalid type string '%s';\nthis system doesn't provide a %zu-byte %s type\n",
                     spec, size, integral ? "integral" : "floating point");
            return 0;
        }
        int strings = 0;
        if (*p == 'z') { strings = 1; ++p; }
        if (!zov_od_add_type(types, count, kind, size, strings)) {
            snprintf(error, error_size, "od: too many types in type string '%s'\n", spec);
            return 0;
        }
        types[*count - 1].bfloat = bfloat;
    }
    return 1;
}
static inline int zov_od_size(const char *text, uint64_t *value) {
    if (text == NULL || *text == '\0' || *text == '-') return 0;
    char *end = NULL;
    errno = 0;
    unsigned long long n = strtoull(text, &end, 0);
    if (errno == ERANGE || end == text) return 0;
    uint64_t multiplier = 1;
    if (*end != '\0') {
        if (strcmp(end, "b") == 0) multiplier = 512;
        else if (strcmp(end, "K") == 0 || strcmp(end, "KiB") == 0) multiplier = 1024;
        else if (strcmp(end, "KB") == 0) multiplier = 1000;
        else if (strcmp(end, "M") == 0 || strcmp(end, "MiB") == 0) multiplier = 1024ULL * 1024;
        else if (strcmp(end, "MB") == 0) multiplier = 1000ULL * 1000;
        else if (strcmp(end, "G") == 0 || strcmp(end, "GiB") == 0) multiplier = 1024ULL * 1024 * 1024;
        else if (strcmp(end, "GB") == 0) multiplier = 1000ULL * 1000 * 1000;
        else if (strcmp(end, "T") == 0 || strcmp(end, "TiB") == 0) multiplier = 1ULL << 40;
        else if (strcmp(end, "TB") == 0) multiplier = 1000ULL * 1000 * 1000 * 1000;
        else if (strcmp(end, "P") == 0 || strcmp(end, "PiB") == 0) multiplier = 1ULL << 50;
        else if (strcmp(end, "PB") == 0) multiplier = 1000ULL * 1000 * 1000 * 1000 * 1000;
        else if (strcmp(end, "E") == 0 || strcmp(end, "EiB") == 0) multiplier = 1ULL << 60;
        else if (strcmp(end, "EB") == 0) multiplier = 1000ULL * 1000 * 1000 * 1000 * 1000 * 1000;
        else if (strcmp(end, "Z") == 0 || strcmp(end, "ZiB") == 0) multiplier = UINT64_MAX;
        else if (strcmp(end, "Y") == 0 || strcmp(end, "YiB") == 0) multiplier = UINT64_MAX;
        else if (strcmp(end, "R") == 0 || strcmp(end, "RiB") == 0) multiplier = UINT64_MAX;
        else if (strcmp(end, "Q") == 0 || strcmp(end, "QiB") == 0) multiplier = UINT64_MAX;
        else return 0;
    }
    if ((uint64_t)n > UINT64_MAX / multiplier) return 0;
    *value = (uint64_t)n * multiplier; return 1;
}

static inline int zov_od_copy_file(FILE *destination, const char *path,
                                   uint64_t *total, int *opened) {
    FILE *source = strcmp(path, "-") == 0 ? stdin : fopen(path, "rb");
    if (source == NULL) {
        fprintf(stderr, "od: %s: %s\n", path, strerror(errno));
        return 0;
    }
    *opened = 1;
    unsigned char block[65536];
    int ok = 1;
    for (;;) {
        size_t n = fread(block, 1, sizeof(block), source);
        if (n != 0) {
            if (UINT64_MAX - *total < n || fwrite(block, 1, n, destination) != n) {
                ok = 0;
                break;
            }
            *total += n;
        }
        if (n < sizeof(block)) {
            if (ferror(source)) ok = 0;
            break;
        }
    }
    if (source != stdin && fclose(source) != 0) ok = 0;
    if (!ok) fprintf(stderr, "od: read error: %s\n", path);
    return ok;
}

static inline int zov_od_discard(FILE *input, uint64_t amount) {
    unsigned char block[65536];
    while (amount != 0) {
        size_t request = amount < sizeof(block) ? (size_t)amount : sizeof(block);
        size_t n = fread(block, 1, request, input);
        if (n == 0) return 0;
        amount -= n;
    }
    return 1;
}

static inline int zov_od_copy_stream(FILE *input, FILE *output, uint64_t amount) {
    unsigned char block[65536];
    while (amount != 0) {
        size_t request = amount < sizeof(block) ? (size_t)amount : sizeof(block);
        size_t n = fread(block, 1, request, input);
        if (n == 0 || fwrite(block, 1, n, output) != n) return 0;
        amount -= n;
    }
    return 1;
}
static inline uint64_t zov_od_value(const unsigned char *p, size_t n, int big_endian) {
    uint64_t v = 0;
    if (big_endian) {
        for (size_t i = 0; i < n; ++i) v = (v << 8) | p[i];
    } else {
        for (size_t i = n; i > 0; --i) v = (v << 8) | p[i - 1];
    }
    return v;
}

static inline const char *zov_od_ascii_name(unsigned char c) {
    static const char *names[33] = {"nul","soh","stx","etx","eot","enq","ack","bel","bs","ht","nl","vt","ff","cr","so","si","dle","dc1","dc2","dc3","dc4","nak","syn","etb","can","em","sub","esc","fs","gs","rs","us","sp"};
    if (c <= 32) return names[c];
    if (c == 127) return "del";
    static char one[2]; one[0] = (char)c; one[1] = '\0'; return one;
}

static inline void zov_od_print_char(unsigned char c) {
    if (c == '\0') fputs("\\0", stdout);
    else if (c == '\a') fputs("\\a", stdout);
    else if (c == '\b') fputs("\\b", stdout);
    else if (c == '\t') fputs("\\t", stdout);
    else if (c == '\n') fputs("\\n", stdout);
    else if (c == '\v') fputs("\\v", stdout);
    else if (c == '\f') fputs("\\f", stdout);
    else if (c == '\r') fputs("\\r", stdout);
    else if (c >= 32 && c < 127) fputc(c, stdout);
    else fprintf(stdout, "\\%03o", c);
}

static inline size_t zov_od_field_width(zov_od_type type) {
    if (type.kind == 'a') return 3;
    if (type.kind == 'c') return 3;
    if (type.kind == 'x') return type.size * 2;
    if (type.kind == 'o') return type.size * 3;
    if (type.kind == 'f') return type.size == 2 ? 15 : type.size == 4 ? 15 : type.size == 8 ? 24 :
        (LDBL_MANT_DIG > 64 ? 44 : 29);
    if (type.kind == 'd') return type.size == 1 ? 4 : type.size == 2 ? 6 : type.size == 4 ? 11 : 20;
    return type.size == 1 ? 3 : type.size == 2 ? 5 : type.size == 4 ? 10 : 20;
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

static inline void zov_od_print_type(zov_od_type type, const unsigned char *data,
                                     size_t available, int big_endian, int multi, int pad_extra) {
    unsigned char bytes[16] = {0};
    if (available > type.size) available = type.size;
    if (available != 0) memcpy(bytes, data, available);
    uint64_t value = zov_od_value(bytes, type.size, big_endian);
    size_t width = zov_od_field_width(type);
    if (pad_extra) {
        if (type.kind == 'x' || type.kind == 'o') fputc(' ', stdout);
        else width += 1;
    }
    if (multi) fputc(' ', stdout);
    if (type.kind == 'a') {
        const char *name = zov_od_ascii_name(bytes[0] & 0x7f);
        fprintf(stdout, "%*s", (int)width, name);
    } else if (type.kind == 'c') {
        char rendered[8];
        FILE *tmp = tmpfile();
        if (tmp == NULL) { fprintf(stdout, "%*s", (int)width, "?"); return; }
        if (bytes[0] >= 32 && bytes[0] < 127) fputc(bytes[0], tmp);
        else {
            unsigned char c = bytes[0];
            if (c == 0) fputs("\\0", tmp); else if (c == 7) fputs("\\a", tmp); else if (c == 8) fputs("\\b", tmp);
            else if (c == 9) fputs("\\t", tmp); else if (c == 10) fputs("\\n", tmp); else if (c == 11) fputs("\\v", tmp);
            else if (c == 12) fputs("\\f", tmp); else if (c == 13) fputs("\\r", tmp); else fprintf(tmp, "%03o", c);
        }
        rewind(tmp); size_t n = fread(rendered, 1, sizeof(rendered) - 1, tmp); rendered[n] = '\0'; fclose(tmp);
        fprintf(stdout, "%*s", (int)width, rendered);
    } else if (type.kind == 'x') fprintf(stdout, "%0*" PRIx64, (int)width, value);
    else if (type.kind == 'o') fprintf(stdout, "%0*" PRIo64, (int)width, value);
    else if (type.kind == 'u') fprintf(stdout, "%*" PRIu64, (int)width, value);
    else if (type.kind == 'd') {
        int64_t signed_value;
        if (type.size >= 8) signed_value = (int64_t)value;
        else { unsigned bits = (unsigned)(type.size * 8); uint64_t sign = (uint64_t)1 << (bits - 1); signed_value = (int64_t)((value ^ sign) - sign); }
        fprintf(stdout, "%*" PRId64, (int)width, signed_value);
    } else if (type.kind == 'f') {
        if (type.size == 2) {
            uint16_t bits=(uint16_t)value;
            float f;
            if(type.bfloat) { uint32_t raw=(uint32_t)bits<<16; memcpy(&f,&raw,sizeof(f)); }
            else f=zov_od_half_float(bits);
            char text[64]; zov_od_float_text(f,text,sizeof(text));
            fprintf(stdout, "%*s", (int)width, text);
        } else if (type.size == 4) {
            float f; uint32_t v = (uint32_t)value; memcpy(&f, &v, sizeof(f));
            char text[64]; zov_od_float_text(f,text,sizeof(text));
            fprintf(stdout, "%*s", (int)width, text);
        } else if (type.size == 8) {
            double f; memcpy(&f, &value, sizeof(f));
            char text[96]; zov_od_double_text(f,text,sizeof(text));
            fprintf(stdout, "%*s", (int)width, text);
        } else if (type.size == sizeof(long double)) {
            unsigned char ordered[sizeof(long double)] = {0};
            size_t copied = available < sizeof(long double) ? available : sizeof(long double);
            for (size_t i = 0; i < copied; ++i)
                ordered[big_endian ? sizeof(long double) - copied + i : i] =
                    bytes[big_endian ? copied - i - 1 : i];
            long double f = 0;
            memcpy(&f, ordered, sizeof(f));
            char text[128]; zov_od_long_double_text(f,text,sizeof(text));
            fprintf(stdout, "%*s", (int)width, text);
        } else fprintf(stdout, "%*s", (int)width, "0");
    }
}

static inline void zov_od_print_address(uint64_t address, char radix, int width) {
    if (radix == 'n') return;
    if (radix == 'x') fprintf(stdout, "%0*" PRIx64, width, address);
    else if (radix == 'd') fprintf(stdout, "%0*" PRIu64, width, address);
    else fprintf(stdout, "%0*" PRIo64, width, address);
}

static inline void zov_od_help(void) {
    puts("Usage: od [OPTION]... [FILE]...\n"
         "  or:  od [-abcdfilosx]... [FILE] [[+]OFFSET[.][b]]\n"
         "  or:  od --traditional [OPTION]... [FILE] [[+]OFFSET[.][b] [+][LABEL][.][b]]\n\n"
         "Write an unambiguous representation, octal bytes by default,\n"
         "of FILE to standard output.  With more than one FILE argument,\n"
         "concatenate them in the listed order to form the input.\n\n"
         "With no FILE, or when FILE is -, read standard input.\n\n"
         "If first and second call formats both apply, the second format is assumed\n"
         "if the last operand begins with + or (if there are 2 operands) a digit.\n"
         "An OFFSET operand means -j OFFSET.  LABEL is the pseudo-address\n"
         "at first byte printed, incremented when dump is progressing.\n"
         "For OFFSET and LABEL, a 0x or 0X prefix indicates hexadecimal;\n"
         "suffixes may be . for octal and b for multiply by 512.\n\n"
         "Mandatory arguments to long options are mandatory for short options too.\n"
         "  -A, --address-radix=RADIX\n"
         "         output format for file offsets;\n"
         "         RADIX is one of [doxn], for Decimal, Octal, Hex or None\n"
         "      --endian={big|little}\n"
         "         swap input bytes according the specified order\n"
         "  -j, --skip-bytes=BYTES\n"
         "         skip BYTES input bytes first\n"
         "  -N, --read-bytes=BYTES\n"
         "         limit dump to BYTES input bytes\n"
         "  -S BYTES, --strings[=BYTES]\n"
         "         show only NUL terminated strings\n"
         "         of at least BYTES (default 3) printable characters\n"
         "  -t, --format=TYPE\n"
         "         select output format or formats\n"
         "  -v, --output-duplicates\n"
         "         do not use * to mark line suppression\n"
         "  -w[BYTES], --width[=BYTES]\n"
         "         output BYTES bytes per output line;\n"
         "         32 is implied when BYTES is not specified\n"
         "      --traditional\n"
         "         accept arguments in third form above\n"
         "      --help\n"
         "         display this help and exit\n"
         "      --version\n"
         "         output version information and exit\n\n\n"
         "Traditional format specifications may be intermixed; they accumulate:\n"
         "  -a   same as -t a,  select named characters, ignoring high-order bit\n"
         "  -b   same as -t o1, select octal bytes\n"
         "  -c   same as -t c,  select printable characters or backslash escapes\n"
         "  -d   same as -t u2, select unsigned decimal 2-byte units\n"
         "  -f   same as -t fF, select floats\n"
         "  -i   same as -t dI, select decimal ints\n"
         "  -l   same as -t dL, select decimal longs\n"
         "  -o   same as -t o2, select octal 2-byte units\n"
         "  -s   same as -t d2, select decimal 2-byte units\n"
         "  -x   same as -t x2, select hexadecimal 2-byte units\n\n\n"
         "TYPE is made up of one or more of these specifications:\n"
         "  a          named character, ignoring high-order bit\n"
         "  c          printable character or backslash escape\n"
         "  d[SIZE]    signed decimal, SIZE bytes per integer\n"
         "  f[SIZE]    floating point, SIZE bytes per float\n"
         "  o[SIZE]    octal, SIZE bytes per integer\n"
         "  u[SIZE]    unsigned decimal, SIZE bytes per integer\n"
         "  x[SIZE]    hexadecimal, SIZE bytes per integer\n\n"
         "SIZE is a number.  For TYPE in [doux], SIZE may also be C for\n"
         "sizeof(char), S for sizeof(short), I for sizeof(int) or L for\n"
         "sizeof(long).  If TYPE is f, SIZE may also be B for Brain 16 bit,\n"
         "H for Half precision float, F for sizeof(float), D for sizeof(double),\n"
         "or L for sizeof(long double).\n\n"
         "Adding a z suffix to any type displays printable characters at the end of\n"
         "each output line.\n\n\n"
         "BYTES is hex with 0x or 0X prefix, and may have a multiplier suffix:\n"
         "  b    512\n"
         "  KB   1000\n"
         "  K    1024\n"
         "  MB   1000*1000\n"
         "  M    1024*1024\n"
         "and so on for G, T, P, E, Z, Y, R, Q.\n"
         "Binary prefixes can be used, too: KiB=K, MiB=M, and so on.\n\n"
         "Report bugs to: bug-coreutils@gnu.org\n"
         "GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n"
         "General help using GNU software: <https://www.gnu.org/gethelp/>\n"
         "Report any translation bugs to <https://translationproject.org/team/>\n"
         "Full documentation <https://www.gnu.org/software/coreutils/od>\n"
         "or available locally via: info '(coreutils) od invocation'");
}

static inline int zov_od_run(int argc, int8_t **argv) {
    char address_radix = 'o';
    uint64_t skip = 0, read_limit = UINT64_MAX;
    size_t width = 16, string_min = 3;
    int width_set = 0, show_duplicates = 0, big_endian = 0, options_done = 0, strings_mode = 0;
    zov_od_type types[16]; size_t type_count = 0;
    char **files = (char **)malloc((size_t)(argc + 1) * sizeof(*files));
    if (files == NULL) return 1;
    int file_count = 0, status = 0;
    for (int i = 0; i < argc; ++i) {
        char *arg = (char *)argv[i];
        if (!options_done && strcmp(arg, "--") == 0) { options_done = 1; continue; }
        if (!options_done && strncmp(arg, "--", 2) == 0) {
            char *eq = strchr(arg, '='); size_t keylen = eq ? (size_t)(eq - arg) : strlen(arg); const char *value = eq ? eq + 1 : NULL;
#define OD_LONG(name) (keylen <= strlen(name) && keylen > 2 && strncmp(arg, name, keylen) == 0)
            if (OD_LONG("--help") || OD_LONG("--version") || OD_LONG("--output-duplicates") || OD_LONG("--traditional")) {
                const char *name = OD_LONG("--help") ? "--help" : OD_LONG("--version") ? "--version" :
                                   OD_LONG("--output-duplicates") ? "--output-duplicates" : "--traditional";
                if (value != NULL) {
                    fprintf(stderr, "od: option '%s' doesn't allow an argument\nTry 'od --help' for more information.\n", name);
                    free(files);
                    return 1;
                }
                if (strcmp(name, "--help") == 0) { zov_od_help(); free(files); return 0; }
                if (strcmp(name, "--version") == 0) { puts("od (zovutils) 0.5.6"); free(files); return 0; }
                if (strcmp(name, "--output-duplicates") == 0) { show_duplicates = 1; continue; }
                continue;
            }
            if (OD_LONG("--strings")) {
                strings_mode=1;
                if(value!=NULL) { uint64_t n; if(!zov_od_size(value,&n) || n==0 || n>SIZE_MAX) { fprintf(stderr,"od: invalid string length: %s\n",value); status=1; break; } string_min=(size_t)n; }
                continue;
            }
            if (OD_LONG("--traditional")) continue;
            if (OD_LONG("--width") && value == NULL) { width=32; width_set=1; continue; }
            if (OD_LONG("--endian")) {
                if (value == NULL && i + 1 < argc) value = (char *)argv[++i];
                if (value == NULL) { fprintf(stderr, "od: option '--endian' requires an argument\nTry 'od --help' for more information.\n"); status=1; break; }
                if (strcmp(value,"big") && strcmp(value,"little")) { fprintf(stderr,"od: invalid argument '%s' for '--endian'\nValid arguments are:\n  - 'little'\n  - 'big'\nTry 'od --help' for more information.\n", value); status=1; break; }
                big_endian = strcmp(value,"big") == 0; continue;
            }
            if (OD_LONG("--address-radix") || OD_LONG("--skip-bytes") || OD_LONG("--read-bytes") || OD_LONG("--format") || OD_LONG("--width")) {
                const char *name = OD_LONG("--address-radix") ? "--address-radix" : OD_LONG("--skip-bytes") ? "--skip-bytes" : OD_LONG("--read-bytes") ? "--read-bytes" : OD_LONG("--format") ? "--format" : "--width";
                if (value == NULL && i + 1 < argc) value = argv[++i];
                if (value == NULL) { fprintf(stderr,"od: option '%s' requires an argument\n",name); status=1; break; }
                if (strcmp(name,"--address-radix")==0) { if (value[0]=='\0' || !strchr("doxn",value[0]) || value[1]!='\0') { fprintf(stderr,"od: invalid output address radix '%c'; it must be one character from [doxn]\n",value[0]); status=1; break; } address_radix=value[0]; }
                else if (strcmp(name,"--skip-bytes")==0) { if (!zov_od_size(value,&skip)) { fprintf(stderr,"od: invalid suffix in -j argument '%s'\n",value); status=1; break; } }
                else if (strcmp(name,"--read-bytes")==0) { if (!zov_od_size(value,&read_limit)) { fprintf(stderr,"od: invalid suffix in -N argument '%s'\n",value); status=1; break; } }
                else if (strcmp(name,"--format")==0) { char error[256]; if (!zov_od_parse_types(value,types,&type_count,error,sizeof(error))) { fputs(error,stderr); status=1; break; } }
                else { uint64_t n; if (!zov_od_size(value,&n) || n==0 || n>1048576) { fprintf(stderr,"od: invalid --width argument '%s'\n",value); status=1; break; } width=(size_t)n; width_set=1; }
                continue;
            }
            fprintf(stderr,"od: unrecognized option '%s'\nTry 'od --help' for more information.\n",arg); status=1; break;
#undef OD_LONG
        }
        if (!options_done && arg[0]=='-' && arg[1]!='\0') {
            for (size_t j=1; arg[j]; ++j) {
                char ch=arg[j];
                if (ch=='v') show_duplicates=1;
                else if (ch=='a') { if(!zov_od_add_type(types,&type_count,'a',1,0)) status=1; }
                else if (ch=='b') { if(!zov_od_add_type(types,&type_count,'o',1,0)) status=1; }
                else if (ch=='c') { if(!zov_od_add_type(types,&type_count,'c',1,0)) status=1; }
                else if (ch=='d') { if(!zov_od_add_type(types,&type_count,'u',2,0)) status=1; }
                else if (ch=='f') { if(!zov_od_add_type(types,&type_count,'f',sizeof(float),0)) status=1; }
                else if (ch=='i') { if(!zov_od_add_type(types,&type_count,'d',sizeof(int),0)) status=1; }
                else if (ch=='l') { if(!zov_od_add_type(types,&type_count,'d',sizeof(long),0)) status=1; }
                else if (ch=='o') { if(!zov_od_add_type(types,&type_count,'o',2,0)) status=1; }
                else if (ch=='s') { if(!zov_od_add_type(types,&type_count,'d',2,0)) status=1; }
                else if (ch=='x') { if(!zov_od_add_type(types,&type_count,'x',2,0)) status=1; }
                else if (ch=='S') { const char *value=arg[j+1]?arg+j+1:(i+1<argc?(char *)argv[++i]:NULL); uint64_t n; if(value==NULL) { fprintf(stderr,"od: option requires an argument -- 'S'\n"); status=1; } else if(!zov_od_size(value,&n) || n==0 || n>SIZE_MAX) { fprintf(stderr,"od: invalid string length: %s\n",value); status=1; } else { strings_mode=1; string_min=(size_t)n; } j=strlen(arg)-1; }
                else if (ch=='A' || ch=='j' || ch=='N' || ch=='t' || ch=='w') {
                    const char *value = NULL;
                    if (arg[j+1]) { value=arg+j+1; j=strlen(arg)-1; }
                    else if (ch=='w') { width=32; width_set=1; break; }
                    else if (i+1<argc) value=(char *)argv[++i];
                    else { fprintf(stderr,"od: option requires an argument -- '%c'\n",ch); status=1; break; }
                    if (ch=='A') { if(value[0]=='\0' || !strchr("doxn",value[0]) || value[1]!='\0') { fprintf(stderr,"od: invalid output address radix '%c'; it must be one character from [doxn]\n",value[0]); status=1; } else address_radix=value[0]; }
                    else if (ch=='j') { if(!zov_od_size(value,&skip)) { fprintf(stderr,"od: invalid suffix in -j argument '%s'\n",value); status=1; } }
                    else if (ch=='N') { if(!zov_od_size(value,&read_limit)) { fprintf(stderr,"od: invalid suffix in -N argument '%s'\n",value); status=1; } }
                    else if (ch=='t') { char error[256]; if(!zov_od_parse_types(value,types,&type_count,error,sizeof(error))) { fputs(error,stderr); status=1; } }
                    else { uint64_t n; if(!zov_od_size(value,&n) || n==0 || n>1048576) { fprintf(stderr,"od: invalid -w argument '%s'\n",value); status=1; } else { width=(size_t)n; width_set=1; } }
                    break;
                } else { fprintf(stderr,"od: invalid option -- '%c'\nTry 'od --help' for more information.\n",ch); status=1; break; }
                if(status) break;
            }
            if (status) break;
            continue;
        }
        files[file_count++] = arg;
    }
    if (status) { free(files); return 1; }
    if(file_count>0) {
        const char *last=files[file_count-1];
        const char *digits=last;
        int offset_operand=0;
        if(digits[0]=='+') { ++digits; offset_operand=1; }
        else if(file_count>1 && isdigit((unsigned char)digits[0])) offset_operand=1;
        if(offset_operand) {
            char number[128]; size_t n=strlen(digits); int base=0; uint64_t multiplier=1;
            if(n>0 && digits[n-1]=='b') { multiplier=512; --n; }
            else if(n>0 && digits[n-1]=='.') { base=8; --n; }
            if(n==0 || n>=sizeof(number)) { fprintf(stderr,"od: invalid offset '%s'\n",last); free(files); return 1; }
            memcpy(number,digits,n); number[n]='\0'; char *end=NULL; errno=0;
            unsigned long long value=strtoull(number,&end,base);
            if(errno==ERANGE || end==number || *end!='\0' || value>UINT64_MAX/multiplier) { fprintf(stderr,"od: invalid offset '%s'\n",last); free(files); return 1; }
            skip=(uint64_t)value*multiplier; --file_count;
        }
    }
    if (type_count == 0) zov_od_add_type(types,&type_count,'o',2,0);
    if (!width_set) { width = 16; }
    if (width == 0) width=16;
    FILE *input = tmpfile();
    if (input == NULL) { fprintf(stderr, "od: cannot create temporary input: %s\n", strerror(errno)); free(files); return 1; }
    uint64_t total = 0;
    int opened = 0;
    if (file_count == 0) {
        if (!zov_od_copy_file(input, "-", &total, &opened)) status = 1;
    } else {
        for (int i = 0; i < file_count; ++i)
            if (!zov_od_copy_file(input, files[i], &total, &opened)) status = 1;
    }
    free(files);
    if (fflush(input) != 0 || fseek(input, 0, SEEK_SET) != 0) {
        fprintf(stderr, "od: temporary input error\n");
        fclose(input);
        return 1;
    }
    uint64_t start = skip < total ? skip : total;
    uint64_t available = total - start;
    if (available > read_limit) available = read_limit;
    if (!zov_od_discard(input, start)) {
        fprintf(stderr, "od: read error\n");
        fclose(input);
        return 1;
    }
    int addr_width = address_radix == 'x' ? 6 : 7;
    if (strings_mode) {
        FILE *string = tmpfile();
        if (string == NULL) { fprintf(stderr, "od: cannot create temporary string buffer: %s\n", strerror(errno)); fclose(input); return 1; }
        uint64_t processed = 0, string_length = 0, string_start = start;
        int printable = 1;
        while (processed < available) {
            int c = fgetc(input);
            if (c == EOF) { status = 1; break; }
            ++processed;
            if (c == 0) {
                if (string_length >= string_min && printable) {
                    zov_od_print_address(string_start, address_radix, addr_width);
                    fputc(' ', stdout);
                    rewind(string);
                    if (!zov_od_copy_stream(string, stdout, string_length)) status = 1;
                    fputc('\n', stdout);
                }
                rewind(string);
                string_length = 0;
                printable = 1;
                string_start = start + processed;
            } else {
                if (!isprint((unsigned char)c)) printable = 0;
                if (fputc(c, string) == EOF) { status = 1; break; }
                ++string_length;
            }
        }
        fclose(string);
    } else {
        unsigned char *row = (unsigned char *)malloc(width);
        unsigned char *previous = (unsigned char *)malloc(width);
        if (row == NULL || previous == NULL) {
            fprintf(stderr, "od: memory exhausted\n");
            free(row); free(previous); fclose(input); return 1;
        }
        uint64_t offset = 0;
        int have_previous = 0, star = 0;
        while (offset < available) {
            size_t chunk = (available - offset) < width ? (size_t)(available - offset) : width;
            if (fread(row, 1, chunk, input) != chunk) { fprintf(stderr, "od: read error\n"); status = 1; break; }
            int duplicate = chunk == width && have_previous && memcmp(previous, row, width) == 0;
            if (duplicate && !show_duplicates) {
                if (!star) { puts("*"); star = 1; }
                offset += chunk;
                continue;
            }
            star = 0;
            for (size_t t = 0; t < type_count; ++t) {
                if (t == 0) {
                    zov_od_print_address(start + offset, address_radix, addr_width);
                    fputc(' ', stdout);
                } else {
                    for (int k = 0; k < addr_width + 1; ++k) fputc(' ', stdout);
                }
                zov_od_type type = types[t];
                for (size_t pos = 0; pos < chunk; pos += type.size) {
                    size_t n = chunk - pos;
                    if (n > type.size) n = type.size;
                    zov_od_print_type(type, row + pos, n, big_endian,
                                      pos > 0, t == 0 && type_count > 1);
                }
                if (type.strings) {
                    size_t total_cells = (width + type.size - 1) / type.size;
                    size_t shown_cells = (chunk + type.size - 1) / type.size;
                    for (size_t cell = shown_cells; cell < total_cells; ++cell)
                        for (size_t k = 0; k < zov_od_field_width(type) + 1; ++k) fputc(' ', stdout);
                    fputs("  >", stdout);
                    for (size_t k = 0; k < chunk; ++k) fputc(isprint(row[k]) ? row[k] : '.', stdout);
                    fputc('<', stdout);
                }
                fputc('\n', stdout);
            }
            if (chunk == width) { memcpy(previous, row, width); have_previous = 1; }
            offset += chunk;
        }
        free(row);
        free(previous);
    }
    if (opened > 0 && !strings_mode) {
        zov_od_print_address(start + available, address_radix, addr_width);
        if (address_radix != 'n') fputc('\n', stdout);
    }
    if (fclose(input) != 0) status = 1;
    if (fflush(stdout) != 0) status = 1;
    return status;
}

#endif
