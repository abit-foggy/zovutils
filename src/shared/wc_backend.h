#ifndef ZOVUTILS_WC_BACKEND_H
#define ZOVUTILS_WC_BACKEND_H

#include <stddef.h>
#include <string.h>

static inline size_t zov_wc_count_newlines(const unsigned char *buffer, size_t length) {
    const unsigned char *cursor = buffer;
    size_t remaining = length;
    size_t count = 0;
    while (remaining != 0) {
        const unsigned char *found = (const unsigned char *)memchr(cursor, '\n', remaining);
        if (found == NULL) break;
        ++count;
        remaining -= (size_t)(found - cursor) + 1;
        cursor = found + 1;
    }
    return count;
}

static inline int zov_wc_newline_backend(void) {
#if defined(__aarch64__)
    return 3;
#endif
#if defined(__x86_64__)
    unsigned int eax, ebx, ecx, edx;
    unsigned int leaf1_ecx;
    unsigned long long xcr0;
    __asm__ __volatile__("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1u), "c"(0u));
    leaf1_ecx = ecx;
    if ((leaf1_ecx & (1u << 27)) == 0 || (leaf1_ecx & (1u << 28)) == 0) return 0;
    __asm__ __volatile__(".byte 0x0f, 0x01, 0xd0" : "=a"(eax), "=d"(edx) : "c"(0u));
    xcr0 = ((unsigned long long)edx << 32) | eax;
    __asm__ __volatile__("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(7u), "c"(0u));
    if ((xcr0 & 0xe6u) == 0xe6u && (ebx & (1u << 16)) != 0 && (ebx & (1u << 30)) != 0) return 2;
    if ((xcr0 & 0x6u) == 0x6u && (ebx & (1u << 5)) != 0) return 1;
#endif
    return 0;
}

#endif
