#ifndef ZOVUTILS_PLATFORM_QUOTE_H
#define ZOVUTILS_PLATFORM_QUOTE_H

#include <stddef.h>
#include <stdio.h>

static inline int zov_quote_shell_meta(unsigned char c) {
    return c == '!' || c == '"' || c == '$' || c == '&' || c == '(' ||
           c == ')' || c == '*' || c == ':' || c == ';' || c == '<' ||
           c == '=' || c == '>' || c == '?' || c == '[' || c == '\\' ||
           c == '^' || c == '`' || c == '|';
}

static inline void zov_quote_shell_segment(FILE *stream, const char *text, size_t length) {
    int apostrophe = 0, backslash = 0, special = 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == '\'') apostrophe = 1;
        if (c == '\\') backslash = 1;
        if (zov_quote_shell_meta(c)) special = 1;
    }
    if (apostrophe && !backslash && !special) {
        fputc('"', stream);
        fwrite(text, 1, length, stream);
        fputc('"', stream);
        return;
    }
    fputc('\'', stream);
    for (size_t i = 0; i < length; ++i) {
        if (text[i] == '\'') fputs("'\\''", stream);
        else fputc((unsigned char)text[i], stream);
    }
    fputc('\'', stream);
}

static inline void zov_quote_shell(FILE *stream, const char *text) {
    size_t length = 0;
    while (text[length] != '\0') ++length;
    size_t i = 0;
    while (i < length) {
        if ((unsigned char)text[i] >= 32 && (unsigned char)text[i] < 127) {
            size_t start = i++;
            while (i < length && (unsigned char)text[i] >= 32 && (unsigned char)text[i] < 127) ++i;
            zov_quote_shell_segment(stream, text + start, i - start);
            continue;
        }
        fputs("$'", stream);
        while (i < length && !((unsigned char)text[i] >= 32 && (unsigned char)text[i] < 127)) {
            unsigned char c = (unsigned char)text[i++];
            if (c == 7) fputs("\\a", stream);
            else if (c == 8) fputs("\\b", stream);
            else if (c == 9) fputs("\\t", stream);
            else if (c == 10) fputs("\\n", stream);
            else if (c == 11) fputs("\\v", stream);
            else if (c == 12) fputs("\\f", stream);
            else if (c == 13) fputs("\\r", stream);
            else fprintf(stream, "\\%03o", (unsigned)c);
        }
        fputc('\'', stream);
    }
}

#endif
