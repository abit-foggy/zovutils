#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void zov_apply(const char *name, FILE *stream) {
    const char *mode = getenv(name);
    if (!mode || !*mode || !strcmp(mode, "-")) return;
    if (!strcmp(mode, "0")) { setvbuf(stream, NULL, _IONBF, 0); return; }
    if (!strcmp(mode, "L")) { setvbuf(stream, NULL, _IOLBF, 0); return; }
    char *end = NULL;
    unsigned long long size = strtoull(mode, &end, 10);
    if (!end || end == mode) return;
    if (!strcmp(end, "K")) size *= 1024ULL;
    else if (!strcmp(end, "M")) size *= 1024ULL * 1024ULL;
    else if (*end) return;
    if (size > 0 && size <= 16ULL * 1024ULL * 1024ULL)
        setvbuf(stream, NULL, _IOFBF, (size_t)size);
}

#if defined(__GNUC__)
__attribute__((constructor))
#endif
static void zov_stdbuf_init(void) {
    zov_apply("_STDBUF_I", stdin);
    zov_apply("_STDBUF_O", stdout);
    zov_apply("_STDBUF_E", stderr);
}
