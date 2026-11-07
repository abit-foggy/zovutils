#ifndef ZOVUTILS_FACTOR_BACKEND_H
#define ZOVUTILS_FACTOR_BACKEND_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline uint64_t zov_factor_addmod(uint64_t a, uint64_t b, uint64_t m) { return a >= m - b ? a - (m - b) : a + b; }
static inline uint64_t zov_factor_mulmod(uint64_t a, uint64_t b, uint64_t m) { uint64_t r = 0; while (b) { if (b & 1) r = zov_factor_addmod(r, a, m); b >>= 1; if (b) a = zov_factor_addmod(a, a, m); } return r; }
static inline uint64_t zov_factor_powmod(uint64_t a, uint64_t e, uint64_t m) { uint64_t r = 1; while (e) { if (e & 1) r = zov_factor_mulmod(r, a, m); e >>= 1; if (e) a = zov_factor_mulmod(a, a, m); } return r; }
static inline int zov_factor_prime(uint64_t n) {
    if (n < 2) return 0;
    static const uint32_t small[] = {2,3,5,7,11,13,17,19,23,29,31,37};
    for (size_t i = 0; i < sizeof(small)/sizeof(small[0]); ++i) { if (n == small[i]) return 1; if (n % small[i] == 0) return 0; }
    uint64_t d = n - 1; unsigned s = 0; while (!(d & 1)) { d >>= 1; ++s; }
    static const uint64_t bases[] = {2,325,9375,28178,450775,9780504,1795265022};
    for (size_t i = 0; i < sizeof(bases)/sizeof(bases[0]); ++i) {
        uint64_t a = bases[i] % n; if (!a) continue;
        uint64_t x = zov_factor_powmod(a, d, n); if (x == 1 || x == n - 1) continue;
        unsigned j; for (j = 1; j < s; ++j) { x = zov_factor_mulmod(x, x, n); if (x == n - 1) break; }
        if (j == s) return 0;
    }
    return 1;
}
static inline int zov_factor_collect(uint64_t n, uint64_t *f, size_t *count) {
    if (n == 1) return 1;
    if (zov_factor_prime(n)) { if (*count >= 64) return 0; f[(*count)++] = n; return 1; }
    if (!(n & 1)) { if (*count >= 64) return 0; f[(*count)++] = 2; return zov_factor_collect(n / 2, f, count); }
    for (uint64_t d = 3; d <= n / d; d += 2) if (n % d == 0) { if (*count >= 64) return 0; f[(*count)++] = d; return zov_factor_collect(n / d, f, count); }
    if (*count >= 64) return 0; f[(*count)++] = n; return 1;
}
static inline int zov_factor_one(const char *token, int exponents) {
    if (!token || !*token) return 1;
    uint64_t n = 0; const unsigned char *p = (const unsigned char *)token;
    if (*p == '+') ++p;
    if (!*p) goto invalid;
    for (; *p; ++p) { if (*p < '0' || *p > '9' || n > (UINT64_MAX - (*p - '0')) / 10) goto invalid; n = n * 10 + (*p - '0'); }
    printf("%llu:", (unsigned long long)n);
    if (n > 1) {
        uint64_t factors[64]; size_t count = 0;
        if (!zov_factor_collect(n, factors, &count)) return 1;
        if (exponents) for (size_t i = 0; i < count;) { size_t j = i + 1; while (j < count && factors[j] == factors[i]) ++j; printf(" %llu", (unsigned long long)factors[i]); if (j - i > 1) printf("^%zu", j - i); i = j; }
        else for (size_t i = 0; i < count; ++i) printf(" %llu", (unsigned long long)factors[i]);
    }
    putchar('\n'); return ferror(stdout) ? 1 : 0;
invalid:
    fprintf(stderr, "factor: '%s' is not a valid positive integer\n", token); return 1;
}
static inline int zov_factor_run(int argc, int8_t **argv) {
    int exponents = 0, options = 1, errors = 0, operands = 0;
    for (int i = 0; i < argc; ++i) {
        const char *a = (const char *)argv[i];
        if (options && !strcmp(a, "--")) { options = 0; continue; }
        if (options && (!strcmp(a, "--help") || !strcmp(a, "--version"))) { if (!strcmp(a, "--help")) puts("Usage: factor [OPTION] [NUMBER]...\nPrint the prime factors of each specified integer NUMBER.  If none\nare specified on the command line, read them from standard input.\n\n  -h, --exponents  print repeated factors in form p^e unless e is 1\n      --help       display this help and exit\n      --version    output version information and exit\n\nReport bugs to: bug-coreutils@gnu.org\nGNU coreutils home page: <https://www.gnu.org/software/coreutils/>"); else puts("factor (zovutils) 0.7.0"); return 0; }
        if (options && (!strcmp(a, "-h") || !strcmp(a, "--exponents"))) { exponents = 1; continue; }
        if (options && a[0] == '-' && a[1] == '-') { fprintf(stderr, "factor: unrecognized option '%s'\nTry 'factor --help' for more information.\n", a); return 1; }
        if (options && a[0] == '-' && a[1]) { fprintf(stderr, "factor: invalid option -- '%c'\nTry 'factor --help' for more information.\n", a[1]); return 1; }
        ++operands; if (zov_factor_one(a, exponents)) errors = 1;
    }
    if (!operands) { char token[512]; while (scanf("%511s", token) == 1) if (zov_factor_one(token, exponents)) errors = 1; }
    return errors || ferror(stdout) ? 1 : 0;
}
#endif
