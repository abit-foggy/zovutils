#ifndef ZOVUTILS_PLATFORM_LOCALE_H
#define ZOVUTILS_PLATFORM_LOCALE_H

#include <langinfo.h>
#include <locale.h>
#include <strings.h>

static inline int zov_locale_uses_unicode_quotes(void) {
    setlocale(LC_CTYPE, "");
    const char *codeset = nl_langinfo(CODESET);
    return codeset != NULL
        && (strcasecmp(codeset, "UTF-8") == 0 || strcasecmp(codeset, "UTF8") == 0);
}

#endif
