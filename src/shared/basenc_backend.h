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
    int decode = 0, ignore = 0, wrap = 76, url = 0, mode = strcmp(program, "base32") == 0 ? 1 : 0;
    const char *file = NULL;
    for (int i=0;i<argc;i++) {
        const char *a=argv[i];
        if (!strcmp(a,"--help")) { fprintf(stdout,"Usage: %s [OPTION]... [FILE]\nBase-encode or decode FILE, or standard input, to standard output.\n  -d, --decode          decode data\n  -i, --ignore-garbage  when decoding, ignore non-alphabet characters\n  -w, --wrap=COLS       wrap encoded lines after COLS character (default 76)\n      --help            display this help and exit\n      --version         output version information and exit\n",program); return 0; }
        if (!strcmp(a,"--version")) { printf("%s (zovutils) 0.10.13\n",program); return 0; }
        if (!strcmp(a,"-d")||!strcmp(a,"--decode")) {decode=1;continue;}
        if (!strcmp(a,"-i")||!strcmp(a,"--ignore-garbage")) {ignore=1;continue;}
        if (!strcmp(program,"basenc")&&(!strcmp(a,"--base64url"))) {mode=0;url=1;continue;}
        if (!strcmp(program,"basenc")&&!strcmp(a,"--base32")) {mode=1;url=0;continue;}
        if (!strcmp(program,"basenc")&&!strcmp(a,"--base32hex")) {mode=2;url=0;continue;}
        if (!strcmp(program,"basenc")&&!strcmp(a,"--base16")) {mode=3;url=0;continue;}
        if (!strcmp(program,"basenc")&&!strcmp(a,"--base2msbf")) {mode=4;url=0;continue;}
        if (!strcmp(program,"basenc")&&!strcmp(a,"--base2lsbf")) {mode=5;url=0;continue;}
        if (!strcmp(program,"basenc")&&!strcmp(a,"--base58")) {mode=6;url=0;continue;}
        if (!strcmp(program,"basenc")&&!strcmp(a,"--z85")) {mode=7;url=0;continue;}
        if (!strcmp(program,"basenc")&&!strcmp(a,"--base64")) {mode=0;url=0;continue;}
        if (!strcmp(a,"-w")||!strncmp(a,"--wrap=",7)) { const char *v=!strcmp(a,"-w")?(i+1<argc?argv[++i]:NULL):a+7; if(!v){fprintf(stderr,"%s: option requires an argument -- 'w'\n",program);return 1;} char *e; long n=strtol(v,&e,10);if(*e||n<0){fprintf(stderr,"%s: invalid wrap size: %s\n",program,v);return 1;}wrap=(int)n;continue; }
        if (a[0]=='-' && strcmp(a,"-")) {fprintf(stderr,"%s: unrecognized option '%s'\nTry '%s --help' for more information.\n",program,a,program);return 1;}
        if(file){fprintf(stderr,"%s: extra operand '%s'\nTry '%s --help' for more information.\n",program,a,program);return 1;} file=a;
    }
    FILE *in=stdin; if(file&&strcmp(file,"-")){in=fopen(file,"rb");if(!in){fprintf(stderr,"%s: %s: %s\n",program,file,strerror(errno));return 1;}}
    size_t cap=8192,n=0; unsigned char *data=malloc(cap);if(!data){if(in!=stdin)fclose(in);return 1;}
    for(;;){if(n==cap){if(cap>SIZE_MAX/2){free(data);if(in!=stdin)fclose(in);return 1;}cap*=2;unsigned char *p=realloc(data,cap);if(!p){free(data);if(in!=stdin)fclose(in);return 1;}data=p;}size_t r=fread(data+n,1,cap-n,in);n+=r;if(!r){if(ferror(in)){fprintf(stderr,"%s: read error: %s\n",program,strerror(errno));free(data);if(in!=stdin)fclose(in);return 1;}break;}}
    if(in!=stdin)fclose(in);
    const char *alphabet=mode==1?"ABCDEFGHIJKLMNOPQRSTUVWXYZ234567":mode==2?"0123456789ABCDEFGHIJKLMNOPQRSTUV":mode==3?"0123456789ABCDEF":mode==6?"123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz":mode==7?"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-:+=^!/*?&<>()[]{}@%$#":(url?"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_":"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
    int status=0,col=0;
#define OUT(ch) do { if(wrap>0&&col>=wrap){putchar('\n');col=0;} putchar((ch));col++; } while(0)
    if(!decode){
        if(mode==3){for(size_t i=0;i<n;i++){OUT(alphabet[data[i]>>4]);OUT(alphabet[data[i]&15]);}if(n&&wrap)putchar('\n');}
        else if(mode==4||mode==5){for(size_t i=0;i<n;i++)for(unsigned b=0;b<8;b++)OUT(((unsigned)data[i]>>(mode==4?7-b:b))&1?'1':'0');if(n&&wrap)putchar('\n');}
        else if(mode==6){size_t zeros=0;while(zeros<n&&data[zeros]==0)zeros++;size_t cap=n*2+1;unsigned char*digits=(unsigned char*)calloc(cap,1);if(!digits){free(data);return 1;}size_t used=0;for(size_t i=zeros;i<n;i++){unsigned carry=data[i];size_t j=0;for(size_t k=cap;k>0&&(carry||j<used);k--,j++){carry+=256u*digits[k-1];digits[k-1]=(unsigned char)(carry%58u);carry/=58u;}used=j;}for(size_t i=0;i<zeros;i++)OUT('1');size_t first=cap-used;while(first<cap&&!digits[first])first++;for(size_t i=first;i<cap;i++)OUT(alphabet[digits[i]]);if((n>0||used>0)&&wrap)putchar('\n');free(digits);}
        else if(mode==7){if(n%4){fprintf(stderr,"%s: invalid input (length must be multiple of 4 characters)\n",program);status=1;}else{for(size_t i=0;i<n;i+=4){uint64_t v=((uint64_t)data[i]<<24)|((uint64_t)data[i+1]<<16)|((uint64_t)data[i+2]<<8)|data[i+3];char out[5];for(int j=4;j>=0;j--){out[j]=alphabet[v%85u];v/=85u;}for(int j=0;j<5;j++)OUT(out[j]);}if(n&&wrap)putchar('\n');}}
        else {unsigned width=mode==1||mode==2?5u:6u,bits=0,nb=0;for(size_t i=0;i<n;i++){bits=(bits<<8)|data[i];nb+=8;while(nb>=width){nb-=width;OUT(alphabet[(bits>>nb)&((1u<<width)-1)]);}}if(nb)OUT(alphabet[(bits<<(width-nb))&((1u<<width)-1)]);if(mode==1||mode==2){unsigned rem=(unsigned)(n%5);unsigned pad=rem==1?6:rem==2?4:rem==3?3:rem==4?1:0;while(pad--)OUT('=');}else{unsigned rem=(unsigned)(n%3);unsigned pad=rem==1?2:rem==2?1:0;while(pad--)OUT('=');}if(n&&wrap)putchar('\n');}
    }
    else if(mode==3){int high=-1;for(size_t i=0;i<n;i++){unsigned char c=data[i];int v=c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;if(v<0){if(c=='\n'||c=='\r'||c==' '||c=='\t')continue;if(ignore)continue;fprintf(stderr,"%s: invalid input\n",program);status=1;break;}if(high<0)high=v;else{putchar((high<<4)|v);high=-1;}}if(!status&&high>=0){fprintf(stderr,"%s: invalid input\n",program);status=1;}}
    else if(mode==4||mode==5){unsigned byte=0,nb=0;for(size_t i=0;i<n;i++){unsigned char c=data[i];if(c=='\n'||c=='\r'||c==' '||c=='\t')continue;if(c!='0'&&c!='1'){if(ignore)continue;fprintf(stderr,"%s: invalid input\n",program);status=1;break;}unsigned v=(unsigned)(c-'0');if(mode==4)byte=(byte<<1)|v;else byte|=v<<nb;nb++;if(nb==8){putchar((int)byte);byte=0;nb=0;}}if(!status&&nb){fprintf(stderr,"%s: invalid input\n",program);status=1;}}
    else if(mode==6){static const char*abc="123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";size_t zeros=0,pos=0;while(pos<n&&(data[pos]=='\n'||data[pos]=='\r'||data[pos]==' '||data[pos]=='\t'))pos++;while(pos<n&&data[pos]=='1'){zeros++;pos++;}size_t cap=n+1;unsigned char*bytes=(unsigned char*)calloc(cap,1);if(!bytes){free(data);return 1;}size_t used=0;for(;pos<n;pos++){unsigned char c=data[pos];if(c=='\n'||c=='\r'||c==' '||c=='\t')continue;const char*q=strchr(abc,c);if(!q){if(ignore)continue;fprintf(stderr,"%s: invalid input\n",program);status=1;break;}unsigned carry=(unsigned)(q-abc);size_t j=0;for(size_t k=cap;k>0&&(carry||j<used);k--,j++){carry+=58u*bytes[k-1];bytes[k-1]=(unsigned char)(carry&255u);carry>>=8;}used=j;}if(!status){for(size_t i=0;i<zeros;i++)putchar(0);size_t first=cap-used;while(first<cap&&!bytes[first])first++;for(size_t i=first;i<cap;i++)putchar(bytes[i]);}free(bytes);}
    else if(mode==7){static const char*abc="0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-:+=^!/*?&<>()[]{}@%$#";size_t len=0;for(size_t i=0;i<n;i++)if(data[i]!='\n'&&data[i]!='\r'&&data[i]!=' '&&data[i]!='\t')len++;if(len%5){fprintf(stderr,"%s: invalid input\n",program);status=1;}else{uint64_t value=0;unsigned count=0;for(size_t i=0;i<n;i++){unsigned char c=data[i];if(c=='\n'||c=='\r'||c==' '||c=='\t')continue;const char*q=strchr(abc,c);if(!q){if(ignore)continue;fprintf(stderr,"%s: invalid input\n",program);status=1;break;}value=value*85u+(uint64_t)(q-abc);if(++count==5){if(value>0xffffffffULL){fprintf(stderr,"%s: invalid input\n",program);status=1;break;}putchar((int)(value>>24));putchar((int)(value>>16));putchar((int)(value>>8));putchar((int)value);value=0;count=0;}}}}
    else {unsigned width=mode==1||mode==2?5u:6u,bits=0,nb=0;int ended=0;for(size_t i=0;i<n;i++){unsigned char c=data[i];if(c=='='){ended=1;continue;}int v=-1;if(mode==1){if(c>='A'&&c<='Z')v=c-'A';else if(c>='a'&&c<='z')v=c-'a';else if(c>='2'&&c<='7')v=c-'2'+26;}else if(mode==2){if(c>='0'&&c<='9')v=c-'0';else if(c>='A'&&c<='V')v=c-'A'+10;else if(c>='a'&&c<='v')v=c-'a'+10;}else v=zov_b64_value(c,url,ignore);if(v==-2||c=='\n'||c=='\r'||c=='\t'||c==' ')continue;if(v<0||ended){if(ignore)continue;fprintf(stderr,"%s: invalid input\n",program);status=1;break;}bits=(bits<<width)|(unsigned)v;nb+=width;if(nb>=8){nb-=8;putchar((bits>>nb)&255);}}}
#undef OUT
    if(ferror(stdout))status=1;free(data);return status;
}
#endif
