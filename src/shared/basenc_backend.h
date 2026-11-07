#ifndef ZOVUTILS_BASENC_BACKEND_H
#define ZOVUTILS_BASENC_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

static inline int zov_b64_value(unsigned char c, int url, int ignore) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == (url ? '-' : '+')) return 62;
    if (c == (url ? '_' : '/')) return 63;
    if (c == '\n' || c == '\r' || c == '\t' || c == ' ' || (ignore && 0)) return -2;
    return ignore ? -2 : -1;
}
static inline int zov_basenc_run(const char *program, int argc, char **argv) {
    int decode = 0, ignore = 0, wrap = 76, url = 0, base32 = strcmp(program, "base32") == 0;
    const char *file = NULL;
    for (int i=0;i<argc;i++) {
        const char *a=argv[i];
        if (!strcmp(a,"--help")) { fprintf(stdout,"Usage: %s [OPTION]... [FILE]\nBase-encode or decode FILE, or standard input, to standard output.\n  -d, --decode          decode data\n  -i, --ignore-garbage  when decoding, ignore non-alphabet characters\n  -w, --wrap=COLS       wrap encoded lines after COLS character (default 76)\n      --help            display this help and exit\n      --version         output version information and exit\n",program); return 0; }
        if (!strcmp(a,"--version")) { printf("%s (zovutils) 0.10.1\n",program); return 0; }
        if (!strcmp(a,"-d")||!strcmp(a,"--decode")) {decode=1;continue;}
        if (!strcmp(a,"-i")||!strcmp(a,"--ignore-garbage")) {ignore=1;continue;}
        if (!strcmp(a,"-u")||!strcmp(a,"--base64url")) {url=1;continue;}
        if (!strcmp(a,"--base32")) {base32=1;continue;}
        if (!strcmp(a,"--base64")) {base32=0;url=0;continue;}
        if (!strcmp(a,"-w")||!strncmp(a,"--wrap=",7)) { const char *v=!strcmp(a,"-w")?(i+1<argc?argv[++i]:NULL):a+7; if(!v){fprintf(stderr,"%s: option requires an argument -- 'w'\n",program);return 1;} char *e; long n=strtol(v,&e,10);if(*e||n<0){fprintf(stderr,"%s: invalid wrap size: %s\n",program,v);return 1;}wrap=(int)n;continue; }
        if (a[0]=='-' && strcmp(a,"-")) {fprintf(stderr,"%s: unrecognized option '%s'\nTry '%s --help' for more information.\n",program,a,program);return 1;}
        if(file){fprintf(stderr,"%s: extra operand '%s'\nTry '%s --help' for more information.\n",program,a,program);return 1;} file=a;
    }
    FILE *in=stdin; if(file&&strcmp(file,"-")){in=fopen(file,"rb");if(!in){fprintf(stderr,"%s: %s: %s\n",program,file,strerror(errno));return 1;}}
    size_t cap=8192,n=0; unsigned char *data=malloc(cap);if(!data){if(in!=stdin)fclose(in);return 1;}
    for(;;){if(n==cap){if(cap>SIZE_MAX/2){free(data);if(in!=stdin)fclose(in);return 1;}cap*=2;unsigned char *p=realloc(data,cap);if(!p){free(data);if(in!=stdin)fclose(in);return 1;}data=p;}size_t r=fread(data+n,1,cap-n,in);n+=r;if(!r){if(ferror(in)){fprintf(stderr,"%s: read error: %s\n",program,strerror(errno));free(data);if(in!=stdin)fclose(in);return 1;}break;}}
    if(in!=stdin)fclose(in);
    const char *alphabet=base32?"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567":(url?"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_":"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
    int status=0,col=0;
#define OUT(ch) do { if(wrap>0&&col>=wrap){putchar('\n');col=0;} putchar((ch));col++; } while(0)
    if(!decode){ unsigned bits=0,nb=0; for(size_t i=0;i<n;i++){bits=(bits<<8)|data[i];nb+=8;while(nb>=(base32?5u:6u)){nb-=base32?5u:6u;OUT(alphabet[(bits>>nb)&(base32?31:63)]);}}if(nb){unsigned width=base32?5u:6u;OUT(alphabet[(bits<<(width-nb))&(base32?31:63)]);} if(base32){unsigned rem=(unsigned)(n%5);unsigned pad=rem==1?6:rem==2?4:rem==3?3:rem==4?1:0;while(pad--)OUT('=');}else{unsigned rem=(unsigned)(n%3);unsigned pad=rem==1?2:rem==2?1:0;while(pad--)OUT('=');} if(n>0&&wrap!=0)putchar('\n'); }
    else { unsigned bits=0,nb=0;int ended=0;for(size_t i=0;i<n;i++){unsigned char c=data[i];if(c=='='){ended=1;continue;}int v;if(base32){if(c>='A'&&c<='Z')v=c-'A';else if(c>='a'&&c<='z')v=c-'a';else if(c>='2'&&c<='7')v=c-'2'+26;else v=(c==' '||c=='\n'||c=='\r'||c=='\t'||ignore)?-2:-1;}else v=zov_b64_value(c,url,ignore);if(v==-2)continue;if(v<0){fprintf(stderr,"%s: invalid input\n",program);status=1;break;}unsigned width=base32?5u:6u;bits=(bits<<width)|(unsigned)v;nb+=width;if(nb>=8){nb-=8;putchar((bits>>nb)&255);}} }
#undef OUT
    if(ferror(stdout))status=1;free(data);return status;
}
#endif
