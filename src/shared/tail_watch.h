#ifndef ZOVUTILS_TAIL_WATCH_H
#define ZOVUTILS_TAIL_WATCH_H

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__linux__)
#include <sys/inotify.h>

static inline int zov_tail_watch_open(void) {
    int descriptor = inotify_init();
    if (descriptor < 0)
        return -1;
    if (fcntl(descriptor, F_SETFL, O_NONBLOCK) < 0
        || fcntl(descriptor, F_SETFD, FD_CLOEXEC) < 0) {
        close(descriptor);
        return -1;
    }
    return descriptor;
}

static inline int zov_tail_watch_add(int descriptor, const char *path, int follow_name) {
    struct stat link_info;
    struct stat info;
    int count = 0;
    if (lstat(path, &link_info) == 0 && !S_ISLNK(link_info.st_mode)
        && stat(path, &info) == 0 && S_ISREG(info.st_mode)) {
        uint32_t mask = IN_MODIFY | IN_ATTRIB | IN_CLOSE_WRITE | IN_DELETE_SELF | IN_MOVE_SELF;
        if (inotify_add_watch(descriptor, path, mask) >= 0)
            count++;
    }
    if (follow_name && lstat(path, &link_info) == 0 && !S_ISLNK(link_info.st_mode)) {
        size_t length = strlen(path);
        char *parent = (char *) malloc(length + 2);
        if (parent != NULL) {
            memcpy(parent, path, length + 1);
            char *slash = strrchr(parent, '/');
            if (slash == NULL) {
                parent[0] = '.';
                parent[1] = '\0';
            } else if (slash == parent) {
                parent[1] = '\0';
            } else {
                *slash = '\0';
            }
            uint32_t mask = IN_CREATE | IN_MOVED_TO | IN_DELETE | IN_MOVED_FROM | IN_ATTRIB;
            if (inotify_add_watch(descriptor, parent, mask) >= 0)
                count++;
            free(parent);
        }
    }
    return count;
}

static inline int zov_tail_watch_wait(int descriptor, double seconds) {
    struct pollfd event;
    event.fd = descriptor;
    event.events = POLLIN;
    event.revents = 0;
    int timeout = seconds >= 2147483.647 ? 2147483647 : (int) (seconds * 1000.0);
    int ready;
    do {
        ready = poll(&event, 1, timeout);
    } while (ready < 0 && errno == EINTR);
    if (ready <= 0)
        return ready;
    char buffer[8192];
    while (read(descriptor, buffer, sizeof buffer) > 0) { }
    return 1;
}

#else

static inline int zov_tail_watch_open(void) { return -1; }
static inline int zov_tail_watch_add(int descriptor, const char *path, int follow_name) {
    (void) descriptor;
    (void) path;
    (void) follow_name;
    return 0;
}
static inline int zov_tail_watch_wait(int descriptor, double seconds) {
    (void) descriptor;
    (void) seconds;
    return -1;
}

#endif

#endif
