#ifndef ZOVUTILS_PLATFORM_LOCALE_H
#define ZOVUTILS_PLATFORM_LOCALE_H

#include <langinfo.h>
#include <locale.h>
#include <stddef.h>
#include <strings.h>
#include <wchar.h>
#include <wctype.h>

static inline int zov_locale_uses_unicode_quotes(void) {
    setlocale(LC_CTYPE, "");
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

#endif
