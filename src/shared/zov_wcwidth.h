#ifndef ZOVUTILS_ZOV_WCWIDTH_H
#define ZOVUTILS_ZOV_WCWIDTH_H

#include <stdint.h>
#include <wchar.h>
#include <wctype.h>

int wcwidth(wchar_t character);

static inline int zov_wcwidth(uint32_t character) {
    return wcwidth((wchar_t) character);
}

static inline int zov_iswspace(uint32_t character) {
    return iswspace((wint_t) character);
}

#endif
