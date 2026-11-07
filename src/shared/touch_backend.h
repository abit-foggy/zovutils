#ifndef ZOVUTILS_TOUCH_BACKEND_H
#define ZOVUTILS_TOUCH_BACKEND_H

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include "platform_quote.h"

#if defined(__APPLE__) || defined(__linux__)
extern time_t timegm(struct tm *);
#endif

#ifndef AT_FDCWD
#define AT_FDCWD -100
#endif
#ifndef AT_SYMLINK_NOFOLLOW
#define AT_SYMLINK_NOFOLLOW 0x100
#endif
#ifndef UTIME_NOW
#define UTIME_NOW ((1l << 30) - 1l)
#endif
#ifndef UTIME_OMIT
#define UTIME_OMIT ((1l << 30) - 2l)
#endif

static inline int zov_touch_long(const char *arg, const char *name, size_t keylen) {
    return keylen > 2 && keylen <= strlen(name) && strncmp(arg, name, keylen) == 0;
}

static inline void zov_touch_help(void) {
    fputs("Usage: touch [OPTION]... FILE...\n"
          "Update the access and modification times of each FILE to the current time.\n"
          "\n"
          "A FILE argument that does not exist is created empty, unless -c or -h\n"
          "is supplied.\n"
          "\n"
          "A FILE argument string of - is handled specially and causes touch to\n"
          "change the times of the file associated with standard output.\n"
          "\n"
          "Mandatory arguments to long options are mandatory for short options too.\n"
          "  -a\n"
          "         change only the access time\n"
          "  -c, --no-create\n"
          "         do not create any files\n"
          "  -d, --date=STRING\n"
          "         parse STRING and use it instead of current time\n"
          "  -f\n"
          "         (ignored)\n"
          "  -h, --no-dereference\n"
          "         affect each symbolic link instead of any referenced file;\n"
          "         useful only on systems that can change the timestamps of a symlink\n"
          "  -m\n"
          "         change only the modification time\n"
          "  -r, --reference=FILE\n"
          "         use this file's times instead of current time\n"
          "  -t [[CC]YY]MMDDhhmm[.ss]\n"
          "         use specified time instead of current time,\n"
          "         with a date-time format that differs from -d's\n"
          "      --time=WORD\n"
          "         specify which time to change:\n"
          "         access time (-a): 'access', 'atime', 'use';\n"
          "         modification time (-m): 'modify', 'mtime'\n"
          "      --help\n"
          "         display this help and exit\n"
          "      --version\n"
          "         output version information and exit\n"
          "\n"
          "Report bugs to: bug-coreutils@gnu.org\n"
          "GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n"
          "General help using GNU software: <https://www.gnu.org/gethelp/>\n"
          "Report any translation bugs to <https://translationproject.org/team/>\n"
          "Full documentation <https://www.gnu.org/software/coreutils/touch>\n"
          "or available locally via: info '(coreutils) touch invocation'\n", stdout);
}

static inline int zov_touch_parse_int(const char *text, size_t length, int *value) {
    int result = 0;
    if (length == 0) return 0;
    for (size_t i = 0; i < length; ++i) {
        if (text[i] < '0' || text[i] > '9') return 0;
        result = result * 10 + text[i] - '0';
    }
    *value = result;
    return 1;
}

static inline int zov_touch_valid_tm(struct tm *tm, time_t *out) {
    int year = tm->tm_year, mon = tm->tm_mon, mday = tm->tm_mday;
    int hour = tm->tm_hour, minute = tm->tm_min, second = tm->tm_sec;
    tm->tm_isdst = -1;
    time_t result = mktime(tm);
    if (result == (time_t)-1) return 0;
    struct tm *roundtrip = localtime(&result);
    if (roundtrip == NULL || roundtrip->tm_year != year || roundtrip->tm_mon != mon ||
        roundtrip->tm_mday != mday || roundtrip->tm_hour != hour ||
        roundtrip->tm_min != minute || roundtrip->tm_sec != second) return 0;
    *out = result;
    return 1;
}

static inline int zov_touch_valid_utc_tm(struct tm *tm, time_t *out) {
#if defined(__APPLE__) || defined(__linux__)
    int year = tm->tm_year, mon = tm->tm_mon, mday = tm->tm_mday;
    int hour = tm->tm_hour, minute = tm->tm_min, second = tm->tm_sec;
    time_t result = timegm(tm);
    if (result == (time_t)-1) return 0;
    struct tm *roundtrip = gmtime(&result);
    if (roundtrip == NULL || roundtrip->tm_year != year || roundtrip->tm_mon != mon ||
        roundtrip->tm_mday != mday || roundtrip->tm_hour != hour ||
        roundtrip->tm_min != minute || roundtrip->tm_sec != second) return 0;
    *out = result;
    return 1;
#else
    return zov_touch_valid_tm(tm, out);
#endif
}

static inline int zov_touch_parse_fraction(const char *text, size_t length, long *nanos) {
    if (length == 0) return 0;
    long value = 0;
    size_t used = 0;
    for (; used < length && used < 9; ++used) {
        if (text[used] < '0' || text[used] > '9') return 0;
        value = value * 10 + text[used] - '0';
    }
    for (; used < 9; ++used) value *= 10;
    for (size_t i = 9; i < length; ++i)
        if (text[i] < '0' || text[i] > '9') return 0;
    *nanos = value;
    return 1;
}

static inline int zov_touch_parse_stamp(const char *text, struct timespec *value) {
    size_t length = strlen(text);
    const char *dot = strchr(text, '.');
    size_t digits = dot == NULL ? length : (size_t)(dot - text);
    int second = 0;
    if (dot != NULL && (!zov_touch_parse_int(dot + 1, strlen(dot + 1), &second) ||
                        strlen(dot + 1) > 2 || second > 59)) return 0;
    if (digits != 8 && digits != 10 && digits != 12) return 0;
    int year = 0, month, day, hour, minute;
    size_t offset = 0;
    if (digits == 12) {
        if (!zov_touch_parse_int(text, 4, &year)) return 0;
        offset = 4;
    } else {
        int short_year = 0;
        if (digits == 10) {
            if (!zov_touch_parse_int(text, 2, &short_year)) return 0;
            year = short_year >= 69 ? 1900 + short_year : 2000 + short_year;
            offset = 2;
        } else {
            time_t now = time(NULL);
            struct tm *current = localtime(&now);
            if (current == NULL) return 0;
            year = current->tm_year + 1900;
        }
    }
    if (!zov_touch_parse_int(text + offset, 2, &month) ||
        !zov_touch_parse_int(text + offset + 2, 2, &day) ||
        !zov_touch_parse_int(text + offset + 4, 2, &hour) ||
        !zov_touch_parse_int(text + offset + 6, 2, &minute)) return 0;
    struct tm tm_value;
    memset(&tm_value, 0, sizeof(tm_value));
    tm_value.tm_year = year - 1900;
    tm_value.tm_mon = month - 1;
    tm_value.tm_mday = day;
    tm_value.tm_hour = hour;
    tm_value.tm_min = minute;
    tm_value.tm_sec = second;
    if (!zov_touch_valid_tm(&tm_value, &value->tv_sec)) return 0;
    value->tv_nsec = 0;
    return 1;
}

static inline int zov_touch_parse_date(const char *text, struct timespec *value) {
    if (strcmp(text, "now") == 0) {
        value->tv_sec = time(NULL);
        value->tv_nsec = 0;
        return value->tv_sec != (time_t)-1;
    }
    if (text[0] == '@') {
        const char *number = text + 1;
        int negative = 0;
        if (*number == '-' || *number == '+') { negative = *number == '-'; ++number; }
        const char *dot = strchr(number, '.');
        size_t whole_length = dot == NULL ? strlen(number) : (size_t)(dot - number);
        if (whole_length == 0) return 0;
        for (size_t i = 0; i < whole_length; ++i)
            if (number[i] < '0' || number[i] > '9') return 0;
        char whole_text[64];
        if (whole_length >= sizeof(whole_text)) return 0;
        memcpy(whole_text, number, whole_length);
        whole_text[whole_length] = '\0';
        errno = 0;
        long long whole = strtoll(whole_text, NULL, 10);
        if (errno != 0) return 0;
        long nanos = 0;
        if (dot != NULL && !zov_touch_parse_fraction(dot + 1, strlen(dot + 1), &nanos)) return 0;
        long long seconds = negative ? -whole : whole;
        if (negative && nanos != 0) {
            if (seconds == LLONG_MIN) return 0;
            --seconds;
            nanos = 1000000000L - nanos;
        }
        value->tv_sec = (time_t)seconds;
        if ((long long)value->tv_sec != seconds) return 0;
        value->tv_nsec = nanos;
        return 1;
    }
    if (strcmp(text, "today") == 0 || strcmp(text, "yesterday") == 0 || strcmp(text, "tomorrow") == 0) {
        time_t now = time(NULL);
        struct tm *current = localtime(&now);
        if (current == NULL) return 0;
        struct tm tm_value = *current;
        tm_value.tm_hour = tm_value.tm_min = tm_value.tm_sec = 0;
        if (strcmp(text, "yesterday") == 0) --tm_value.tm_mday;
        if (strcmp(text, "tomorrow") == 0) ++tm_value.tm_mday;
        tm_value.tm_isdst = -1;
        value->tv_sec = mktime(&tm_value);
        value->tv_nsec = 0;
        return value->tv_sec != (time_t)-1;
    }
    int year, month, day, hour = 0, minute = 0, second = 0;
    size_t length = strlen(text), end = 10;
    if (length < 10 || text[4] != '-' || text[7] != '-' ||
        !zov_touch_parse_int(text, 4, &year) ||
        !zov_touch_parse_int(text + 5, 2, &month) ||
        !zov_touch_parse_int(text + 8, 2, &day) || year < 1 || month < 1 || month > 12 || day < 1 || day > 31) return 0;
    int utc = 0;
    long nanos = 0;
    if (length > 10) {
        if ((text[10] != ' ' && text[10] != 'T' && text[10] != 't') || length < 16 ||
            text[13] != ':' || !zov_touch_parse_int(text + 11, 2, &hour) ||
            !zov_touch_parse_int(text + 14, 2, &minute)) return 0;
        end = 16;
        if (length > end && text[end] == ':') {
            if (length < end + 3 || !zov_touch_parse_int(text + end + 1, 2, &second)) return 0;
            end += 3;
        }
        if (length > end && text[end] == '.') {
            size_t fraction_start = ++end;
            while (end < length && text[end] >= '0' && text[end] <= '9') ++end;
            if (!zov_touch_parse_fraction(text + fraction_start, end - fraction_start, &nanos)) return 0;
        }
        if (length == end + 1 && (text[end] == 'Z' || text[end] == 'z')) utc = 1;
        else if (strcmp(text + end, " UTC") == 0 || strcmp(text + end, " GMT") == 0) utc = 1;
        else if (length != end) return 0;
    }
    struct tm tm_value;
    memset(&tm_value, 0, sizeof(tm_value));
    tm_value.tm_year = year - 1900;
    tm_value.tm_mon = month - 1;
    tm_value.tm_mday = day;
    tm_value.tm_hour = hour;
    tm_value.tm_min = minute;
    tm_value.tm_sec = second;
    if (!(utc ? zov_touch_valid_utc_tm(&tm_value, &value->tv_sec)
              : zov_touch_valid_tm(&tm_value, &value->tv_sec))) return 0;
    value->tv_nsec = nanos;
    return 1;
}

static inline void zov_touch_get_stat_times(const struct stat *info, struct timespec times[2]) {
#if defined(__APPLE__)
    times[0] = info->st_atimespec;
    times[1] = info->st_mtimespec;
#else
    times[0] = info->st_atim;
    times[1] = info->st_mtim;
#endif
}

static inline void zov_touch_error(const char *operation, const char *path, int error) {
    fprintf(stderr, "touch: %s ", operation);
    zov_quote_shell(stderr, path);
    fprintf(stderr, ": %s\n", strerror(error));
}

static inline int zov_touch_run(int argc, int8_t **argv) {
    int access_time = 1, modify_time = 1, selected_times = 0, no_create = 0, no_dereference = 0;
    int options_done = 0;
    const char *date_text = NULL, *stamp_text = NULL, *reference = NULL;
    char **files = (char **)malloc((size_t)(argc + 1) * sizeof(*files));
    if (files == NULL) return 1;
    int file_count = 0;
    for (int i = 0; i < argc; ++i) {
        char *arg = (char *)argv[i];
        if (!options_done && strcmp(arg, "--") == 0) { options_done = 1; continue; }
        if (!options_done && strncmp(arg, "--", 2) == 0) {
            char *equal = strchr(arg, '=');
            size_t keylen = equal ? (size_t)(equal - arg) : strlen(arg);
            const char *value = equal ? equal + 1 : NULL;
            if (zov_touch_long(arg, "--help", keylen) || zov_touch_long(arg, "--version", keylen)) {
                if (value != NULL) { fprintf(stderr, "touch: option '%s' doesn't allow an argument\n", keylen == 6 ? "--help" : "--version"); free(files); return 1; }
                if (keylen == 6) { zov_touch_help(); free(files); return 0; }
                puts("touch (zovutils) 0.10.0"); free(files); return 0;
            }
            if (zov_touch_long(arg, "--no-create", keylen)) {
                if (value != NULL) { fputs("touch: option '--no-create' doesn't allow an argument\n", stderr); free(files); return 1; }
                no_create = 1; continue;
            }
            if (zov_touch_long(arg, "--no-dereference", keylen)) {
                if (value != NULL) { fputs("touch: option '--no-dereference' doesn't allow an argument\n", stderr); free(files); return 1; }
                no_dereference = 1; continue;
            }
            if (zov_touch_long(arg, "--date", keylen) || zov_touch_long(arg, "--reference", keylen) || zov_touch_long(arg, "--time", keylen)) {
                const char *name = zov_touch_long(arg, "--date", keylen) ? "--date" : zov_touch_long(arg, "--reference", keylen) ? "--reference" : "--time";
                if (value == NULL && i + 1 < argc) value = (char *)argv[++i];
                if (value == NULL) { fprintf(stderr, "touch: option '%s' requires an argument\nTry 'touch --help' for more information.\n", name); free(files); return 1; }
                if (strcmp(name, "--date") == 0) date_text = value;
                else if (strcmp(name, "--reference") == 0) reference = value;
                else if (strcmp(value, "access") == 0 || strcmp(value, "use") == 0 || strcmp(value, "atime") == 0) { selected_times |= 1; }
                else if (strcmp(value, "modify") == 0 || strcmp(value, "mtime") == 0) { selected_times |= 2; }
                else { fprintf(stderr, "touch: invalid argument '%s' for '--time'\nValid arguments are:\n  - 'atime', 'access', 'use'\n  - 'mtime', 'modify'\nTry 'touch --help' for more information.\n", value); free(files); return 1; }
                continue;
            }
            fprintf(stderr, "touch: unrecognized option '%s'\nTry 'touch --help' for more information.\n", arg); free(files); return 1;
        }
        if (!options_done && arg[0] == '-' && arg[1] != '\0') {
            for (size_t j = 1; arg[j] != '\0'; ++j) {
                char option = arg[j];
                if (option == 'a') selected_times |= 1;
                else if (option == 'm') selected_times |= 2;
                else if (option == 'c') no_create = 1;
                else if (option == 'h') no_dereference = 1;
                else if (option == 'f') { }
                else if (option == 'd' || option == 'r' || option == 't') {
                    const char *value = arg[j + 1] != '\0' ? arg + j + 1 : (i + 1 < argc ? (char *)argv[++i] : NULL);
                    if (value == NULL) { fprintf(stderr, "touch: option requires an argument -- '%c'\nTry 'touch --help' for more information.\n", option); free(files); return 1; }
                    if (option == 'd') date_text = value;
                    else if (option == 'r') reference = value;
                    else stamp_text = value;
                    break;
                } else { fprintf(stderr, "touch: invalid option -- '%c'\nTry 'touch --help' for more information.\n", option); free(files); return 1; }
            }
            continue;
        }
        files[file_count++] = arg;
        const char *posix = getenv("POSIXLY_CORRECT");
        if (posix != NULL && *posix != '\0') options_done = 1;
    }
    if (file_count == 0) { fputs("touch: missing file operand\nTry 'touch --help' for more information.\n", stderr); free(files); return 1; }
    if (selected_times != 0) {
        access_time = (selected_times & 1) != 0;
        modify_time = (selected_times & 2) != 0;
    }
    struct timespec parsed;
    int fixed_time = 0, explicit_time = 0;
    if (date_text != NULL && stamp_text != NULL) { fputs("touch: cannot specify times from more than one source\n", stderr); free(files); return 1; }
    if (date_text != NULL || stamp_text != NULL) {
        explicit_time = 1;
        if (date_text != NULL && strcmp(date_text, "now") == 0) fixed_time = 0;
        else {
            fixed_time = date_text != NULL ? zov_touch_parse_date(date_text, &parsed) : zov_touch_parse_stamp(stamp_text, &parsed);
            if (!fixed_time) { fprintf(stderr, "touch: invalid date format '%s'\n", date_text != NULL ? date_text : stamp_text); free(files); return 1; }
        }
    }
    struct stat reference_info;
    if (reference != NULL && !explicit_time && stat(reference, &reference_info) != 0) {
        zov_touch_error("failed to get attributes of", reference, errno); free(files); return 1;
    }
    int flags = no_dereference ? AT_SYMLINK_NOFOLLOW : 0;
    int status = 0;
    for (int i = 0; i < file_count; ++i) {
        const char *path = files[i];
        struct timespec times[2];
        if (reference != NULL && !explicit_time) zov_touch_get_stat_times(&reference_info, times);
        else if (fixed_time) times[0] = times[1] = parsed;
        else {
            times[0].tv_sec = times[1].tv_sec = 0;
            times[0].tv_nsec = times[1].tv_nsec = UTIME_NOW;
        }
        if (reference == NULL && !fixed_time) {
            if (!access_time) times[0].tv_nsec = UTIME_OMIT;
            if (!modify_time) times[1].tv_nsec = UTIME_OMIT;
        } else {
            if (!access_time) times[0].tv_nsec = UTIME_OMIT;
            if (!modify_time) times[1].tv_nsec = UTIME_OMIT;
        }
        if (strcmp(path, "-") == 0) {
            if (futimens(STDOUT_FILENO, times) != 0) {
                int error = errno;
#if !defined(__APPLE__)
                if (error == EINVAL) continue;
#endif
                if (error != EBADF) {
                    zov_touch_error("setting times of", path, error);
                    status = 1;
                }
            }
            continue;
        }
        if (!no_create && !no_dereference) {
            struct stat info;
            int stat_result = no_dereference ? lstat(path, &info) : stat(path, &info);
            if (stat_result != 0 && errno == ENOENT) {
                int fd = open(path, O_WRONLY | O_CREAT, 0666);
                if (fd >= 0) close(fd);
            }
        }
        if (utimensat(AT_FDCWD, path, times, flags) != 0) {
            int error = errno;
            if (no_create && error == ENOENT) continue;
            zov_touch_error("setting times of", path, error);
            status = 1;
        }
    }
    free(files);
    return status;
}

#endif
