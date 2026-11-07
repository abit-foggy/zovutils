#ifndef ZOVUTILS_PTX_BACKEND_H
#define ZOVUTILS_PTX_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <strings.h>
#include <unistd.h>
#include <regex.h>

typedef struct {
    char *word;
    char *left;
    char *right;
    char *reference;
    int sentence_offset;
} zov_ptx_entry;

static inline int zov_ptx_compare(const void *aa, const void *bb, int fold) {
    const zov_ptx_entry *a = (const zov_ptx_entry *)aa;
    const zov_ptx_entry *b = (const zov_ptx_entry *)bb;
    int cmp = fold ? strcasecmp(a->word, b->word) : strcmp(a->word, b->word);
    if (cmp) return cmp;
    cmp = strcmp(a->word, b->word);
    if (cmp) return cmp;
    return strcmp(a->reference, b->reference);
}

static int zov_ptx_fold_sort;
static int zov_ptx_qsort_compare(const void *a, const void *b) {
    return zov_ptx_compare(a, b, zov_ptx_fold_sort);
}

static inline char *zov_ptx_dup_range(const char *s, size_t n) {
    char *copy = (char *)malloc(n + 1);
    if (!copy) return NULL;
    memcpy(copy, s, n);
    copy[n] = 0;
    return copy;
}

static inline char *zov_ptx_trim_copy(const char *s, size_t n) {
    char *out=(char *)malloc(n+1); if(!out)return NULL;
    size_t used=0; int pending_space=0;
    for(size_t i=0;i<n;i++){
        if(isspace((unsigned char)s[i])){ if(used)pending_space=1; continue; }
        if(pending_space)out[used++]=' ';
        out[used++]=s[i]; pending_space=0;
    }
    out[used]=0; return out;
}

static inline int zov_ptx_is_break(unsigned char c, const unsigned char *breaks) {
    return isspace(c) || breaks[c];
}

static inline void zov_ptx_sentence_bounds(const char *line,size_t len,size_t pos,
                                            regex_t *re,size_t *start,size_t *end) {
    size_t cursor=0;*start=0;*end=len;
    while(cursor<len){regmatch_t m;if(regexec(re,line+cursor,1,&m,0)!=0)break;
        size_t a=cursor+(size_t)m.rm_so,b=cursor+(size_t)m.rm_eo;
        if(b<=a){if(a>=len)break;cursor=a+1;continue;}
        if(pos<b){*end=b;return;}
        *start=b;cursor=b;
    }
}

static inline int zov_ptx_add(zov_ptx_entry **entries, size_t *count, size_t *capacity,
                              const char *line, size_t line_len, size_t begin, size_t end,
                              const char *reference) {
    if (*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 32;
        zov_ptx_entry *grown = (zov_ptx_entry *)realloc(*entries, next * sizeof(**entries));
        if (!grown) return 0;
        *entries = grown;
        *capacity = next;
    }
    zov_ptx_entry *item = &(*entries)[(*count)++];
    item->word = zov_ptx_dup_range(line + begin, end - begin);
    item->left = zov_ptx_trim_copy(line, begin);
    item->right = zov_ptx_trim_copy(line + end, line_len - end);
    item->reference = strdup(reference);
    item->sentence_offset = 0;
    return item->word && item->left && item->right && item->reference;
}

static inline int zov_ptx_word_allowed(const char *word, char **list, size_t count,
                                       int is_only) {
    if (!list) return is_only ? 0 : 1;
    int found = 0;
    for (size_t i = 0; i < count; i++) if (!strcasecmp(word, list[i])) { found = 1; break; }
    return is_only ? found : !found;
}

static inline int zov_ptx_load_words(const char *path, char ***words, size_t *count) {
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "ptx: %s: %s\n", path, strerror(errno)); return 0; }
    char *line = NULL; size_t cap = 0; ssize_t n;
    while ((n = getline(&line, &cap, f)) >= 0) {
        char *p = line;
        while (*p) {
            while (*p && isspace((unsigned char)*p)) p++;
            char *b = p;
            while (*p && !isspace((unsigned char)*p)) p++;
            if (p == b) break;
            char **grown = (char **)realloc(*words, (*count + 1) * sizeof(char *));
            if (!grown) { free(line); fclose(f); return 0; }
            *words = grown;
            (*words)[*count] = zov_ptx_dup_range(b, (size_t)(p - b));
            if (!(*words)[*count]) { free(line); fclose(f); return 0; }
            (*count)++;
        }
    }
    free(line); fclose(f); return 1;
}

static inline void zov_ptx_free(zov_ptx_entry *entries, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(entries[i].word); free(entries[i].left); free(entries[i].right); free(entries[i].reference);
    }
    free(entries);
}

static inline int zov_ptx_run(int argc, char **argv) {
    int auto_ref = 0, right_ref = 0, fold = 0, refs_in = 0, traditional = 0;
    int width = 72, gap = 3, width_set = 0, typeset = 0, roff = 0, tex = 0;
    const char *break_file = NULL, *ignore_file = NULL, *only_file = NULL;
    const char *macro_name = "xx", *truncation_flag = "/", *word_pattern = NULL, *sentence_pattern = NULL;
    const char **files = (const char **)malloc((size_t)(argc + 1) * sizeof(char *));
    if (!files) return 1;
    int nf = 0;
    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--version")) { puts("ptx (zovutils) 0.10.30"); free(files); return 0; }
        if (!strcmp(a, "--help")) {
            puts("Usage: ptx [OPTION]... [INPUT]...\nOutput a permuted index, including context, of the words in the input files.\n  -A, --auto-reference       output automatically generated references\n  -G, --traditional          use traditional System V output\n  -b, --break-file=FILE      use word break characters from FILE\n  -f, --ignore-case          fold lower case to upper case for sorting\n  -g, --gap-size=NUMBER      gap size between output fields\n  -i, --ignore-file=FILE     read words to ignore from FILE\n  -o, --only-file=FILE       read words to include from FILE\n  -r, --references           first field of each line is a reference\n  -R, --right-side-refs      put references at the right\n  -t, --typeset-mode         use a default width of 100 columns\n  -w, --width=NUMBER         output width in columns\n      --help                 display this help and exit\n      --version              output version information and exit");
            free(files); return 0;
        }
        if (!strcmp(a,"-A") || !strcmp(a,"--auto-reference")) { auto_ref=1; continue; }
        if (!strcmp(a,"-R") || !strcmp(a,"--right-side-refs")) { right_ref=1; continue; }
        if (!strcmp(a,"-f") || !strcmp(a,"--ignore-case")) { fold=1; continue; }
        if (!strcmp(a,"-r") || !strcmp(a,"--references")) { refs_in=1; continue; }
        if (!strcmp(a,"-G") || !strcmp(a,"--traditional")) { traditional=1; continue; }
        if (!strcmp(a,"-O")) { roff=1; continue; }
        if (!strcmp(a,"-T")) { tex=1; continue; }
        if (!strcmp(a,"-t") || !strcmp(a,"--typeset-mode")) { typeset=1; if (!width_set) width=100; continue; }
        const char *value=NULL; char opt=0;
        if (!strcmp(a,"-g") || !strcmp(a,"-w") || !strcmp(a,"-b") || !strcmp(a,"-i") || !strcmp(a,"-o") || !strcmp(a,"-F") || !strcmp(a,"-M") || !strcmp(a,"-S") || !strcmp(a,"-W")) {
            opt=a[1]; if (i+1<argc) value=argv[++i];
        } else if (!strncmp(a,"--format=",9)) {
            if (!strcmp(a+9,"roff")) roff=1;
            else if (!strcmp(a+9,"tex")) tex=1;
            else { fprintf(stderr,"ptx: invalid format '%s'\n",a+9); free(files); return 1; }
            continue;
        } else if (!strncmp(a,"--flag-truncation=",18)) { opt='F'; value=a+18;
        } else if (!strncmp(a,"--macro-name=",13)) { opt='M'; value=a+13;
        } else if (!strncmp(a,"--sentence-regexp=",18)) { opt='S'; value=a+18;
        } else if (!strncmp(a,"--word-regexp=",14)) { opt='W'; value=a+14;
        } else if (!strncmp(a,"--gap-size=",11)) { opt='g'; value=a+11; }
        else if (!strncmp(a,"--width=",8)) { opt='w'; value=a+8; }
        else if (!strncmp(a,"--break-file=",13)) { opt='b'; value=a+13; }
        else if (!strncmp(a,"--ignore-file=",14)) { opt='i'; value=a+14; }
        else if (!strncmp(a,"--only-file=",12)) { opt='o'; value=a+12; }
        else if (!strcmp(a,"--gap-size") || !strcmp(a,"--width") || !strcmp(a,"--break-file") || !strcmp(a,"--ignore-file") || !strcmp(a,"--only-file") || !strcmp(a,"--flag-truncation") || !strcmp(a,"--macro-name") || !strcmp(a,"--sentence-regexp") || !strcmp(a,"--word-regexp")) {
            opt=!strcmp(a,"--gap-size")?'g':!strcmp(a,"--width")?'w':!strcmp(a,"--break-file")?'b':!strcmp(a,"--ignore-file")?'i':!strcmp(a,"--only-file")?'o':!strcmp(a,"--flag-truncation")?'F':!strcmp(a,"--macro-name")?'M':!strcmp(a,"--sentence-regexp")?'S':'W';
            if (i+1<argc) value=argv[++i];
        }
        if (opt) {
            if (!value) { fprintf(stderr,"ptx: option requires an argument -- '%c'\nTry 'ptx --help' for more information.\n",opt); free(files); return 1; }
            if (opt=='g' || opt=='w') {
                char *end=NULL; errno=0; long v=strtol(value,&end,10);
                if (errno || end==value || *end || v<0 || v>10000 || (opt=='w'&&v<1)) {
                    fprintf(stderr,"ptx: invalid number: '%s'\n",value); free(files); return 1;
                }
                if (opt=='g') gap=(int)v; else { width=(int)v; width_set=1; }
            } else if (opt=='b') break_file=value;
            else if (opt=='i') ignore_file=value;
            else if (opt=='o') only_file=value;
            else if (opt=='F') truncation_flag=value;
            else if (opt=='M') macro_name=value;
            else if (opt=='S') sentence_pattern=value;
            else if (opt=='W') word_pattern=value;
            continue;
        }
        if (a[0]=='-' && strcmp(a,"-")) { fprintf(stderr,"ptx: unrecognized option '%s'\nTry 'ptx --help' for more information.\n",a); free(files); return 1; }
        files[nf++]=a;
    }
    if (traditional) { auto_ref=0; roff=1; }
    if (typeset && !width_set) width=100;
    if (!nf) files[nf++]="-";
    unsigned char breaks[256]={0};
    if (!break_file) for (int c=0;c<256;c++) breaks[c]=(unsigned char)ispunct(c);
    if (break_file) {
        FILE *bf=fopen(break_file,"rb");
        if (!bf) { fprintf(stderr,"ptx: %s: %s\n",break_file,strerror(errno)); free(files); return 1; }
        int ch; while ((ch=fgetc(bf))!=EOF) breaks[(unsigned char)ch]=1;
        if (ferror(bf)) { fprintf(stderr,"ptx: %s: read error\n",break_file); fclose(bf); free(files); return 1; }
        fclose(bf);
    }
    char **ignored=NULL, **only=NULL; size_t ignored_n=0, only_n=0;
    if (ignore_file && !zov_ptx_load_words(ignore_file,&ignored,&ignored_n)) { free(files); return 1; }
    if (only_file && !zov_ptx_load_words(only_file,&only,&only_n)) { free(files); return 1; }
    regex_t word_re, sentence_re; int has_word_re=0,has_sentence_re=0;
    if(word_pattern){int rc=regcomp(&word_re,word_pattern,REG_EXTENDED);if(rc){fprintf(stderr,"ptx: invalid word regular expression\n");free(files);return 1;}has_word_re=1;}
    if(sentence_pattern){int rc=regcomp(&sentence_re,sentence_pattern,REG_EXTENDED);if(rc){fprintf(stderr,"ptx: invalid sentence regular expression\n");if(has_word_re)regfree(&word_re);free(files);return 1;}has_sentence_re=1;}
    zov_ptx_entry *entries=NULL; size_t used=0, capacity=0; int status=0;
    for (int fi=0;fi<nf;fi++) {
        FILE *in=!strcmp(files[fi],"-")?stdin:fopen(files[fi],"r");
        if (!in) { fprintf(stderr,"ptx: %s: %s\n",files[fi],strerror(errno)); status=1; continue; }
        char *line=NULL; size_t len=0, linecap=0;
        unsigned char chunk[8192]; size_t nread;
        while ((nread=fread(chunk,1,sizeof chunk,in))>0) {
            if (len+nread+1>linecap) {
                size_t next=linecap?linecap:8192;
                while(next<len+nread+1)next*=2;
                char *grown=(char *)realloc(line,next);
                if(!grown){status=1;break;} line=grown;linecap=next;
            }
            memcpy(line+len,chunk,nread);len+=nread;
        }
        if(ferror(in))status=1;
        if(!line){line=(char *)calloc(1,1);linecap=1;}
        if(line)line[len]=0;
        size_t p=0, line_start=0, first_begin=0, first_end=0;
        unsigned long lineno=1; int have_first=0; char *input_reference=NULL;
        while(line && p<len){
            if(line[p]=='\n'){lineno++;line_start=++p;have_first=0;free(input_reference);input_reference=NULL;continue;}
            size_t begin,finish;
            if(has_word_re){
                regmatch_t match;
                if(regexec(&word_re,line+p,1,&match,0)!=0)break;
                begin=p+(size_t)match.rm_so;finish=p+(size_t)match.rm_eo;
                if(finish<=begin){p=begin+1;continue;}
                p=finish;
            }else{
                if(isspace((unsigned char)line[p])){p++;continue;}
                begin=p;while(p<len&&!isspace((unsigned char)line[p]))p++;finish=p;
            }
            if(!have_first){have_first=1;first_begin=begin;first_end=finish;if(refs_in){input_reference=zov_ptx_dup_range(line+begin,finish-begin);continue;}}
            char *word=zov_ptx_dup_range(line+begin,finish-begin);
            if(!word){status=1;break;}
            int allowed=(!ignore_file||zov_ptx_word_allowed(word,ignored,ignored_n,0))&&
                        (!only_file||zov_ptx_word_allowed(word,only,only_n,1));
            free(word);if(!allowed)continue;
            char ref[PATH_MAX];
            if(refs_in&&!auto_ref&&input_reference)snprintf(ref,sizeof ref,"%s",input_reference);
            else if(!strcmp(files[fi],"-"))snprintf(ref,sizeof ref,":%lu",lineno);
            else snprintf(ref,sizeof ref,"%s:%lu",files[fi],lineno);
            size_t context_start=refs_in?first_end:0;
            size_t context_end=len;
            if(has_sentence_re)zov_ptx_sentence_bounds(line,len,begin,&sentence_re,&context_start,&context_end);
            if(refs_in&&context_start<first_end)context_start=first_end;
            if(!zov_ptx_add(&entries,&used,&capacity,line+context_start,context_end-context_start,
                            begin-context_start,finish-context_start,ref)){status=1;break;}
            entries[used-1].sentence_offset=has_sentence_re&&context_start>0;
            if(!has_word_re){
                for(size_t split=begin;split+1<finish;split++) if(breaks[(unsigned char)line[split]]){
                    char *suffix=zov_ptx_dup_range(line+split+1,finish-split-1);
                    int suffix_allowed=suffix&&(!ignore_file||zov_ptx_word_allowed(suffix,ignored,ignored_n,0))&&(!only_file||zov_ptx_word_allowed(suffix,only,only_n,1));
                    free(suffix);
                    if(has_sentence_re)zov_ptx_sentence_bounds(line,len,split+1,&sentence_re,&context_start,&context_end);
                    if(refs_in&&context_start<first_end)context_start=first_end;
                    if(suffix_allowed&&!zov_ptx_add(&entries,&used,&capacity,line+context_start,context_end-context_start,
                                                    split+1-context_start,finish-context_start,ref)){status=1;break;}
                    if(suffix_allowed)entries[used-1].sentence_offset=has_sentence_re&&context_start>0;
                }
            }
        }
        free(input_reference);free(line);if(in!=stdin)fclose(in);
    }
    zov_ptx_fold_sort=fold;
    if (used) qsort(entries,used,sizeof(*entries),zov_ptx_qsort_compare);
    for (size_t i=0;i<used;i++) {
        zov_ptx_entry *e=&entries[i];
        int target=width/2+(right_ref?0:gap);
        if (!right_ref && (auto_ref || refs_in))
            target=(width+(int)strlen(e->reference)+1)/2+1;
        char prefix[PATH_MAX+32]="";
        if (auto_ref && !right_ref) snprintf(prefix,sizeof prefix,"%s: ",e->reference);
        else if (refs_in && !auto_ref && !right_ref) snprintf(prefix,sizeof prefix,"%s ",e->reference);
        size_t pl=strlen(prefix), leftlen=strlen(e->left);
        if (roff || tex) {
            const char *ref=(auto_ref || refs_in)?e->reference:"";
            if (roff) {
                if (!leftlen) {
                    printf(".%s \"\" \"\" \"%s",macro_name,e->word);
                    if (*e->right) printf(" %s",e->right);
                    printf("\" \"\"");
                } else {
                    printf(".%s \"\" \"%s\" \"%s\" \"\"",macro_name,e->left,e->word);
                    if (*e->right) printf(" \"%s\"",e->right);
                }
                if (*ref) printf(" \"%s\"",ref);
                putchar('\n');
            } else {
                printf("\\%s {}{%s}{%s}{%s%s}{}{%s}\n",macro_name,e->left,e->word,
                       *e->right?" ":"",e->right,ref);
            }
            continue;
        }
        if (auto_ref && !right_ref && (int)pl > target) {
            fputs(prefix, stdout);
            if (leftlen) fputs(" /   ", stdout);
            else for (int j = 0; j < 4; j++) putchar(' ');
            fputs(e->word, stdout);
            if (*e->right) fputs(truncation_flag, stdout);
            putchar('\n');
            continue;
        }
        int before=leftlen?gap:0;
        if(e->sentence_offset&&!leftlen)target++;
        int spaces=target-(int)pl-(int)leftlen-before;
        fputs(prefix,stdout);
        for (int j=0;j<spaces;j++) putchar(' ');
        fputs(e->left,stdout);
        for (int j=0;j<before;j++) putchar(' ');
        fputs(e->word,stdout);
        if (*e->right) {
            int after=leftlen?(ispunct((unsigned char)e->left[leftlen-1])?1:gap):1;
            for (int j=0;j<after;j++) putchar(' ');
            fputs(e->right,stdout);
        }
        if (right_ref && (auto_ref || refs_in)) {
            int after=*e->right?(leftlen?gap:1):0;
            int usedcols=target+(int)strlen(e->word)+after+(int)strlen(e->right);
            int pad=width+gap-usedcols;
            for (int j=0;j<pad;j++) putchar(' ');
            fputs(e->reference,stdout);
        }
        putchar('\n');
    }
    for (size_t i=0;i<ignored_n;i++) free(ignored[i]); free(ignored);
    for (size_t i=0;i<only_n;i++) free(only[i]); free(only);
    zov_ptx_free(entries,used); free(files); (void)break_file; (void)truncation_flag;
    if(has_word_re)regfree(&word_re);if(has_sentence_re)regfree(&sentence_re);
    return status;
}
#endif
