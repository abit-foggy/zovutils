#ifndef ZOVUTILS_TR_BACKEND_H
#define ZOVUTILS_TR_BACKEND_H

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned char sequence[256];
    size_t length;
    unsigned char members[256];
} zov_tr_set;

static inline int zov_tr_append(zov_tr_set *set, unsigned value, size_t repeats) {
    while (repeats-- && set->length < sizeof(set->sequence)) {
        unsigned char c = (unsigned char)value;
        set->sequence[set->length++] = c;
        set->members[c] = 1;
    }
    return 0;
}

static inline int zov_tr_escape(const char *s, size_t n, size_t *i,
                                unsigned *value) {
    if (s[*i] != '\\') { *value = (unsigned char)s[*i]; return 1; }
    ++*i;
    if (*i >= n) { *value = '\\'; return 1; }
    unsigned char c = (unsigned char)s[*i];
    if (c == 'n') *value = '\n';
    else if (c == 't') *value = '\t';
    else if (c == 'r') *value = '\r';
    else if (c == 'b') *value = '\b';
    else if (c == 'f') *value = '\f';
    else if (c == 'v') *value = '\v';
    else if (c == 'a') *value = '\a';
    else if (c == '\\') *value = '\\';
    else if (c >= '0' && c <= '7') {
        unsigned number = 0;
        for (int k = 0; k < 3 && *i < n && s[*i] >= '0' && s[*i] <= '7'; ++k) {
            number = number * 8 + (unsigned)(s[*i] - '0');
            if (k < 2 && *i + 1 < n && s[*i + 1] >= '0' && s[*i + 1] <= '7') ++*i;
        }
        *value = number & 255u;
    } else if (c == 'x' && *i + 2 < n) {
        char hex[3] = {s[*i + 1], s[*i + 2], '\0'};
        char *end = NULL;
        unsigned long number = strtoul(hex, &end, 16);
        if (end == hex || *end != '\0') return 0;
        *value = (unsigned)(number & 255u);
        *i += 2;
    } else *value = c;
    return 1;
}

static inline int zov_tr_class_char(const char *name, unsigned char c) {
    if (strcmp(name, "upper") == 0) return isupper(c);
    if (strcmp(name, "lower") == 0) return islower(c);
    if (strcmp(name, "alpha") == 0) return isalpha(c);
    if (strcmp(name, "alnum") == 0) return isalnum(c);
    if (strcmp(name, "digit") == 0) return isdigit(c);
    if (strcmp(name, "xdigit") == 0) return isxdigit(c);
    if (strcmp(name, "space") == 0) return isspace(c);
    if (strcmp(name, "blank") == 0) return c == ' ' || c == '\t';
    if (strcmp(name, "cntrl") == 0) return iscntrl(c);
    if (strcmp(name, "print") == 0) return isprint(c);
    if (strcmp(name, "graph") == 0) return isgraph(c);
    if (strcmp(name, "punct") == 0) return ispunct(c);
    if (strcmp(name, "word") == 0) return isalnum(c) || c == '_';
    return -1;
}

static inline int zov_tr_expand(const char *text, zov_tr_set *set) {
    memset(set, 0, sizeof(*set));
    size_t n = strlen(text);
    for (size_t i = 0; i < n; ++i) {
        if (text[i] == '[') {
            size_t star = i + 1;
            while (star < n && text[star] != '*' && text[star] != ']') ++star;
            if (star < n && text[star] == '*') {
                size_t close = star + 1;
                while (close < n && text[close] != ']') ++close;
                if (close < n && star > i + 1) {
                    unsigned repeated = (unsigned char)text[i + 1];
                    size_t repeats = 256;
                    if (close > star + 1) {
                        char number[32];
                        size_t digits = close - star - 1;
                        if (digits >= sizeof(number)) return 0;
                        memcpy(number, text + star + 1, digits); number[digits] = '\0';
                        char *tail = NULL;
                        unsigned long parsed = strtoul(number, &tail, 0);
                        if (tail == number || *tail != '\0') return 0;
                        repeats = parsed > 256 ? 256 : (size_t)parsed;
                    }
                    zov_tr_append(set, repeated, repeats);
                    i = close;
                    continue;
                }
            }
        }
        if (text[i] == '[' && i + 2 < n && text[i + 1] == ':') {
            size_t end = i + 2;
            while (end + 1 < n && !(text[end] == ':' && text[end + 1] == ']')) ++end;
            if (end + 1 < n) {
                char name[32];
                size_t length = end - (i + 2);
                if (length == 0 || length >= sizeof(name)) return 0;
                memcpy(name, text + i + 2, length); name[length] = '\0';
                int known = zov_tr_class_char(name, 0);
                if (known < 0) return 0;
                for (unsigned c = 0; c < 256; ++c)
                    if (zov_tr_class_char(name, (unsigned char)c) > 0) zov_tr_append(set, c, 1);
                i = end + 1;
                continue;
            }
        }
        unsigned first = 0;
        if (!zov_tr_escape(text, n, &i, &first)) return 0;
        if (i + 1 < n && text[i + 1] == '-' && i + 2 < n) {
            size_t end_i = i + 2;
            unsigned last = 0;
            if (!zov_tr_escape(text, n, &end_i, &last)) return 0;
            if (last < first) return 0;
            for (unsigned c = first; c <= last; ++c) zov_tr_append(set, c, 1);
            i = end_i;
            continue;
        }
        if (text[i] == '[' && i + 2 < n && text[i + 1] == '*') {
            size_t end = i + 2;
            while (end < n && text[end] != ']') ++end;
            if (end < n) {
                size_t repeats = 0;
                if (end > i + 2) {
                    char number[32];
                    size_t length = end - (i + 2);
                    if (length >= sizeof(number)) return 0;
                    memcpy(number, text + i + 2, length); number[length] = '\0';
                    char *tail = NULL;
                    unsigned long parsed = strtoul(number, &tail, 10);
                    if (tail == number || *tail != '\0') return 0;
                    repeats = parsed > 256 ? 256 : (size_t)parsed;
                } else repeats = 256;
                zov_tr_append(set, first, repeats);
                i = end;
                continue;
            }
        }
        zov_tr_append(set, first, 1);
    }
    return 1;
}

static inline int zov_tr_run(const char *set1_text, const char *set2_text,
                             int has_set2, int complement, int delete_mode,
                             int squeeze, int truncate) {
    (void)setlocale(LC_CTYPE, "");
    zov_tr_set set1, set2;
    if (!zov_tr_expand(set1_text, &set1) ||
        (has_set2 && !zov_tr_expand(set2_text, &set2))) {
        fprintf(stderr, "tr: invalid character array\n");
        return 1;
    }
    if (!has_set2) memset(&set2, 0, sizeof(set2));
    if (truncate && has_set2 && !delete_mode && !squeeze && set2.length < set1.length) {
        set1.length = set2.length;
        memset(set1.members, 0, sizeof(set1.members));
        for (size_t i = 0; i < set1.length; ++i) set1.members[set1.sequence[i]] = 1;
    }
    size_t target_length = set1.length;
    if (complement) {
        target_length = 0;
        for (unsigned c = 0; c < 256; ++c) if (!set1.members[c]) ++target_length;
    }
    if (has_set2 && !delete_mode && set2.length > 0 && target_length > set2.length && !truncate) {
        unsigned char pad = set2.sequence[set2.length - 1];
        zov_tr_append(&set2, pad, target_length - set2.length);
    }
    unsigned char mapping[256];
    unsigned char mapped[256];
    memset(mapped, 0, sizeof(mapped));
    size_t set_index = 0;
    for (unsigned c = 0; c < 256; ++c) {
        int selected = complement ? !set1.members[c] : set1.members[c];
        if (!selected || mapped[c]) continue;
        unsigned char target = c;
        if (!delete_mode && has_set2 && set2.length > 0) {
            size_t index = 0;
            if (complement) {
                for (unsigned j = 0; j < c; ++j)
                    if (!set1.members[j]) ++index;
            } else {
                for (size_t j = 0; j < set1.length; ++j)
                    if (set1.sequence[j] == c) { index = j; break; }
            }
            if (index >= set2.length) index = set2.length - 1;
            target = set2.sequence[index];
        }
        mapping[c] = target;
        mapped[c] = 1;
        ++set_index;
    }
    unsigned char squeeze_set[256];
    if (has_set2 && squeeze) memcpy(squeeze_set, set2.members, sizeof(squeeze_set));
    else if (delete_mode && squeeze) memcpy(squeeze_set, set1.members, sizeof(squeeze_set));
    else memcpy(squeeze_set, set1.members, sizeof(squeeze_set));
    int previous_valid = 0;
    unsigned char previous = 0;
    int c;
    while ((c = fgetc(stdin)) != EOF) {
        unsigned char input = (unsigned char)c;
        int selected = complement ? !set1.members[input] : set1.members[input];
        if (delete_mode && selected) continue;
        unsigned char output = input;
        if (selected && has_set2 && !delete_mode && mapped[input]) output = mapping[input];
        if (squeeze && squeeze_set[output] && previous_valid && output == previous) continue;
        if (fputc(output, stdout) == EOF) return 1;
        previous = output;
        previous_valid = 1;
    }
    if (ferror(stdin) || fflush(stdout) != 0) return 1;
    return 0;
}

#endif
