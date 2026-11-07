#ifndef ZOVUTILS_NUMFMT_BACKEND_H
#define ZOVUTILS_NUMFMT_BACKEND_H
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZOV_NUMFMT_LINE_MAX 65536

static inline int zov_numfmt_unit(const char *s, int allow_auto) {
    if (allow_auto && !strcmp(s, "auto")) return 1;
    if (!strcmp(s, "none")) return 0;
    if (!strcmp(s, "si")) return 2;
    if (!strcmp(s, "iec")) return 3;
    if (!strcmp(s, "iec-i")) return 4;
    return -1;
}
static inline int zov_numfmt_suffix_power(char c) {
    const char *letters = "kKmMgGtTpPeEzZyYrRqQ";
    const char *found = strchr(letters, c);
    return found ? (int)(found - letters) / 2 + 1 : 0;
}
static inline long double zov_numfmt_power(int base, int exponent) {
    long double value = 1;
    while (exponent-- > 0) value *= (long double)base;
    return value;
}
static inline void zov_numfmt_trim(const char **begin, const char **end) {
    while (*begin < *end && isspace((unsigned char)**begin)) ++*begin;
    while (*end > *begin && isspace((unsigned char)(*end)[-1])) --*end;
}

/* Returns 0 for malformed input, 2 for a suffix with no --from mode, and
   3 for a malformed suffix. */
static inline int zov_numfmt_parse(const char *text, int from,
                                    const char *accepted_suffix,
                                    const char *unit_separator,
                                    long double *value, int *precision) {
    const char *begin = text, *end = text + strlen(text);
    zov_numfmt_trim(&begin, &end);
    if (begin == end) return 0;
    char number[ZOV_NUMFMT_LINE_MAX]; size_t n = (size_t)(end - begin);
    if (n >= sizeof(number)) return 0;
    memcpy(number, begin, n); number[n] = '\0';
    if (accepted_suffix && *accepted_suffix) {
        size_t suffix_len = strlen(accepted_suffix), number_len = strlen(number);
        if (number_len >= suffix_len && !strcmp(number + number_len - suffix_len, accepted_suffix))
            number[number_len - suffix_len] = '\0';
    }
    const char *numeric_end=number, *scan=number;
    if(*scan=='-')++scan;
    int before=0,after=0,saw_dot=0;
    while(isdigit((unsigned char)*scan)){++before;++scan;}
    if(*scan=='.'){saw_dot=1;++scan;while(isdigit((unsigned char)*scan)){++after;++scan;}}
    if((!before&&!after)||(saw_dot&&!after))return 0;
    int decimals=after;
    char saved=*scan;* (char*)scan='\0';
    errno=0;long double parsed=strtold(number,NULL);
    *(char*)scan=saved;
    if(errno==ERANGE)return 0;
    const char *suffix=scan;
    if (unit_separator && *unit_separator && !strncmp(suffix, unit_separator, strlen(unit_separator))) suffix += strlen(unit_separator);
    if (*suffix) {
        int unit = from;
        int exponent = zov_numfmt_suffix_power(*suffix);
        int candidate = exponent && (*suffix == 'k' || *suffix == 'K' || (*suffix >= 'A' && *suffix <= 'Z'));
        if (!from) return candidate && (!suffix[1] || (suffix[1]=='i'||suffix[1]=='I')&&!suffix[2]) ? 2 : 3;
        if (!exponent || (*suffix>='a'&&*suffix<='z'&&*suffix!='k'&&from!=3&&from!=4)) return 3;
        if (unit == 4 && !suffix[1]) return 5;
        if (unit == 4 && (suffix[1]!='i'&&suffix[1]!='I')) return 5;
        if (unit != 4 && suffix[1]) {
            if(unit==1&&(suffix[1]=='i'||suffix[1]=='I')&&!suffix[2]){}
            else return (suffix[1]=='i'||suffix[1]=='I')?4:3;
        }
        int base = 1024;
        if (unit == 2) {
            if (suffix[1]) return 3;
            base = 1000;
        } else if (unit == 3) {
            if (suffix[1]) return 3;
        } else if (unit == 4) {
            if (!suffix[1] || (suffix[1] != 'i' && suffix[1] != 'I') || suffix[2]) return 3;
        } else if (unit == 1) {
            if (suffix[1] && ((suffix[1] != 'i' && suffix[1] != 'I') || suffix[2])) return 3;
            if (suffix[1]) base = 1024;
            else base = 1000;
        }
        parsed *= zov_numfmt_power(base, exponent);
    }
    *value = parsed; *precision = decimals;
    return 1;
}

static inline int zov_numfmt_format_valid(const char *format) {
    const char *percent = strchr(format, '%');
    if (!percent) return 0;
    if (strchr(percent + 1, '%')) return 0;
    const char *p = percent + 1;
    if (*p == '0') ++p;
    if (*p == '\'') ++p;
    if (*p == '-') ++p;
    while (isdigit((unsigned char)*p)) ++p;
    if (*p == '.') { ++p; while (isdigit((unsigned char)*p)) ++p; }
    if (*p != 'f' || p[1] != '\0') return 0;
    return 1;
}

static inline void zov_numfmt_bad_format(const char *format) {
    if (!strchr(format, '%')) fprintf(stderr, "numfmt: format '%s' has no %% directive\n", format);
    else if (strchr(strchr(format, '%') + 1, '%')) fprintf(stderr, "numfmt: format '%s' has too many %% directives\n", format);
    else fprintf(stderr, "numfmt: invalid format '%s', directive must be %%[0]['][-][N][.][N]f\n", format);
}

static inline long double zov_numfmt_round(long double x, int method, int precision) {
    long double scale = zov_numfmt_power(10, precision);
    long double y = x * scale;
    if (method == 1) y = ceill(y);             /* up */
    else if (method == 2) y = floorl(y);        /* down */
    else if (method == 3) y = x < 0 ? ceill(y) : floorl(y); /* towards zero */
    else if (method == 4) y = roundl(y);        /* nearest, ties away */
    else y = x < 0 ? floorl(y) : ceill(y);      /* from zero */
    return y / scale;
}

static inline int zov_numfmt_field_selected(const char *spec, unsigned long field) {
    if (!spec || !*spec || !field) return field == 1;
    if (!strcmp(spec, "-")) return 1;
    const char *p = spec;
    while (*p) {
        if (*p == '-' && (p[1] == ',' || p[1] == '\0')) return 1;
        unsigned long first = 0, last = 0; int have_first = 0, have_last = 0;
        if (*p == '-') { ++p; } else {
            if (!isdigit((unsigned char)*p)) return 0;
            have_first = 1;
            while (isdigit((unsigned char)*p)) { if (first > 100000000) return 0; first = first * 10 + (unsigned)(*p++ - '0'); }
        }
        if (*p == '-') {
            ++p;
            if (isdigit((unsigned char)*p)) {
                have_last = 1;
                while (isdigit((unsigned char)*p)) { if (last > 100000000) return 0; last = last * 10 + (unsigned)(*p++ - '0'); }
            }
        } else if (have_first) { last = first; have_last = 1; }
        if ((!have_first && !have_last) || (have_first && first == 0) || (have_last && last == 0) || (have_first && have_last && first > last)) return 0;
        if ((!have_first || field >= first) && (!have_last || field <= last)) return 1;
        if (*p == ',') ++p; else if (*p) return 0;
    }
    return 0;
}

static inline void zov_numfmt_diagnostic(const char *input, int parse_error, int invalid_mode) {
    if (invalid_mode == 3) return;
    if (parse_error == 2)
        fprintf(stderr, "numfmt: rejecting suffix in input: '%s' (consider using --from)\n", input);
    else if (parse_error == 4)
        fprintf(stderr, "numfmt: invalid suffix in input '%s': 'i'\n", input);
    else if (parse_error == 5)
        fprintf(stderr, "numfmt: missing 'i' suffix in input: '%s' (e.g Ki/Mi/Gi)\n", input);
    else if (parse_error == 3)
        fprintf(stderr, "numfmt: invalid suffix in input: '%s'\n", input);
    else
        fprintf(stderr, "numfmt: invalid number: '%s'\n", input);
}

static inline int zov_numfmt_convert(const char *input, int from, int to,
                                     long double from_unit, long double to_unit,
                                     int round_mode, const char *suffix,
                                     const char *unit_separator, const char *format,
                                     int precision_override, int invalid_mode,
                                     char *output, size_t output_size) {
    long double value; int precision = 0;
    int parsed = zov_numfmt_parse(input, from, suffix, unit_separator, &value, &precision);
    if (parsed != 1) return parsed;
    value *= from_unit;
    value /= to_unit;
    int exponent = 0, base = to == 2 ? 1000 : 1024;
    long double scaled = value;
    if (to > 0) {
        long double rounded_for_scale = zov_numfmt_round(value, round_mode, 0);
        while (exponent < 10 && fabsl(rounded_for_scale) >= base) {
            scaled /= base; rounded_for_scale /= base; ++exponent;
        }
    }
    if (exponent > 0) {
        scaled = zov_numfmt_round(scaled, round_mode, 1);
        if (fabsl(scaled) >= base) { scaled /= base; ++exponent; }
    }
    if (format) {
        if (snprintf(output, output_size, format, (double)scaled) >= (int)output_size) return 0;
    } else if (exponent > 0) {
        static const char si[] = " kMGTPEZYRQ";
        static const char iec[] = " KMGTPEZYRQ";
        char unit = to == 2 && exponent == 1 ? 'k' : (to == 2 ? si[exponent] : iec[exponent]);
        const char *sep = unit_separator ? unit_separator : "";
        const char *infix = to == 4 ? "i" : "";
        snprintf(output, output_size, "%.1Lf%s%c%s%s%s", scaled, sep, unit, infix, suffix ? suffix : "", "");
    } else if (to > 0) {
        snprintf(output, output_size, "%.0Lf%s", truncl(value), suffix ? suffix : "");
    } else {
        if (precision_override >= 0) precision = precision_override;
        if (precision > 18) precision = 18;
        snprintf(output, output_size, "%.*Lf%s", precision, scaled, suffix ? suffix : "");
    }
    return 1;
}

static inline void zov_numfmt_put_padded(const char *text, int padding) {
    size_t n = strlen(text), width = (size_t)(padding < 0 ? -(long long)padding : padding);
    if (padding > 0 && width > n) for (size_t i=0; i<width-n; ++i) putchar(' ');
    fputs(text, stdout);
    if (padding < 0 && width > n) for (size_t i=n; i<width; ++i) putchar(' ');
}

static inline void zov_numfmt_group(char *text, size_t capacity) {
    struct lconv *lc=localeconv();
    if(!lc||!lc->thousands_sep||!*lc->thousands_sep||!lc->grouping||!lc->grouping[0])return;
    char *point=strchr(text,lc->decimal_point&&*lc->decimal_point?lc->decimal_point[0]:'.');
    size_t end=point?(size_t)(point-text):strlen(text),start=(text[0]=='-'||text[0]=='+')?1:0;
    if(end<=start)return;
    size_t digits=end-start, separators=0, pos=digits; unsigned char gi=0; int group=(unsigned char)lc->grouping[0];
    while(group>0&&pos>(size_t)group){pos-=(size_t)group;++separators;if(lc->grouping[gi+1]==0){}else if((unsigned char)lc->grouping[gi+1]!=(unsigned char)CHAR_MAX){++gi;group=(unsigned char)lc->grouping[gi];}else break;}
    size_t sep_len=strlen(lc->thousands_sep),newlen=strlen(text)+separators*sep_len;
    if(newlen+1>capacity)return;
    char *out=(char*)malloc(newlen+1);if(!out)return;
    size_t w=0, next_group=(unsigned char)lc->grouping[0], group_at=0;unsigned char gidx=0;
    for(size_t i=0;i<strlen(text);++i){
        size_t passed=i-start;
        if(i>=start&&i<end&&passed>0&&digits-passed==group_at+next_group){memcpy(out+w,lc->thousands_sep,sep_len);w+=sep_len;group_at+=next_group;if(lc->grouping[gidx+1]==0){}else if((unsigned char)lc->grouping[gidx+1]!=(unsigned char)CHAR_MAX){++gidx;next_group=(unsigned char)lc->grouping[gidx];}else next_group=0;}
        out[w++]=text[i];
    }
    out[w]=0;strcpy(text,out);free(out);
}

static inline int zov_numfmt_convert_line(const char *line, int from, int to,
                                          long double from_unit, long double to_unit,
                                          int round_mode, const char *suffix,
                                          const char *unit_separator, const char *format,
                                          int precision_override, int invalid_mode,
                                          const char *field_spec, int delimiter,
                                          int padding, int padding_set, int grouping, int zero_terminated) {
    size_t length = strlen(line), begin = 0; unsigned long field = 0; int status = 0;
    int auto_align = !delimiter && !padding_set && strpbrk(line, " \t\r\v\f") != NULL;
    if (delimiter) {
        for (size_t end=0; end<=length; ++end) {
            if (end<length && (unsigned char)line[end]!=(unsigned char)delimiter) continue;
            if (field) putchar(delimiter);
            ++field;
            if (zov_numfmt_field_selected(field_spec, field)) {
                size_t n=end-begin; char *token=(char*)malloc(n+1);
                if(!token) return 2;
                memcpy(token,line+begin,n); token[n]='\0';
                char *converted=(char*)malloc(n+ZOV_NUMFMT_LINE_MAX);
                if(!converted){free(token);return 2;}
                int r=zov_numfmt_convert(token,from,to,from_unit,to_unit,round_mode,suffix,unit_separator,format,precision_override,invalid_mode,converted,n+ZOV_NUMFMT_LINE_MAX);
                if(r!=1){zov_numfmt_diagnostic(token,r,invalid_mode);status=invalid_mode==0||invalid_mode==1?2:0;if(invalid_mode==0){free(converted);free(token);return 2;}fputs(token,stdout);}
                else {if(grouping)zov_numfmt_group(converted,n+ZOV_NUMFMT_LINE_MAX);zov_numfmt_put_padded(converted,padding);}
                free(converted);free(token);
            } else fwrite(line+begin,1,end-begin,stdout);
            begin=end+1;
        }
        putchar(zero_terminated?0:'\n');
        return status;
    }
    while (begin < length) {
        size_t end = begin;
        int is_separator = delimiter ? (unsigned char)line[begin] == (unsigned char)delimiter : isspace((unsigned char)line[begin]);
        while (end < length && (delimiter ? (unsigned char)line[end] == (unsigned char)delimiter : isspace((unsigned char)line[end])) == is_separator) ++end;
        if (!is_separator) {
            ++field;
            if (zov_numfmt_field_selected(field_spec, field)) {
                char token[ZOV_NUMFMT_LINE_MAX], converted[ZOV_NUMFMT_LINE_MAX];
                size_t n = end - begin; memcpy(token, line + begin, n); token[n] = '\0';
                int r = zov_numfmt_convert(token, from, to, from_unit, to_unit, round_mode, suffix, unit_separator, format, precision_override, invalid_mode, converted, sizeof(converted));
                if (r != 1) { zov_numfmt_diagnostic(token, r, invalid_mode); status = invalid_mode == 0 || invalid_mode == 1 ? 2 : 0; if (invalid_mode == 0) return 2; fputs(token, stdout); }
                else {if(grouping)zov_numfmt_group(converted,sizeof(converted));zov_numfmt_put_padded(converted, auto_align ? (int)n : padding);}
            } else fwrite(line + begin, 1, end - begin, stdout);
        } else fwrite(line + begin, 1, end - begin, stdout);
        begin = end;
    }
    if (field == 0 && length == 0 && field_spec && zov_numfmt_field_selected(field_spec, 1)) {
        int r = zov_numfmt_convert(line, from, to, from_unit, to_unit, round_mode, suffix, unit_separator, format, precision_override, invalid_mode, (char[ZOV_NUMFMT_LINE_MAX]){0}, ZOV_NUMFMT_LINE_MAX);
        if (r != 1) { zov_numfmt_diagnostic(line, r, invalid_mode); return invalid_mode == 0 || invalid_mode == 1 ? 2 : 0; }
    }
    putchar(zero_terminated ? 0 : '\n');
    return status;
}

static inline int zov_numfmt_parse_int(const char *text, long *value) {
    char *end; errno = 0; long n = strtol(text, &end, 10);
    if (end == text || *end || errno == ERANGE) return 0;
    *value = n; return 1;
}
static inline int zov_numfmt_parse_field_spec(const char *spec) {
    if (!*spec) return 0;
    if (!strcmp(spec, "-")) return 1;
    for (unsigned long i=1; i<1000; ++i) (void)zov_numfmt_field_selected(spec, i);
    /* Validate the complete grammar independently of the largest selected index. */
    const char *p = spec;
    while (*p) {
        int have_a = 0, have_b = 0; unsigned long a=0,b=0;
        if (*p != '-') { if (!isdigit((unsigned char)*p)) return 0; have_a=1; while(isdigit((unsigned char)*p)){if(a>100000000)return 0;a=a*10+(unsigned)(*p++-'0');} }
        if (*p == '-') { ++p; if (isdigit((unsigned char)*p)){have_b=1;while(isdigit((unsigned char)*p)){if(b>100000000)return 0;b=b*10+(unsigned)(*p++-'0');}} }
        else if(have_a){b=a;have_b=1;}
        if ((!have_a&&!have_b) && (*p=='\0'||*p==',')) { if (*p==',') {++p;if(!*p)return 0;continue;} return 1; }
        if ((have_a&&!a)||(have_b&&!b)||(have_a&&have_b&&a>b)) return 0;
        if (*p == ',') { ++p; if (!*p) return 0; } else if (*p) return 0;
    }
    return 1;
}

static inline void zov_numfmt_help(void) {
    fputs("Usage: numfmt [OPTION]... [NUMBER]...\n"
          "Reformat NUMBER(s), or the numbers from standard input if none are specified.\n\n"
          "Mandatory arguments to long options are mandatory for short options too.\n"
          "      --debug\n         print warnings about invalid input\n"
          "  -d, --delimiter=X\n         use X instead of whitespace for field delimiter\n"
          "      --field=FIELDS\n         replace the numbers in these input fields (default=1);\n         see FIELDS below for details\n"
          "      --format=FORMAT\n         use printf style floating-point FORMAT;\n         see FORMAT below for details\n"
          "      --from=UNIT\n         auto-scale input numbers to UNITs; default is 'none';\n         see UNIT below for details\n"
          "      --from-unit=N\n         specify the input unit size (instead of the default 1)\n"
          "      --grouping\n         use locale-defined grouping of digits, e.g. 1,000,000.\n         This has no effect in the C/POSIX locale\n"
          "      --header[=N]\n         print (without converting) the first N header lines;\n         N defaults to 1 if not specified\n"
          "      --invalid=MODE\n         failure mode for invalid numbers;\n         MODE can be: abort (default), fail, warn, ignore\n"
          "      --padding=N\n         pad the output to N characters;\n         positive N will right-align, negative N will left-align;\n         padding is ignored if the output is wider than N;\n         the default is to automatically pad if a whitespace is found\n"
          "      --round=METHOD\n         use METHOD for rounding when scaling; METHOD can be:\n         up, down, from-zero (default), towards-zero, nearest\n"
          "      --suffix=SUFFIX\n         add SUFFIX to output numbers,\n         and accept an optional SUFFIX in input numbers\n"
          "      --unit-separator=SEP\n         insert SEP between number and unit on output,\n         and accept an optional SEP in input numbers\n"
          "      --to=UNIT\n         auto-scale output numbers to UNITs; see UNIT below\n"
          "      --to-unit=N\n         the output unit size (instead of the default 1)\n"
          "  -z, --zero-terminated\n         line delimiter is NUL, not newline\n"
          "      --help\n         display this help and exit\n"
          "      --version\n         output version information and exit\n\n"
          "UNIT options:\n"
          "  none       no auto-scaling is done; suffixes will trigger an error\n"
          "  auto       accept optional single/two letter suffix:\n               1K = 1000, 1k = 1000,\n               1Ki = 1024,\n               1M = 1000000,\n               1Mi = 1048576,\n"
          "  si         accept optional single letter suffix:\n               1k = 1000, 1K = 1000,\n               1M = 1000000,\n               ...\n"
          "  iec        accept optional single letter suffix:\n               1K = 1024, 1k = 1024,\n               1M = 1048576,\n               ...\n"
          "  iec-i      accept optional two-letter suffix:\n               1Ki = 1024, 1ki = 1024,\n               1Mi = 1048576,\n               ...\n\n"
          "FIELDS supports cut(1) style field ranges:\n"
          "  N    N'th field, counted from 1\n  N-   from N'th field, to end of line\n"
          "  N-M  from N'th to M'th field (inclusive)\n  -M   from first to M'th field (inclusive)\n"
          "  -    all fields\nMultiple fields/ranges can be separated with commas\n\n"
          "FORMAT must be suitable for printing one floating-point argument '%f'.\n"
          "Optional quote (%'f) will enable --grouping (if supported by current locale).\n"
          "Optional width value (%10f) will pad output. Optional zero (%010f) width\n"
          "will zero pad the number. Optional negative values (%-10f) will left align.\n"
          "Optional precision (%.1f) will override the input determined precision.\n\n"
          "Exit status is 0 if all input numbers were successfully converted.\n"
          "By default, numfmt will stop at the first conversion error with exit status 2.\n"
          "With --invalid='fail' a warning is printed for each conversion error\n"
          "and the exit status is 2.  With --invalid='warn' each conversion error is\n"
          "diagnosed, but the exit status is 0.  With --invalid='ignore' conversion\n"
          "errors are not diagnosed and the exit status is 0.\n\n"
          "Examples:\n  $ numfmt --to=si 1000\n            -> \"1.0k\"\n"
          "  $ numfmt --to=iec 2048\n           -> \"2.0K\"\n"
          "  $ numfmt --to=iec-i 4096\n           -> \"4.0Ki\"\n"
          "  $ echo 1K | numfmt --from=si\n           -> \"1000\"\n"
          "  $ echo 1K | numfmt --from=iec\n           -> \"1024\"\n"
          "  $ df -B1 | numfmt --header --field 2-4 --to=si\n"
          "  $ ls -l  | numfmt --header --field 5 --to=iec\n"
          "  $ ls -lh | numfmt --header --field 5 --from=iec --padding=10\n"
          "  $ ls -lh | numfmt --header --field 5 --from=iec --format %10f\n\n"
          "Report bugs to: bug-coreutils@gnu.org\n"
          "GNU coreutils home page: <https://www.gnu.org/software/coreutils/>\n"
          "General help using GNU software: <https://www.gnu.org/gethelp/>\n"
          "Report any translation bugs to <https://translationproject.org/team/>\n"
          "Full documentation <https://www.gnu.org/software/coreutils/numfmt>\n"
          "or available locally via: info '(coreutils) numfmt invocation'\n", stdout);
}

static inline int zov_numfmt_run(int argc, int8_t **argv) {
    int from=0,to=0,zero=0,round_mode=0,invalid=0,options=1,delimiter=0,header=0,precision=-1,debug=0,grouping=0,padding_set=0;
    long padding=0; long double from_unit=1,to_unit=1;
    const char *suffix=NULL,*unit_separator=NULL,*format=NULL,*field_spec=NULL;
    const char *operands[4096]; int noperands=0;
    for(int i=0;i<argc;++i) {
        const char *a=(const char*)argv[i];
        if(options&&!strcmp(a,"--")){options=0;continue;}
        if(options&&!strcmp(a,"--help")){zov_numfmt_help();return 0;}
        if(options&&!strcmp(a,"--version")){puts("numfmt (zovutils) 0.10.37");return 0;}
        if(options&&(!strcmp(a,"-z")||!strcmp(a,"--zero-terminated"))){zero=1;continue;}
        if(options&&!strncmp(a,"--",2)) {
            const char *eq=strchr(a,'='); size_t keylen=eq?(size_t)(eq-a):strlen(a); const char *v=eq?eq+1:NULL;
#define NFKEY(x) (keylen==strlen(x)&&!strncmp(a,x,keylen))
            if(NFKEY("--debug")&&!eq){debug=1;continue;}
            if(NFKEY("--grouping")&&!eq){grouping=1;continue;}
            if(NFKEY("--header")) { if(v){long n;if(!zov_numfmt_parse_int(v,&n)||n<0)goto badvalue;header=n;}else header=1;continue; }
            if(NFKEY("--from")||NFKEY("--to")) { if(!v&&i+1<argc)v=(const char*)argv[++i]; if(!v) goto needarg; int is_from=NFKEY("--from"); int unit=zov_numfmt_unit(v,is_from); if(unit<0||(!is_from&&unit==1))goto unitvalue; if(is_from)from=unit;else to=unit;continue; }
            if(NFKEY("--from-unit")||NFKEY("--to-unit")) { if(!v&&i+1<argc)v=(const char*)argv[++i]; if(!v)goto needarg; char *end; errno=0; long x=strtol(v,&end,10); if(*end||end==v||errno==ERANGE||x<=0)goto unitinvalid; if(NFKEY("--from-unit"))from_unit=(long double)x;else to_unit=(long double)x;continue; }
            if(NFKEY("--delimiter")||NFKEY("--unit-separator")||NFKEY("--suffix")||NFKEY("--format")||NFKEY("--padding")||NFKEY("--round")||NFKEY("--invalid")||NFKEY("--field")) {
                if(!v && i+1<argc) v=(const char*)argv[++i];
                if(!v)goto needarg;
                if(NFKEY("--delimiter")){if(strlen(v)>1){fputs("numfmt: the delimiter must be a single character\n",stderr);return 1;}delimiter=(unsigned char)v[0];}
                else if(NFKEY("--unit-separator"))unit_separator=v;
                else if(NFKEY("--suffix"))suffix=v;
                else if(NFKEY("--format")){if(!zov_numfmt_format_valid(v)){zov_numfmt_bad_format(v);return 1;}format=v;}
                else if(NFKEY("--padding")){long n;if(!zov_numfmt_parse_int(v,&n))goto badvalue;padding=n;padding_set=1;}
                else if(NFKEY("--round")){if(!strcmp(v,"up"))round_mode=1;else if(!strcmp(v,"down"))round_mode=2;else if(!strcmp(v,"towards-zero"))round_mode=3;else if(!strcmp(v,"nearest"))round_mode=4;else if(!strcmp(v,"from-zero"))round_mode=0;else {fprintf(stderr,"numfmt: invalid argument '%s' for '--round'\nValid arguments are:\n  - 'up'\n  - 'down'\n  - 'from-zero'\n  - 'towards-zero'\n  - 'nearest'\nTry 'numfmt --help' for more information.\n",v);return 1;}}
                else if(NFKEY("--invalid")){if(!strcmp(v,"abort"))invalid=0;else if(!strcmp(v,"fail"))invalid=1;else if(!strcmp(v,"warn"))invalid=2;else if(!strcmp(v,"ignore"))invalid=3;else {fprintf(stderr,"numfmt: invalid argument '%s' for '--invalid'\nValid arguments are:\n  - 'abort'\n  - 'fail'\n  - 'warn'\n  - 'ignore'\nTry 'numfmt --help' for more information.\n",v);return 1;}}
                else {if(!zov_numfmt_parse_field_spec(v))goto fieldvalue;field_spec=v;}
                continue;
            }
            if(!eq&&(NFKEY("--debug")||NFKEY("--grouping")))continue;
            if(!eq){fprintf(stderr,"numfmt: unrecognized option '%s'\nTry 'numfmt --help' for more information.\n",a);return 1;}
            fprintf(stderr,"numfmt: unrecognized option '%s'\nTry 'numfmt --help' for more information.\n",a);return 1;
needarg: fprintf(stderr,"numfmt: option '%s' requires an argument\nTry 'numfmt --help' for more information.\n",a);return 1;
unitvalue: fprintf(stderr,NFKEY("--from")?"numfmt: invalid argument '%s' for '--from'\nValid arguments are:\n  - 'none'\n  - 'auto'\n  - 'si'\n  - 'iec'\n  - 'iec-i'\nTry 'numfmt --help' for more information.\n":"numfmt: invalid argument '%s' for '--to'\nValid arguments are:\n  - 'none'\n  - 'si'\n  - 'iec'\n  - 'iec-i'\nTry 'numfmt --help' for more information.\n",v);return 1;
unitinvalid: fprintf(stderr,"numfmt: invalid unit size: '%s'\n",v);return 1;
fieldvalue: fprintf(stderr,"numfmt: invalid field value '%s'\nTry 'numfmt --help' for more information.\n",v);return 1;
badvalue: fprintf(stderr,"numfmt: invalid number: '%s'\nTry 'numfmt --help' for more information.\n",v);return 1;
#undef NFKEY
        }
        if(options&&a[0]=='-'&&a[1]){
            if(a[1]=='d'&&a[2]){if(a[3])goto shortbad;delimiter=(unsigned char)a[2];continue;}
            if(a[1]=='d'&&i+1<argc){const char *v=(const char*)argv[++i];if(strlen(v)!=1)goto shortbad;delimiter=(unsigned char)v[0];continue;}
            fprintf(stderr,"numfmt: invalid option -- '%c'\nTry 'numfmt --help' for more information.\n",a[1]);return 1;
shortbad: fprintf(stderr,"numfmt: invalid delimiter: '%s'\nTry 'numfmt --help' for more information.\n",a);return 1;
        }
        if(noperands>=4096){fputs("numfmt: too many operands\n",stderr);return 1;}
        operands[noperands++]=a;
    }
    if(grouping && to>0){fputs("numfmt: grouping cannot be combined with --to\n",stderr);return 1;}
    if(grouping || (format&&strchr(format,'\''))) { setlocale(LC_ALL, ""); grouping=1; }
    if(format && !zov_numfmt_format_valid(format)) return 1;
    int status=0;
    if(noperands) {
        for(int i=0;i<noperands;++i){
            int r=zov_numfmt_convert_line(operands[i],from,to,from_unit,to_unit,round_mode,suffix,unit_separator,format,precision,invalid,field_spec,delimiter,padding,padding_set,grouping,zero);
            if(r==2){status=2;if(invalid==0)break;} else if(r)status=r;
        }
    } else {
        char line[ZOV_NUMFMT_LINE_MAX]; size_t n=0; int ch; long line_number=0;
        while((ch=getchar())!=EOF){if((zero&&ch==0)||(!zero&&ch=='\n')){
                line[n]='\0'; ++line_number;
                if(line_number<=header){fwrite(line,1,n,stdout);putchar(zero?0:'\n');}
                else {int r=zov_numfmt_convert_line(line,from,to,from_unit,to_unit,round_mode,suffix,unit_separator,format,precision,invalid,field_spec,delimiter,padding,padding_set,grouping,zero);if(r==2){status=2;if(invalid==0)break;}else if(r)status=r;}
                n=0;
            } else if(n+1<sizeof(line)) line[n++]=(char)ch;
        }
        if(n&&! (status==2&&invalid==0)){line[n]='\0';++line_number;if(line_number<=header){fwrite(line,1,n,stdout);putchar(zero?0:'\n');}else{int r=zov_numfmt_convert_line(line,from,to,from_unit,to_unit,round_mode,suffix,unit_separator,format,precision,invalid,field_spec,delimiter,padding,padding_set,grouping,zero);if(r==2)status=2;else if(r)status=r;}}
    }
    (void)debug;
    return ferror(stdout)?2:status;
}
#endif
