#ifndef ZOVUTILS_PLATFORM_HOSTID_H
#define ZOVUTILS_PLATFORM_HOSTID_H

#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
static inline int64_t zov_gethostid(void) {
    DWORD serial = 0;
    if (GetVolumeInformationA("C:\\", NULL, 0, &serial, NULL, NULL, NULL, 0)) {
        return (int64_t)serial;
    }
    return 0;
}
#else
#include <unistd.h>
static inline int64_t zov_gethostid(void) {
    return (int64_t)gethostid();
}
#endif

#endif
