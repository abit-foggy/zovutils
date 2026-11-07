#ifndef ZOVUTILS_DIRCOLORS_BACKEND_H
#define ZOVUTILS_DIRCOLORS_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <fnmatch.h>

typedef struct { char *key; char *value; } zov_dc_entry;

static inline char *zov_dc_trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
    return s;
}

static inline const char *zov_dc_key(const char *s) {
    if (!strcmp(s,"NORMAL")) return "no";
    if (!strcmp(s,"FILE")) return "fi";
    if (!strcmp(s,"DIR")) return "di";
    if (!strcmp(s,"LINK")) return "ln";
    if (!strcmp(s,"MULTIHARDLINK")) return "mh";
    if (!strcmp(s,"FIFO")) return "pi";
    if (!strcmp(s,"SOCK")) return "so";
    if (!strcmp(s,"DOOR")) return "do";
    if (!strcmp(s,"BLK")) return "bd";
    if (!strcmp(s,"CHR")) return "cd";
    if (!strcmp(s,"ORPHAN")) return "or";
    if (!strcmp(s,"MISSING")) return "mi";
    if (!strcmp(s,"SETUID")) return "su";
    if (!strcmp(s,"SETGID")) return "sg";
    if (!strcmp(s,"CAPABILITY")) return "ca";
    if (!strcmp(s,"STICKY")) return "st";
    if (!strcmp(s,"OTHER_WRITABLE")) return "ow";
    if (!strcmp(s,"STICKY_OTHER_WRITABLE")) return "tw";
    if (!strcmp(s,"EXEC")) return "ex";
    if (s[0] == '*') return s;
    return NULL;
}

static inline int zov_dc_add(zov_dc_entry **items, size_t *count, size_t *capacity,
                              const char *key, const char *value) {
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 16;
        zov_dc_entry *grown = (zov_dc_entry *)realloc(*items, next * sizeof(**items));
        if (!grown) return 0;
        *items = grown;
        *capacity = next;
    }
    (*items)[*count].key = strdup(key);
    (*items)[*count].value = strdup(value);
    if (!(*items)[*count].key || !(*items)[*count].value) return 0;
    (*count)++;
    return 1;
}

static inline void zov_dc_free(zov_dc_entry *items, size_t count) {
    for (size_t i = 0; i < count; i++) { free(items[i].key); free(items[i].value); }
    free(items);
}

static inline int zov_dc_load(const char *file, zov_dc_entry **items, size_t *count,
                              int *term_seen, int *term_matches) {
    FILE *f = fopen(file, "r");
    if (!f) { fprintf(stderr, "dircolors: %s: %s\n", file, strerror(errno)); return 0; }
    size_t capacity = 0;
    char *line = NULL;
    size_t linecap = 0;
    ssize_t got;
    unsigned long lineno = 0;
    const char *term = getenv("TERM");
    while ((got = getline(&line, &linecap, f)) >= 0) {
        (void)got;
        lineno++;
        char *p = zov_dc_trim(line);
        if (!*p || *p == '#') continue;
        char *key = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        if (!*p) {
            fprintf(stderr, "dircolors: %s:%lu: invalid line;  missing second token\n", file, lineno);
            free(line); fclose(f); zov_dc_free(*items, *count); *items = NULL; *count = 0; return 0;
        }
        *p++ = 0;
        char *value = zov_dc_trim(p);
        if (!*value) {
            fprintf(stderr, "dircolors: %s:%lu: invalid line;  missing second token\n", file, lineno);
            free(line); fclose(f); zov_dc_free(*items, *count); *items = NULL; *count = 0; return 0;
        }
        if (!strcmp(key, "TERM")) {
            *term_seen = 1;
            if (term && fnmatch(value, term, 0) == 0) *term_matches = 1;
            continue;
        }
        if (!strcmp(key, "COLOR") || !strcmp(key, "OPTIONS") || !strcmp(key, "EIGHTBIT")) continue;
        const char *mapped = zov_dc_key(key);
        if (mapped && !zov_dc_add(items, count, &capacity, mapped, value)) {
            fprintf(stderr, "dircolors: memory exhausted\n");
            free(line); fclose(f); zov_dc_free(*items, *count); *items = NULL; *count = 0; return 0;
        }
    }
    free(line);
    if (ferror(f)) { fprintf(stderr, "dircolors: %s: read error\n", file); fclose(f); return 0; }
    fclose(f);
    return 1;
}

static inline int zov_dircolors_run(int argc, char **argv) {
    int shell = 0, shell_selected = 0, print_database = 0, print_colors = 0;
    const char *file = NULL;
    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--help")) {
            puts("Usage: dircolors [OPTION]... [FILE]\nOutput commands to set the LS_COLORS environment variable.\n  -b, --sh, --bourne-shell  output Bourne shell code\n  -c, --csh, --c-shell      output C shell code\n  -p, --print-database      output defaults\n      --print-ls-colors      output escaped colors for display\n      --help                 display this help and exit\n      --version              output version information and exit");
            return 0;
        }
        if (!strcmp(a, "--version")) { puts("dircolors (zovutils) 0.10.20"); return 0; }
        if (!strcmp(a, "-b") || !strcmp(a, "--sh") || !strcmp(a, "--bourne-shell")) { shell = 0; shell_selected = 1; continue; }
        if (!strcmp(a, "-c") || !strcmp(a, "--csh") || !strcmp(a, "--c-shell")) { shell = 1; shell_selected = 1; continue; }
        if (!strcmp(a, "-p") || !strcmp(a, "--print-database")) { print_database = 1; continue; }
        if (!strcmp(a, "--print-ls-colors")) { print_colors = 1; continue; }
        if (a[0] == '-' && strcmp(a, "-")) { fprintf(stderr, "dircolors: unrecognized option '%s'\nTry 'dircolors --help' for more information.\n", a); return 1; }
        if (file) { fprintf(stderr, "dircolors: extra operand '%s'\nTry 'dircolors --help' for more information.\n", a); return 1; }
        file = a;
    }
    if (print_colors && shell_selected) {
        fprintf(stderr, "dircolors: the options to output non shell syntax,\nand to select a shell syntax are mutually exclusive\nTry 'dircolors --help' for more information.\n");
        return 1;
    }
    if (print_database) {
        puts("# Configuration file for dircolors\n# zovutils default color database\nTERM xterm*\nNORMAL 00\nFILE 00\nDIR 01;34\nLINK 01;36\nFIFO 40;33\nSOCK 01;35\nBLK 40;33;01\nCHR 40;33;01\nORPHAN 40;31;01\nEXEC 01;32");
        return 0;
    }
    zov_dc_entry *items = NULL;
    size_t count = 0;
    int term_seen = 0, term_matches = 0;
    if (file && !zov_dc_load(file, &items, &count, &term_seen, &term_matches)) return 1;
    if (term_seen && !term_matches) { zov_dc_free(items, count); items = NULL; count = 0; }
    if (print_colors) {
        for (size_t i = 0; i < count; i++)
            printf("\033[%sm%s\t%s\033[0m\n", items[i].value, items[i].key, items[i].value);
        zov_dc_free(items, count);
        return 0;
    }
    size_t cap = 1;
    for (size_t i = 0; i < count; i++) cap += strlen(items[i].key) + strlen(items[i].value) + 2;
    char *colors = (char *)calloc(cap, 1);
    if (!colors) { zov_dc_free(items, count); return 1; }
    size_t used = 0;
    for (size_t i = 0; i < count; i++) {
        if (used) colors[used++] = ':';
        size_t k = strlen(items[i].key), v = strlen(items[i].value);
        memcpy(colors + used, items[i].key, k); used += k;
        colors[used++] = '=';
        memcpy(colors + used, items[i].value, v); used += v;
    }
    if (count) colors[used++] = ':';
    if (shell) printf("setenv LS_COLORS '%s'\n", colors);
    else printf("LS_COLORS='%s';\nexport LS_COLORS\n", colors);
    free(colors);
    zov_dc_free(items, count);
    return 0;
}
#endif
