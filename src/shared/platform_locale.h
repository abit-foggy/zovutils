#ifndef ZOVUTILS_PLATFORM_LOCALE_H
#define ZOVUTILS_PLATFORM_LOCALE_H

#include <langinfo.h>
#include <locale.h>
#include <stddef.h>
#include <strings.h>
#include <wchar.h>
#include <wctype.h>

static inline void zov_init_ctype_locale(void) {
    setlocale(LC_CTYPE, "");
}

static inline int zov_locale_uses_unicode_quotes(void) {
    zov_init_ctype_locale();
    const char *codeset = nl_langinfo(CODESET);
    return codeset != NULL
        && (strcasecmp(codeset, "UTF-8") == 0 || strcasecmp(codeset, "UTF8") == 0);
}

static inline size_t zov_printable_utf8_length(const char *text, size_t length) {
    if (!zov_locale_uses_unicode_quotes() || length == 0) {
        return 0;
    }

    const unsigned char *bytes = (const unsigned char *) text;
    unsigned char first = bytes[0];
    size_t count;
    if (first >= 0xc2 && first <= 0xdf) {
        count = 2;
    } else if (first >= 0xe0 && first <= 0xef) {
        count = 3;
    } else if (first >= 0xf0 && first <= 0xf4) {
        count = 4;
    } else {
        return 0;
    }
    if (length < count) {
        return 0;
    }
    for (size_t index = 1; index < count; index++) {
        if (bytes[index] < 0x80 || bytes[index] > 0xbf) {
            return 0;
        }
    }
    if ((first == 0xe0 && bytes[1] < 0xa0)
        || (first == 0xed && bytes[1] >= 0xa0)
        || (first == 0xf0 && bytes[1] < 0x90)
        || (first == 0xf4 && bytes[1] > 0x8f)) {
        return 0;
    }

    mbstate_t state = {0};
    wchar_t character;
    size_t decoded = mbrtowc(&character, text, count, &state);
    if (decoded != count || !iswprint((wint_t) character)) {
        return 0;
    }
    return count;
}

static inline void zov_fold_next_char(const unsigned char *text, size_t length,
                                      size_t *bytes_out, size_t *width_out) {
    if (length == 0) { *bytes_out = 0; *width_out = 0; return; }
    unsigned char c = text[0];
    if (c >= 0x80 && MB_CUR_MAX > 1) {
        wchar_t wc;
        mbstate_t state;
        memset(&state, 0, sizeof(state));
        size_t rc = mbrtowc(&wc, (const char *)text, length, &state);
        if (rc != (size_t)-1 && rc != (size_t)-2 && rc != 0) {
            *bytes_out = rc;
            int w = wcwidth(wc);
            *width_out = w < 0 ? 1 : (size_t)w;
            return;
        }
    }
    *bytes_out = 1;
    if (c == 0 || c == '\b') *width_out = 0;
    else if (c == '\t') *width_out = 1;
    else if (MB_CUR_MAX > 1) {
        int w = wcwidth((wchar_t)c);
        *width_out = w < 0 ? 1 : (size_t)w;
    } else {
        *width_out = 1;
    }
}

#endif
