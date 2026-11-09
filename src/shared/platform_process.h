#ifndef ZOVUTILS_PLATFORM_PROCESS_H
#define ZOVUTILS_PLATFORM_PROCESS_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <process.h>
#include <direct.h>
#ifndef SIGHUP
#define SIGHUP 1
#endif
#ifndef SIGQUIT
#define SIGQUIT 3
#endif
#ifndef SIGKILL
#define SIGKILL 9
#endif
#ifndef SIGALRM
#define SIGALRM 14
#endif
#ifndef SIGUSR1
#define SIGUSR1 10
#endif
#ifndef SIGUSR2
#define SIGUSR2 12
#endif
#ifndef WNOHANG
#define WNOHANG 1
#endif
#ifndef WIFEXITED
#define WIFEXITED(s) 1
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(s) (s)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(s) 0
#endif
#ifndef WTERMSIG
#define WTERMSIG(s) 0
#endif
#else
#include <unistd.h>
#include <sys/resource.h>
#include <sys/wait.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

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
#if defined(_WIN32)
    return _putenv_s(name, "");
#else
    return unsetenv(name);
#endif
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

static inline int zov_chroot(const char *path) {
#if defined(__APPLE__)
    extern int chroot(const char *);
#endif
    return chroot(path);
}

static inline int zov_exec_shell(const char *shell) {
    execl(shell, shell, (char *)NULL);
    return errno;
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

static inline long zov_nprocessors_onln(void) {
    long count = 0;
#ifdef __linux__
    FILE *status = fopen("/proc/self/status", "r");
    char line[512];
    if (status) {
        while (fgets(line, sizeof(line), status)) {
            if (strncmp(line, "Cpus_allowed_list:", 18) == 0) {
                char *p = line + 18;
                long total = 0;
                while (*p && *p != '\n') {
                    char *end;
                    long first = strtol(p, &end, 10);
                    if (end == p) break;
                    p = end;
                    long last = first;
                    if (*p == '-') {
                        last = strtol(p + 1, &end, 10);
                        p = end;
                    }
                    if (last >= first) total += last - first + 1;
                    if (*p == ',') ++p;
                    else break;
                }
                if (total > 0) count = total;
                break;
            }
        }
        fclose(status);
    }
#endif
    if (count <= 0) {
#if defined(_WIN32)
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        count = si.dwNumberOfProcessors > 0 ? (long)si.dwNumberOfProcessors : 1;
#else
        count = sysconf(_SC_NPROCESSORS_ONLN);
        if (count <= 0) count = 1;
#endif
    }
    return count;
}

static inline long zov_nprocessors_conf(void) {
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors > 0 ? (long)si.dwNumberOfProcessors : 1;
#else
    long installed = sysconf(_SC_NPROCESSORS_CONF);
    if (installed > 0) return installed;
    return zov_nprocessors_onln();
#endif
}

static inline int zov_getpriority(void) {
#if defined(_WIN32)
    return 0;
#else
    return getpriority(PRIO_PROCESS, 0);
#endif
}

static inline int zov_setpriority(int prio) {
#if defined(_WIN32)
    (void)prio;
    return 0;
#else
    return setpriority(PRIO_PROCESS, 0, prio);
#endif
}

static inline uint64_t zov_random_seed(void) {
#if defined(_WIN32)
    uint64_t seed = (uint64_t)time(NULL) ^ ((uint64_t)GetCurrentProcessId() << 32);
#else
    uint64_t seed = (uint64_t)time(NULL) ^ ((uint64_t)getpid() << 32);
#endif
    return seed != 0 ? seed : 1;
}

static inline void zov_nohup_ignore_signals(void) {
    signal(SIGHUP, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
}

static inline int zov_isatty(int fd) {
    return isatty(fd);
}

static inline int zov_open_devnull_ro(void) {
#if defined(_WIN32)
    return open("NUL", O_RDONLY);
#else
    return open("/dev/null", O_RDONLY);
#endif
}

static inline int zov_open_nohup_out(const char *path) {
    return open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
}

static inline int zov_dup2(int oldfd, int newfd) {
    return dup2(oldfd, newfd);
}

static inline int zov_close(int fd) {
    return close(fd);
}

static inline int zov_fork(void) {
#if defined(_WIN32)
    errno = ENOSYS;
    return -1;
#else
    return (int)fork();
#endif
}

static inline int zov_setpgid(int pid, int pgid) {
#if defined(_WIN32)
    (void)pid; (void)pgid;
    return 0;
#else
    return setpgid((pid_t)pid, (pid_t)pgid);
#endif
}

static inline int zov_waitpid_nohang(int pid, int *status) {
#if defined(_WIN32)
    (void)pid;
    if (status) *status = 0;
    return -1;
#else
    return (int)waitpid((pid_t)pid, status, WNOHANG);
#endif
}

static inline int zov_waitpid_blocking(int pid, int *status) {
#if defined(_WIN32)
    (void)pid;
    if (status) *status = 0;
    return -1;
#else
    return (int)waitpid((pid_t)pid, status, 0);
#endif
}

static inline int zov_kill(int pid, int sig) {
#if defined(_WIN32)
    (void)pid; (void)sig;
    return 0;
#else
    return kill((pid_t)pid, sig);
#endif
}

static inline int zov_wait_status(int st) {
    if (WIFEXITED(st)) return WEXITSTATUS(st);
    if (WIFSIGNALED(st)) return 128 + WTERMSIG(st);
    return 125;
}

static inline void zov_timeout_sleep_step(void) {
#if defined(_WIN32)
    Sleep(10);
#else
    struct timespec pause = {0, 10000000};
    while (nanosleep(&pause, &pause) < 0 && errno == EINTR) {}
#endif
}

static inline int zov_timeout_parse_signal(const char *s) {
    if (!strcmp(s, "TERM") || !strcmp(s, "SIGTERM")) return SIGTERM;
    if (!strcmp(s, "KILL") || !strcmp(s, "SIGKILL")) return SIGKILL;
    if (!strcmp(s, "INT") || !strcmp(s, "SIGINT")) return SIGINT;
    if (!strcmp(s, "HUP") || !strcmp(s, "SIGHUP")) return SIGHUP;
    if (!strcmp(s, "QUIT") || !strcmp(s, "SIGQUIT")) return SIGQUIT;
    if (!strcmp(s, "ALRM") || !strcmp(s, "SIGALRM")) return SIGALRM;
    if (!strcmp(s, "USR1") || !strcmp(s, "SIGUSR1")) return SIGUSR1;
    if (!strcmp(s, "USR2") || !strcmp(s, "SIGUSR2")) return SIGUSR2;
    char *e;
    long n = strtol(s, &e, 10);
    return *e || n <= 0 || n >= NSIG ? -1 : (int)n;
}

static inline const char *zov_timeout_signal_name(int sig) {
    switch (sig) {
        case SIGTERM: return "TERM";
        case SIGKILL: return "KILL";
        case SIGINT: return "INT";
        case SIGHUP: return "HUP";
        case SIGQUIT: return "QUIT";
        case SIGALRM: return "ALRM";
        case SIGUSR1: return "USR1";
        case SIGUSR2: return "USR2";
        default: return "signal";
    }
}

static inline int zov_timeout_duration(const char *value, double *seconds) {
    char *end = NULL;
    errno = 0;
    double amount = strtod(value, &end);
    if (errno || end == value || amount < 0) return 0;
    double scale = 1.0;
    if (*end == 's') end++;
    else if (*end == 'm') { scale = 60.0; end++; }
    else if (*end == 'h') { scale = 3600.0; end++; }
    else if (*end == 'd') { scale = 86400.0; end++; }
    if (*end || amount * scale > 86400.0 * 365.0) return 0;
    *seconds = amount * scale;
    return 1;
}

static inline int zov_sigterm_val(void) { return SIGTERM; }
static inline int zov_sigkill_val(void) { return SIGKILL; }

static inline void zov_exit_process(int status) {
    _exit(status);
}

static inline int zov_stdbuf_size(const char *mode) {
    if (!mode || !*mode) return 0;
    if (!strcmp(mode, "0") || !strcmp(mode, "L")) return 1;
    if (*mode == '-') return 0;
    errno = 0;
    char *end = NULL;
    unsigned long long n = strtoull(mode, &end, 10);
    if (errno || end == mode) return 0;
    unsigned long long multiplier = 1;
    if (!*end) multiplier = 1;
    else if (!strcmp(end, "k") || !strcmp(end, "K") || !strcmp(end, "KiB")) multiplier = 1024ULL;
    else if (!strcmp(end, "KB")) multiplier = 1000ULL;
    else if (!strcmp(end, "M") || !strcmp(end, "MiB")) multiplier = 1024ULL * 1024ULL;
    else if (!strcmp(end, "MB")) multiplier = 1000ULL * 1000ULL;
    else if (!strcmp(end, "G") || !strcmp(end, "GiB")) multiplier = 1024ULL * 1024ULL * 1024ULL;
    else if (!strcmp(end, "GB")) multiplier = 1000ULL * 1000ULL * 1000ULL;
    else if (!strcmp(end, "T") || !strcmp(end, "TiB")) multiplier = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
    else if (!strcmp(end, "TB")) multiplier = 1000ULL * 1000ULL * 1000ULL * 1000ULL;
    else return 0;
    return n <= (unsigned long long)SIZE_MAX / multiplier;
}

static inline int zov_stdbuf_setup_preload_slice(
    const char *mode_i, size_t i_len,
    const char *mode_o, size_t o_len,
    const char *mode_e, size_t e_len
) {
    if (i_len > 0) {
        char buf[128];
        if (i_len >= sizeof buf) return -1;
        memcpy(buf, mode_i, i_len); buf[i_len] = '\0';
        if (setenv("_STDBUF_I", buf, 1) != 0) return errno;
    }
    if (o_len > 0) {
        char buf[128];
        if (o_len >= sizeof buf) return -1;
        memcpy(buf, mode_o, o_len); buf[o_len] = '\0';
        if (setenv("_STDBUF_O", buf, 1) != 0) return errno;
    }
    if (e_len > 0) {
        char buf[128];
        if (e_len >= sizeof buf) return -1;
        memcpy(buf, mode_e, e_len); buf[e_len] = '\0';
        if (setenv("_STDBUF_E", buf, 1) != 0) return errno;
    }
    char exe[PATH_MAX];
#if defined(__APPLE__)
    uint32_t size = (uint32_t)sizeof exe;
    char unresolved[PATH_MAX];
    if (_NSGetExecutablePath(unresolved, &size) != 0 || realpath(unresolved, exe) == NULL) return -1;
#else
    ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n < 0) return errno;
    exe[n] = 0;
#endif
    char *slash = strrchr(exe, '/');
    if (!slash) return -1;
    strcpy(slash + 1, "libzovstdbuf.dylib");
#if defined(__APPLE__)
    if (setenv("DYLD_INSERT_LIBRARIES", exe, 1) != 0) return errno;
    setenv("DYLD_FORCE_FLAT_NAMESPACE", "1", 1);
#else
    const char *old = getenv("LD_PRELOAD");
    char preload[PATH_MAX * 2];
    if (old && *old) snprintf(preload, sizeof preload, "%s:%s", exe, old);
    else snprintf(preload, sizeof preload, "%s", exe);
    if (setenv("LD_PRELOAD", preload, 1) != 0) return errno;
#endif
    return 0;
}

static inline FILE *zov_popen_write(const char *command) {
#if defined(_WIN32)
    return _popen(command, "w");
#else
    return popen(command, "w");
#endif
}

static inline int zov_pclose(FILE *pipe) {
#if defined(_WIN32)
    return _pclose(pipe);
#else
    return pclose(pipe);
#endif
}

static inline int zov_setenv(const char *name, const char *value, int overwrite) {
#if defined(_WIN32)
    if (!overwrite && getenv(name) != NULL) return 0;
    return _putenv_s(name, value);
#else
    return setenv(name, value, overwrite);
#endif
}

#endif

