#include <stdint.h>
#include <wchar.h>

int wcwidth(wchar_t character);

static inline int zov_wcwidth(uint32_t character) {
    return wcwidth((wchar_t) character);
}
