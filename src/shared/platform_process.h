#ifndef ZOVUTILS_PLATFORM_PROCESS_H
#define ZOVUTILS_PLATFORM_PROCESS_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <time.h>

#ifndef NSIG
#define NSIG 32
#endif

extern char **environ;

static inline int zov_env_assign(const char *assignment) {
    const char *separator = strchr(assignment, '=');
    if (separator == NULL || separator == assignment) return -1;
    size_t name_length = (size_t)(separator - assignment);
    int existed = 0;
    for (char **entry = environ; entry != NULL && *entry != NULL; ++entry) {
        if (strncmp(*entry, assignment, name_length) == 0 && (*entry)[name_length] == '=') {
            existed = 1;
            break;
        }
    }
    int result = putenv((char *)assignment);
#ifdef __APPLE__
    if (result == 0 && !existed) {
        size_t index = 0;
        while (environ[index] != NULL) {
            if (strncmp(environ[index], assignment, name_length) == 0 &&
                environ[index][name_length] == '=') break;
            ++index;
        }
        if (environ[index] != NULL && index > 0) {
            char *inserted = environ[index];
            memmove(environ + 1, environ, index * sizeof *environ);
            environ[0] = inserted;
        }
    }
#else
    (void)existed;
#endif
    return result;
}

static inline int zov_get_errno(void) {
    return errno;
}

static inline int zov_env_unset(const char *name) {
    return unsetenv(name);
}

static inline int zov_env_clear(void) {
    while (environ != NULL && environ[0] != NULL) {
        char *entry = environ[0];
        char *separator = strchr(entry, '=');
        if (separator == NULL) {
            return -1;
        }
        size_t length = (size_t)(separator - entry);
        char *name = (char *)malloc(length + 1);
        if (name == NULL) {
            return -1;
        }
        memcpy(name, entry, length);
        name[length] = '\0';
        int result = unsetenv(name);
        free(name);
        if (result != 0) {
            return -1;
        }
    }
    return 0;
}

static inline int zov_env_print(int zero) {
    if (environ == NULL) {
        return 0;
    }
    for (char **entry = environ; *entry != NULL; ++entry) {
        size_t length = strlen(*entry);
        if (fwrite(*entry, 1, length, stdout) != length ||
            fputc(zero ? '\0' : '\n', stdout) == EOF) {
            return -1;
        }
    }
    return ferror(stdout) ? -1 : 0;
}

static inline int zov_change_directory(const char *path) {
    return chdir(path);
}

static inline int zov_exec_command(const char *file, void *argv) {
    execvp(file, (char *const *)argv);
    return errno;
}

static inline int zov_signal_number(const char *text) {
    if (text == NULL || text[0] == '\0') {
        return -1;
    }
    char *end = NULL;
    errno = 0;
    long number = strtol(text, &end, 10);
    if (end != text && *end == '\0') {
        if (errno != 0 || number <= 0 || number >= NSIG) {
            return -1;
        }
        return (int)number;
    }
    char canonical[128];
    size_t text_length = strlen(text);
    if (text_length >= sizeof canonical) {
        return -1;
    }
    for (size_t i = 0; i <= text_length; ++i) canonical[i] = (char)toupper((unsigned char)text[i]);
    text = canonical;
    if (strncmp(text, "SIG", 3) == 0) text += 3;
#ifdef SIGRTMIN
    if (strncmp(text, "RTMIN", 5) == 0) {
        if (text[5] == '\0') return SIGRTMIN;
        if (text[5] == '+') {
            char *rt_end = NULL;
            long offset = strtol(text + 6, &rt_end, 10);
            if (rt_end != text + 6 && *rt_end == '\0' && offset >= 0 && SIGRTMIN + offset <= SIGRTMAX)
                return (int)(SIGRTMIN + offset);
        }
    }
    if (strcmp(text, "RTMAX") == 0) return SIGRTMAX;
#endif
#define ZOV_SIG(name) if (strcmp(text, #name) == 0) return SIG##name
    ZOV_SIG(HUP); ZOV_SIG(INT); ZOV_SIG(QUIT); ZOV_SIG(ILL); ZOV_SIG(TRAP);
    ZOV_SIG(ABRT); ZOV_SIG(BUS); ZOV_SIG(FPE); ZOV_SIG(KILL); ZOV_SIG(USR1);
    ZOV_SIG(SEGV); ZOV_SIG(USR2); ZOV_SIG(PIPE); ZOV_SIG(ALRM); ZOV_SIG(TERM);
    ZOV_SIG(CHLD); ZOV_SIG(CONT); ZOV_SIG(STOP); ZOV_SIG(TSTP); ZOV_SIG(TTIN);
    ZOV_SIG(TTOU); ZOV_SIG(URG); ZOV_SIG(XCPU); ZOV_SIG(XFSZ); ZOV_SIG(VTALRM);
    ZOV_SIG(PROF);
#ifdef SIGWINCH
    ZOV_SIG(WINCH);
#endif
#ifdef SIGIO
    ZOV_SIG(IO);
#endif
#ifdef SIGSYS
    ZOV_SIG(SYS);
#endif
#ifdef SIGPOLL
    if (strcmp(text, "POLL") == 0) return SIGPOLL;
#endif
#ifdef SIGPWR
    if (strcmp(text, "PWR") == 0) return SIGPWR;
#endif
#ifdef SIGIOT
    if (strcmp(text, "IOT") == 0) return SIGIOT;
#endif
#ifdef SIGCLD
    if (strcmp(text, "CLD") == 0) return SIGCLD;
#endif
#undef ZOV_SIG
    return -1;
}

static inline int zov_signal_apply(int mode, const char *list, int all) {
    if (!all && (list == NULL || list[0] == '\0')) {
        return 0;
    }
    if (mode == 0) {
        sigset_t set;
        if (all) {
            if (sigfillset(&set) != 0 || sigprocmask(SIG_BLOCK, &set, NULL) != 0) return -2;
            return 0;
        }
        if (sigemptyset(&set) != 0) return -2;
        const char *start = list;
        for (const char *p = list;; ++p) {
            if (*p == ',' || *p == '\0') {
                size_t length = (size_t)(p - start);
                char name[128];
                if (length == 0 || length >= sizeof name) return (int)(start - list) + 1;
                memcpy(name, start, length); name[length] = '\0';
                int signal_number = zov_signal_number(name);
                if (signal_number < 0) return (int)(start - list) + 1;
                if (sigaddset(&set, signal_number) != 0) return -2;
                if (*p == '\0') break;
                start = p + 1;
            }
        }
        return sigprocmask(SIG_BLOCK, &set, NULL) == 0 ? 0 : -2;
    }
    if (all) {
        for (int signal_number = 1; signal_number < NSIG; ++signal_number) {
            if (signal_number == SIGKILL || signal_number == SIGSTOP) continue;
            struct sigaction action;
            memset(&action, 0, sizeof action);
            action.sa_handler = mode == 1 ? SIG_DFL : SIG_IGN;
            sigemptyset(&action.sa_mask);
            if (sigaction(signal_number, &action, NULL) != 0 && errno != EINVAL) return -2;
        }
        return 0;
    }
    const char *start = list;
    for (const char *p = list;; ++p) {
        if (*p == ',' || *p == '\0') {
            size_t length = (size_t)(p - start);
            char name[128];
            if (length == 0 || length >= sizeof name) return (int)(start - list) + 1;
            memcpy(name, start, length); name[length] = '\0';
            int signal_number = zov_signal_number(name);
            if (signal_number < 0) return (int)(start - list) + 1;
            struct sigaction action;
            memset(&action, 0, sizeof action);
            action.sa_handler = mode == 1 ? SIG_DFL : SIG_IGN;
            sigemptyset(&action.sa_mask);
            if (sigaction(signal_number, &action, NULL) != 0) return -2;
            if (*p == '\0') break;
            start = p + 1;
        }
    }
    return 0;
}

static inline const char *zov_signal_name(int signal_number) {
    static char realtime_name[32];
#define ZOV_SIG_NAME(name) if (signal_number == SIG##name) return #name
    ZOV_SIG_NAME(HUP); ZOV_SIG_NAME(INT); ZOV_SIG_NAME(QUIT); ZOV_SIG_NAME(ILL);
    ZOV_SIG_NAME(TRAP); ZOV_SIG_NAME(ABRT); ZOV_SIG_NAME(BUS); ZOV_SIG_NAME(FPE);
    ZOV_SIG_NAME(KILL); ZOV_SIG_NAME(USR1); ZOV_SIG_NAME(SEGV); ZOV_SIG_NAME(USR2);
    ZOV_SIG_NAME(PIPE); ZOV_SIG_NAME(ALRM); ZOV_SIG_NAME(TERM); ZOV_SIG_NAME(CHLD);
    ZOV_SIG_NAME(CONT); ZOV_SIG_NAME(STOP); ZOV_SIG_NAME(TSTP); ZOV_SIG_NAME(TTIN);
    ZOV_SIG_NAME(TTOU); ZOV_SIG_NAME(URG); ZOV_SIG_NAME(XCPU); ZOV_SIG_NAME(XFSZ);
    ZOV_SIG_NAME(VTALRM); ZOV_SIG_NAME(PROF);
#ifdef SIGWINCH
    ZOV_SIG_NAME(WINCH);
#endif
#ifdef SIGIO
    ZOV_SIG_NAME(IO);
#endif
#ifdef SIGSYS
    ZOV_SIG_NAME(SYS);
#endif
#undef ZOV_SIG_NAME
#ifdef SIGPOLL
    if (signal_number == SIGPOLL) return "POLL";
#endif
#ifdef SIGPWR
    if (signal_number == SIGPWR) return "PWR";
#endif
#ifdef SIGIOT
    if (signal_number == SIGIOT) return "IOT";
#endif
#ifdef SIGCLD
    if (signal_number == SIGCLD) return "CLD";
#endif
#ifdef SIGRTMIN
    if (signal_number >= SIGRTMIN && signal_number <= SIGRTMAX) {
        int offset = signal_number - SIGRTMIN;
        if (offset == 0) return "RTMIN";
        snprintf(realtime_name, sizeof realtime_name, "RTMIN+%d", offset);
        return realtime_name;
    }
#endif
    return "UNKNOWN";
}

static inline int zov_signal_list(void) {
    sigset_t blocked;
    if (sigprocmask(SIG_BLOCK, NULL, &blocked) != 0) return -1;
    for (int signal_number = 1; signal_number < NSIG; ++signal_number) {
        struct sigaction action;
        if (sigaction(signal_number, NULL, &action) != 0) continue;
        int is_blocked = sigismember(&blocked, signal_number) == 1;
        int is_ignored = action.sa_handler == SIG_IGN;
        if (!is_blocked && !is_ignored) continue;
        fprintf(stderr, "%-10s (%2d): %s\n", zov_signal_name(signal_number), signal_number,
                is_ignored ? "IGNORE" : "BLOCK");
    }
    return ferror(stderr) ? -1 : 0;
}

static inline int zov_nanosleep_seconds(double seconds) {
    if (seconds <= 0.0) return 0;
    time_t whole = (time_t)seconds;
    double fraction = seconds - (double)whole;
    struct timespec request;
    request.tv_sec = whole;
    request.tv_nsec = (long)(fraction * 1000000000.0);
    if (request.tv_nsec >= 1000000000L) {
        request.tv_sec += 1;
        request.tv_nsec -= 1000000000L;
    }
    while (nanosleep(&request, &request) != 0) {
        if (errno != EINTR) return 2;
    }
    return 0;
}

static inline void zov_signal_ignore_interrupt(void) {
    signal(SIGINT, SIG_IGN);
}

static inline void zov_signal_ignore_pipe(void) {
    signal(SIGPIPE, SIG_IGN);
}

#endif
