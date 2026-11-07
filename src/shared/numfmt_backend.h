#ifndef ZOVUTILS_NUMFMT_BACKEND_H
#define ZOVUTILS_NUMFMT_BACKEND_H
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline int zov_numfmt_unit(const char *s) { if (!strcmp(s,"auto")) return 1; if (!strcmp(s,"si")) return 2; if (!strcmp(s,"iec")) return 3; if (!strcmp(s,"iec-i")) return 4; if (!strcmp(s,"none")) return 0; return -1; }
static inline long double zov_numfmt_power(int base, int exponent) { long double v=1; while(exponent-->0) v*=base; return v; }
static inline int zov_numfmt_suffix(char c) { const char *p="kKmMgGtTpPeE"; const char *q=strchr(p,c); return q ? (int)(q-p)/2+1 : 0; }
static inline int zov_numfmt_parse(const char *text, int unit, long double *value, int *precision) {
    char *end; errno=0; long double v=strtold(text,&end); if(end==text || errno==ERANGE || !isfinite((double)v)) return 0;
    *precision=0; const char *dot=strchr(text,'.'); if(dot && dot<end) { const char *p=dot+1; while(p<end && isdigit((unsigned char)*p)) { ++*precision; ++p; } }
    if(*end) {
        if(unit==0 || (unit==2 && (end[1] || zov_numfmt_suffix(*end)==0)) || (unit==3 && (end[1] || zov_numfmt_suffix(*end)==0)) || (unit==4 && end[1] && !(end[1]=='i'||end[1]=='I')) || (unit==1 && (end[1] && !(end[1]=='i'||end[1]=='I')))) return 0;
        int exp=zov_numfmt_suffix(*end); if(!exp) return 0;
        int base=(unit==2)?1000:1024;
        if((unit==1||unit==4) && (end[1]=='i'||end[1]=='I')) base=1024;
        v*=zov_numfmt_power(base,exp);
    }
    *value=v; return 1;
}
static inline int zov_numfmt_one(const char *text, int from, int to, int from_unit, int to_unit, int round_mode, const char *suffix, const char *format, int padding, int zero_pad, int invalid_mode) {
    long double value; int precision;
    if(!zov_numfmt_parse(text,from,&value,&precision)) {
        if(invalid_mode!=3) fprintf(stderr,"numfmt: invalid number: '%s'\n",text);
        return invalid_mode==0 || invalid_mode==1 ? 2 : 0;
    }
    if(from_unit) value*=from_unit;
    if(to_unit) value/=(long double)to_unit;
    const char *units=" kMGTPE"; int exp=0; int base=to==2?1000:1024;
    if(to>0) while(fabsl(value)>=base && exp<6) { value/=base; ++exp; }
    if(to>0 && exp>0) {
        long double scale=10.0L; long double rounded;
        if(round_mode==1) rounded=ceill(value*scale)/scale;
        else if(round_mode==2) rounded=floorl(value*scale)/scale;
        else if(round_mode==3) rounded=truncl(value*scale)/scale;
        else rounded=roundl(value*scale)/scale;
        value=rounded; if(fabsl(value)>=base) { value/=base; ++exp; }
    }
    char out[256];
    if(format) { if(snprintf(out,sizeof(out),format,(double)value)<0) return 2; }
    else if(to>0 && exp>0) snprintf(out,sizeof(out),"%.1Lf%c%s",value,(exp==1&&to==2)?'k':(exp==1?'K':units[exp]),(to==4?"i":""));
    else if(precision>0) snprintf(out,sizeof(out),"%.*Lf",precision,value);
    else snprintf(out,sizeof(out),"%.0Lf",value);
    if(suffix) strncat(out,suffix,sizeof(out)-strlen(out)-1);
    size_t n=strlen(out); if(padding>0 && (size_t)padding>n) { for(size_t i=0;i<(size_t)padding-n;++i) putchar(zero_pad?'0':' '); }
    fputs(out,stdout); if(padding<0 && (size_t)(-padding)>n) for(size_t i=n;i<(size_t)(-padding);++i) putchar(' ');
    return ferror(stdout)?2:0;
}
static inline int zov_numfmt_run(int argc, int8_t **argv) {
    int from=0,to=0,fu=1,tu=1,zero=0,padding=0,round_mode=0,invalid=0,options=1; char *suffix=NULL,*format=NULL; int status=0, first=1;
    for(int i=0;i<argc;++i) {
        const char *a=(const char*)argv[i]; if(options&&!strcmp(a,"--")){options=0;continue;}
        if(options&&!strcmp(a,"--help")){puts("Usage: numfmt [OPTION]... [NUMBER]...\nReformat NUMBER(s), or the numbers from standard input if none are specified.\n\n      --from=UNIT       auto-scale input numbers to UNITs\n      --to=UNIT         auto-scale output numbers to UNITs\n      --from-unit=N     specify input unit size\n      --to-unit=N       specify output unit size\n      --round=METHOD    up, down, from-zero, towards-zero, nearest\n      --format=FORMAT   use printf style floating-point FORMAT\n      --padding=N       pad output to N characters\n      --suffix=SUFFIX   append SUFFIX to output\n      --invalid=MODE    abort, fail, warn, ignore\n  -z, --zero-terminated line delimiter is NUL\n      --help            display this help and exit\n      --version         output version information and exit\n\nReport bugs to: bug-coreutils@gnu.org\nGNU coreutils home page: <https://www.gnu.org/software/coreutils/>");return 0;}
        if(options&&!strcmp(a,"--version")){puts("numfmt (zovutils) 0.7.0");return 0;}
        if(options&&(!strcmp(a,"-z")||!strcmp(a,"--zero-terminated"))){zero=1;continue;}
        if(options&&!strncmp(a,"--",2)){
            const char *eq=strchr(a,'='); if(!eq){fprintf(stderr,"numfmt: unrecognized option '%s'\nTry 'numfmt --help' for more information.\n",a);return 1;}
            size_t k=(size_t)(eq-a); const char *v=eq+1;
            if(k==6&&!strncmp(a,"--from",6)){from=zov_numfmt_unit(v);if(from<0)goto badvalue;continue;}
            if(k==4&&!strncmp(a,"--to",4)){to=zov_numfmt_unit(v);if(to<0)goto badvalue;continue;}
            if(k==11&&!strncmp(a,"--from-unit",11)){fu=atoi(v);continue;}
            if(k==9&&!strncmp(a,"--to-unit",9)){tu=atoi(v);continue;}
            if(k==8&&!strncmp(a,"--suffix",8)){suffix=(char*)v;continue;}
            if(k==8&&!strncmp(a,"--format",8)){format=(char*)v;continue;}
            if(k==9&&!strncmp(a,"--padding",9)){padding=atoi(v);continue;}
            if(k==7&&!strncmp(a,"--round",7)){if(!strcmp(v,"up")||!strcmp(v,"from-zero"))round_mode=1;else if(!strcmp(v,"down"))round_mode=2;else if(!strcmp(v,"towards-zero"))round_mode=3;else if(strcmp(v,"nearest"))goto badvalue;continue;}
            if(k==9&&!strncmp(a,"--invalid",9)){if(!strcmp(v,"abort"))invalid=0;else if(!strcmp(v,"fail"))invalid=1;else if(!strcmp(v,"warn"))invalid=2;else if(!strcmp(v,"ignore"))invalid=3;else goto badvalue;continue;}
            fprintf(stderr,"numfmt: unrecognized option '%s'\nTry 'numfmt --help' for more information.\n",a);return 1;
        }
        if(options&&a[0]=='-'&&a[1]){fprintf(stderr,"numfmt: invalid option -- '%c'\nTry 'numfmt --help' for more information.\n",a[1]);return 1;}
        first=0; int r=zov_numfmt_one(a,from,to,fu,tu,round_mode,suffix,format,padding,0,invalid); if(!r || invalid>=2) putchar(zero?0:'\n'); if(r){status=r;if(invalid==0)break;}
    }
    if(first){char line[8192];size_t n=0;int c;while((c=getchar())!=EOF){if((zero&&c==0)||(!zero&&c=='\n')){line[n]=0;int r=zov_numfmt_one(line,from,to,fu,tu,round_mode,suffix,format,padding,0,invalid);if(r){status=r;if(invalid==0)break;}n=0;if(!r||invalid>=2)putchar(zero?0:'\n');}else if(n+1<sizeof(line))line[n++]=(char)c;}if(n){line[n]=0;int r=zov_numfmt_one(line,from,to,fu,tu,round_mode,suffix,format,padding,0,invalid);if(r)status=r;if(!r||invalid>=2)putchar(zero?0:'\n');}}
    return status;
badvalue: fprintf(stderr,"numfmt: invalid number: '%s'\nTry 'numfmt --help' for more information.\n",(const char*)argv[argc-1]);return 1;
}
#endif
