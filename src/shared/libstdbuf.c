#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

static size_t zov_stdbuf_capacity(const char *mode) {
    if (!mode || !*mode || !strcmp(mode, "-") || !strcmp(mode, "0") || !strcmp(mode, "L")) return 0;
    errno = 0;
    char *end = NULL;
    unsigned long long value = strtoull(mode, &end, 10);
    if (errno || end == mode) return 0;
    unsigned long long unit = 1;
    if (!*end) unit = 1;
    else if (!strcmp(end, "k") || !strcmp(end, "K") || !strcmp(end, "KiB")) unit = 1024ULL;
    else if (!strcmp(end, "KB")) unit = 1000ULL;
    else if (!strcmp(end, "M") || !strcmp(end, "MiB")) unit = 1024ULL * 1024ULL;
    else if (!strcmp(end, "MB")) unit = 1000ULL * 1000ULL;
    else if (!strcmp(end, "G") || !strcmp(end, "GiB")) unit = 1024ULL * 1024ULL * 1024ULL;
    else if (!strcmp(end, "GB")) unit = 1000ULL * 1000ULL * 1000ULL;
    else if (!strcmp(end, "T") || !strcmp(end, "TiB")) unit = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
    else if (!strcmp(end, "TB")) unit = 1000ULL * 1000ULL * 1000ULL * 1000ULL;
    else return 0;
    if (value > (unsigned long long)SIZE_MAX / unit) return 0;
    size_t size = (size_t)(value * unit);
    return size > 16U * 1024U * 1024U ? 16U * 1024U * 1024U : size;
}

static void zov_stdbuf_apply(const char *name, FILE *stream) {
    const char *mode = getenv(name);
    if (!mode || !*mode || !strcmp(mode, "-")) return;
    if (!strcmp(mode, "0")) { setvbuf(stream, NULL, _IONBF, 0); return; }
    if (!strcmp(mode, "L")) { setvbuf(stream, NULL, _IOLBF, 0); return; }
    size_t size = zov_stdbuf_capacity(mode);
    if (size) setvbuf(stream, NULL, _IOFBF, size);
}

#if defined(__GNUC__)
__attribute__((constructor))
#endif
static void zov_stdbuf_init(void) {
    zov_stdbuf_apply("_STDBUF_I", stdin);
    zov_stdbuf_apply("_STDBUF_O", stdout);
    zov_stdbuf_apply("_STDBUF_E", stderr);
}
