#ifndef ZOVUTILS_PLATFORM_UTMPX_H
#define ZOVUTILS_PLATFORM_UTMPX_H

#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>

#ifndef RUN_LVL
#define RUN_LVL 1
#endif

static inline void zov_utmpx_start(void) {
    setutxent();
}

static inline void zov_utmpx_end(void) {
    endutxent();
}

static inline int zov_utmpx_next_entry(
    int *ut_type,
    int64_t *tv_sec,
    int *pid,
    char *user, size_t user_sz,
    char *line, size_t line_sz,
    char *id, size_t id_sz,
    char *host, size_t host_sz,
    int *runlevel
) {
    struct utmpx *u = getutxent();
    if (!u) return 0;
    if (ut_type) *ut_type = (int)u->ut_type;
    if (tv_sec) *tv_sec = (int64_t)u->ut_tv.tv_sec;
    if (pid) *pid = (int)u->ut_pid;

    if (user && user_sz > 0) {
        size_t n = 0;
        while (n < sizeof(u->ut_user) && u->ut_user[n] && n + 1 < user_sz) {
            user[n] = u->ut_user[n];
            n++;
        }
        user[n] = '\0';
    }
    if (line && line_sz > 0) {
        size_t n = 0;
        while (n < sizeof(u->ut_line) && u->ut_line[n] && n + 1 < line_sz) {
            line[n] = u->ut_line[n];
            n++;
        }
        line[n] = '\0';
    }
    if (id && id_sz > 0) {
        size_t n = 0;
        while (n < sizeof(u->ut_id) && u->ut_id[n] && n + 1 < id_sz) {
            id[n] = u->ut_id[n];
            n++;
        }
        id[n] = '\0';
    }
    if (host && host_sz > 0) {
#if !defined(__APPLE__)
        size_t n = 0;
        while (n < sizeof(u->ut_host) && u->ut_host[n] && n + 1 < host_sz) {
            host[n] = u->ut_host[n];
            n++;
        }
        host[n] = '\0';
#else
        host[0] = '\0';
#endif
    }
    if (runlevel) {
        int level = (unsigned char)u->ut_pid;
        if (level >= '0' && level <= '9') {
            *runlevel = level - '0';
        } else {
#if defined(__linux__)
            *runlevel = u->ut_exit.__e_termination;
#else
            *runlevel = 0;
#endif
        }
    }
#if defined(__linux__)
    zov_utmp_last_term = (int)u->ut_exit.__e_termination;
    zov_utmp_last_exit = (int)u->ut_exit.__e_exit;
#else
    zov_utmp_last_term = 0;
    zov_utmp_last_exit = 0;
#endif
    return 1;
}

static int zov_utmp_last_term = 0;
static int zov_utmp_last_exit = 0;

static inline int zov_utmpx_last_term(void) { return zov_utmp_last_term; }
static inline int zov_utmpx_last_exit(void) { return zov_utmp_last_exit; }

static inline int zov_utmp_type_user(void) { return USER_PROCESS; }
static inline int zov_utmp_type_boot(void) { return BOOT_TIME; }
static inline int zov_utmp_type_runlvl(void) { return RUN_LVL; }
static inline int zov_utmp_type_login(void) { return LOGIN_PROCESS; }
static inline int zov_utmp_type_init(void) { return INIT_PROCESS; }
static inline int zov_utmp_type_dead(void) { return DEAD_PROCESS; }
static inline int zov_utmp_type_old_time(void) { return OLD_TIME; }
static inline int zov_utmp_type_new_time(void) { return NEW_TIME; }

static inline void zov_utmpx_format_time(int64_t sec, char *buf, size_t buflen) {
    time_t t = (time_t)sec;
    struct tm tmv;
    if (localtime_r(&t, &tmv)) {
        strftime(buf, buflen, "%b %e %H:%M", &tmv);
    } else {
        if (buflen > 0) buf[0] = '\0';
    }
}

static inline void zov_utmpx_device_status(const char *line, char *mesg_out, char *idle_buf, size_t idle_sz) {
    char device[PATH_MAX];
    snprintf(device, sizeof device, "/dev/%s", line);
    char mesg = '?';
    if (idle_sz > 0) {
        strncpy(idle_buf, "old", idle_sz);
        idle_buf[idle_sz - 1] = '\0';
    }
    struct stat st;
    if (stat(device, &st) == 0) {
        mesg = access(device, W_OK) == 0 ? '+' : '-';
        time_t now = time(NULL);
        if (now >= st.st_atime) {
            long long elapsed = (long long)now - (long long)st.st_atime;
            if (elapsed < 60) {
                if (idle_sz > 1) {
                    idle_buf[0] = '.';
                    idle_buf[1] = '\0';
                }
            } else if (elapsed < 86400) {
                snprintf(idle_buf, idle_sz, "%02lld:%02lld", elapsed / 3600, (elapsed / 60) % 60);
            }
        }
    }
#if defined(__APPLE__)
    if (!strcmp(line, "console")) {
        mesg = '-';
        if (idle_sz > 3) {
            strcpy(idle_buf, "old");
        }
    }
#endif
    if (mesg_out) *mesg_out = mesg;
}

static inline void zov_pinky_tty_info(const char *line, int64_t logged, int show_idle, char *tty_buf, size_t tty_sz, char *idle_buf, size_t idle_sz) {
    char device[PATH_MAX];
    snprintf(device, sizeof device, "/dev/%s", line);
    if (idle_sz > 0) idle_buf[0] = '\0';
    time_t now = time(NULL);
    struct stat st;
    if (show_idle && now >= (time_t)logged && stat(device, &st) == 0) {
        long long seconds = (long long)now - (long long)st.st_atime;
        if (seconds >= 60) {
            long long minutes = seconds / 60;
            if (minutes >= 1440) {
                snprintf(idle_buf, idle_sz, "%lldd", minutes / 1440);
            } else {
                snprintf(idle_buf, idle_sz, "%02lld:%02lld", minutes / 60, minutes % 60);
            }
        }
    }
    int unwriteable = (!strcmp(line, "console") || access(device, W_OK) != 0);
    snprintf(tty_buf, tty_sz, "%s%s", unwriteable ? "*" : "", line);
}

static inline int zov_get_user_gecos(const char *user, char *gecos_buf, size_t gecos_sz, char *dir_buf, size_t dir_sz, char *shell_buf, size_t shell_sz) {
    struct passwd *p = getpwnam(user);
    if (!p) return 0;
    if (gecos_buf) {
        snprintf(gecos_buf, gecos_sz, "%s", p->pw_gecos ? p->pw_gecos : "");
        char *comma = strchr(gecos_buf, ',');
        if (comma) *comma = '\0';
    }
    if (dir_buf) {
        snprintf(dir_buf, dir_sz, "%s", p->pw_dir ? p->pw_dir : "");
    }
    if (shell_buf) {
        snprintf(shell_buf, shell_sz, "%s", p->pw_shell ? p->pw_shell : "");
    }
    return 1;
}

#endif
