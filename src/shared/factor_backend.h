#ifndef ZOVUTILS_FACTOR_BACKEND_H
#define ZOVUTILS_FACTOR_BACKEND_H
#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline uint64_t zov_factor_addmod(uint64_t a, uint64_t b, uint64_t m) { return a >= m - b ? a - (m - b) : a + b; }
static inline uint64_t zov_factor_mulmod(uint64_t a, uint64_t b, uint64_t m) { uint64_t r = 0; while (b) { if (b & 1) r = zov_factor_addmod(r, a, m); b >>= 1; if (b) a = zov_factor_addmod(a, a, m); } return r; }
static inline uint64_t zov_factor_powmod(uint64_t a, uint64_t e, uint64_t m) { uint64_t r = 1; while (e) { if (e & 1) r = zov_factor_mulmod(r, a, m); e >>= 1; if (e) a = zov_factor_mulmod(a, a, m); } return r; }
static inline int zov_factor_prime(uint64_t n) {
    if (n < 2) return 0;
    static const uint32_t small[] = {2,3,5,7,11,13,17,19,23,29,31,37};
    for (size_t i = 0; i < sizeof(small)/sizeof(small[0]); ++i) { if (n == small[i]) return 1; if (n % small[i] == 0) return 0; }
    uint64_t d = n - 1; unsigned s = 0; while (!(d & 1)) { d >>= 1; ++s; }
    static const uint64_t bases[] = {2,325,9375,28178,450775,9780504,1795265022};
    for (size_t i = 0; i < sizeof(bases)/sizeof(bases[0]); ++i) {
        uint64_t a = bases[i] % n; if (!a) continue;
        uint64_t x = zov_factor_powmod(a, d, n); if (x == 1 || x == n - 1) continue;
        unsigned j; for (j = 1; j < s; ++j) { x = zov_factor_mulmod(x, x, n); if (x == n - 1) break; }
        if (j == s) return 0;
    }
    return 1;
}
static inline uint64_t zov_factor_gcd(uint64_t a, uint64_t b) { while (b) { uint64_t r = a % b; a = b; b = r; } return a; }
static inline uint64_t zov_factor_rho(uint64_t n) {
    if (!(n & 1)) return 2;
    for (uint64_t c = 1; c < 64; ++c) {
        uint64_t x = 2 + c, y = x, d = 1;
        for (uint64_t iter = 0; d == 1 && iter < 2000000; ++iter) {
            x = zov_factor_addmod(zov_factor_mulmod(x, x, n), c, n);
            y = zov_factor_addmod(zov_factor_mulmod(y, y, n), c, n);
            y = zov_factor_addmod(zov_factor_mulmod(y, y, n), c, n);
            uint64_t delta = x > y ? x - y : y - x;
            d = zov_factor_gcd(delta, n);
        }
        if (d > 1 && d < n) return d;
    }
    return n;
}
static inline int zov_factor_collect(uint64_t n, uint64_t *f, size_t *count) {
    if (n == 1) return 1;
    if (zov_factor_prime(n)) { if (*count >= 64) return 0; f[(*count)++] = n; return 1; }
    if (!(n & 1)) { if (*count >= 64) return 0; f[(*count)++] = 2; return zov_factor_collect(n / 2, f, count); }
    uint64_t d = zov_factor_rho(n);
    if (d == n) return 0;
    return zov_factor_collect(d, f, count) && zov_factor_collect(n / d, f, count);
}
static inline void zov_factor_sort(uint64_t *f, size_t n) { for (size_t i=1;i<n;++i) { uint64_t v=f[i]; size_t j=i; while(j && f[j-1]>v) { f[j]=f[j-1]; --j; } f[j]=v; } }
typedef struct { uint64_t hi, lo; } zov_u128;
static inline zov_u128 zov_u128_make(uint64_t hi,uint64_t lo){zov_u128 x={hi,lo};return x;}
static inline zov_u128 zov_u128_small(uint64_t n){return zov_u128_make(0,n);}
static inline int zov_u128_cmp(zov_u128 a,zov_u128 b){return a.hi<b.hi?-1:a.hi>b.hi?1:a.lo<b.lo?-1:a.lo>b.lo?1:0;}
static inline int zov_u128_zero(zov_u128 a){return !a.hi&&!a.lo;}
static inline zov_u128 zov_u128_sub(zov_u128 a,zov_u128 b){uint64_t borrow=a.lo<b.lo;return zov_u128_make(a.hi-b.hi-borrow,a.lo-b.lo);}
static inline int zov_u128_add_checked(zov_u128 a,zov_u128 b,zov_u128 *out){out->lo=a.lo+b.lo;uint64_t carry=out->lo<a.lo;out->hi=a.hi+b.hi+carry;return out->hi<a.hi||(carry&&out->hi==a.hi);}
static inline zov_u128 zov_u128_shl(zov_u128 a){return zov_u128_make((a.hi<<1)|(a.lo>>63),a.lo<<1);}
static inline zov_u128 zov_u128_shr(zov_u128 a){return zov_u128_make(a.hi>>1,(a.lo>>1)|(a.hi<<63));}
static inline int zov_u128_bit(zov_u128 a,unsigned bit){return bit<64?(int)((a.lo>>bit)&1):(int)((a.hi>>(bit-64))&1);}
static inline zov_u128 zov_u128_divmod(zov_u128 a,zov_u128 m,zov_u128*remainder){zov_u128 r=zov_u128_small(0),q=zov_u128_small(0);for(int i=127;i>=0;--i){int carry=(int)(r.hi>>63);r=zov_u128_shl(r);r.lo|=(uint64_t)zov_u128_bit(a,(unsigned)i);if(carry||zov_u128_cmp(r,m)>=0){r=zov_u128_sub(r,m);if(i<64)q.lo|=(uint64_t)1<<i;else q.hi|=(uint64_t)1<<(i-64);}}if(remainder)*remainder=r;return q;}
static inline zov_u128 zov_u128_mod(zov_u128 a,zov_u128 m){zov_u128 r;zov_u128_divmod(a,m,&r);return r;}
static inline zov_u128 zov_u128_addmod(zov_u128 a,zov_u128 b,zov_u128 m){zov_u128 gap=zov_u128_sub(m,b);return zov_u128_cmp(a,gap)>=0?zov_u128_sub(a,gap):zov_u128_make(a.hi+b.hi+(a.lo+b.lo<a.lo),a.lo+b.lo);}
static inline zov_u128 zov_u128_mulmod(zov_u128 a,zov_u128 b,zov_u128 m){zov_u128 r=zov_u128_small(0);while(!zov_u128_zero(b)){if(b.lo&1)r=zov_u128_addmod(r,a,m);b=zov_u128_shr(b);if(!zov_u128_zero(b))a=zov_u128_addmod(a,a,m);}return r;}
static inline zov_u128 zov_u128_powmod(zov_u128 a,zov_u128 e,zov_u128 m){zov_u128 r=zov_u128_small(1);while(!zov_u128_zero(e)){if(e.lo&1)r=zov_u128_mulmod(r,a,m);e=zov_u128_shr(e);if(!zov_u128_zero(e))a=zov_u128_mulmod(a,a,m);}return r;}
static inline uint32_t zov_u128_modsmall(zov_u128 a,uint32_t d){uint64_t r=0;for(int i=127;i>=0;--i)r=(r*2+(uint64_t)zov_u128_bit(a,(unsigned)i))%d;return(uint32_t)r;}
static inline zov_u128 zov_u128_divsmall(zov_u128 a,uint32_t d,uint32_t *rem){zov_u128 q=zov_u128_small(0);uint64_t r=0;for(int i=127;i>=0;--i){r=r*2+(uint64_t)zov_u128_bit(a,(unsigned)i);if(r>=d){r-=d;if(i<64)q.lo|=(uint64_t)1<<i;else q.hi|=(uint64_t)1<<(i-64);}}*rem=(uint32_t)r;return q;}
static inline int zov_u128_parse(const char *s,zov_u128 *out){zov_u128 n=zov_u128_small(0);if(!*s)return 0;for(const unsigned char*p=(const unsigned char*)s;*p;++p){if(*p<'0'||*p>'9')return 0;zov_u128 t=zov_u128_small(0);for(int i=0;i<10;++i){zov_u128 q;if(zov_u128_add_checked(t,n,&q))return 0;t=q;}zov_u128 q;if(zov_u128_add_checked(t,zov_u128_small((uint64_t)(*p-'0')),&q))return 0;n=q;}*out=n;return 1;}
static inline void zov_u128_string(zov_u128 n,char out[40]){char rev[40];size_t k=0;if(zov_u128_zero(n)){strcpy(out,"0");return;}while(!zov_u128_zero(n)){uint32_t r;n=zov_u128_divsmall(n,10,&r);rev[k++]=(char)('0'+r);}for(size_t i=0;i<k;++i)out[i]=rev[k-i-1];out[k]=0;}
static inline int zov_u128_prime(zov_u128 n){if(!n.hi&&n.lo<2)return 0;static const uint32_t primes[]={2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97};for(size_t i=0;i<sizeof(primes)/sizeof(primes[0]);++i){uint32_t r=zov_u128_modsmall(n,primes[i]);if(!r)return !n.hi&&n.lo==primes[i];}
    zov_u128 nm1=zov_u128_sub(n,zov_u128_small(1)),d=nm1;unsigned s=0;while(!(d.lo&1)){d=zov_u128_shr(d);++s;}static const uint32_t bases[]={2,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71};for(size_t i=0;i<sizeof(bases)/sizeof(bases[0]);++i){zov_u128 a=zov_u128_mod(zov_u128_small(bases[i]),n);if(zov_u128_zero(a))continue;zov_u128 x=zov_u128_powmod(a,d,n);if(zov_u128_cmp(x,zov_u128_small(1))==0||zov_u128_cmp(x,nm1)==0)continue;unsigned j;for(j=1;j<s;++j){x=zov_u128_mulmod(x,x,n);if(zov_u128_cmp(x,nm1)==0)break;}if(j==s)return 0;}return 1;}
static inline zov_u128 zov_u128_gcd(zov_u128 a,zov_u128 b){while(!zov_u128_zero(b)){zov_u128 r=zov_u128_mod(a,b);a=b;b=r;}return a;}
static inline zov_u128 zov_u128_rho(zov_u128 n){if(!(n.lo&1))return zov_u128_small(2);for(uint64_t c=1;c<24;++c){zov_u128 cv=zov_u128_small(c),x=zov_u128_small(2+c),y=x,d=zov_u128_small(1);for(unsigned i=0;i<250000&&zov_u128_cmp(d,zov_u128_small(1))==0;++i){x=zov_u128_addmod(zov_u128_mulmod(x,x,n),cv,n);y=zov_u128_addmod(zov_u128_mulmod(y,y,n),cv,n);y=zov_u128_addmod(zov_u128_mulmod(y,y,n),cv,n);zov_u128 diff=zov_u128_cmp(x,y)>=0?zov_u128_sub(x,y):zov_u128_sub(y,x);d=zov_u128_gcd(diff,n);}if(zov_u128_cmp(d,zov_u128_small(1))>0&&zov_u128_cmp(d,n)<0)return d;}return n;}
static inline int zov_u128_collect(zov_u128 n,zov_u128*f,size_t*count){if(!n.hi&&n.lo==1)return 1;if(zov_u128_prime(n)){if(*count>=128)return 0;f[(*count)++]=n;return 1;}uint32_t pset[]={2,3,5,7,11,13,17,19,23,29,31,37,41,43,47};for(size_t i=0;i<sizeof(pset)/sizeof(pset[0]);++i)if(zov_u128_modsmall(n,pset[i])==0){if(*count>=128)return 0;f[(*count)++]=zov_u128_small(pset[i]);uint32_t rem;zov_u128 q=zov_u128_divsmall(n,pset[i],&rem);return rem==0&&zov_u128_collect(q,f,count);}for(uint32_t d=53;d<=10000;d+=2)if(zov_u128_modsmall(n,d)==0){if(*count>=128)return 0;f[(*count)++]=zov_u128_small(d);uint32_t rem;zov_u128 q=zov_u128_divsmall(n,d,&rem);return rem==0&&zov_u128_collect(q,f,count);}zov_u128 d=zov_u128_rho(n);if(zov_u128_cmp(d,n)==0)return 0;zov_u128 rem;zov_u128 q=zov_u128_divmod(n,d,&rem);return zov_u128_zero(rem)&&zov_u128_collect(d,f,count)&&zov_u128_collect(q,f,count);}
static inline void zov_u128_sort(zov_u128*f,size_t n){for(size_t i=1;i<n;++i){zov_u128 v=f[i];size_t j=i;while(j&&zov_u128_cmp(f[j-1],v)>0){f[j]=f[j-1];--j;}f[j]=v;}}
static inline uint32_t zov_factor_decimal_div(const char *digits,char *quotient,uint32_t divisor){uint64_t rem=0;for(size_t i=0;digits[i];++i){uint64_t x=rem*10+(unsigned)(digits[i]-'0');quotient[i]=(char)('0'+x/divisor);rem=x%divisor;}quotient[strlen(digits)]=0;size_t n=strlen(quotient),i=0;while(i+1<n&&quotient[i]=='0')++i;if(i)memmove(quotient,quotient+i,n-i+1);return(uint32_t)rem;}
static inline int zov_factor_small_prime(uint32_t n){if(n<2)return 0;if(!(n&1))return n==2;for(uint32_t d=3;d<=n/d;d+=2)if(n%d==0)return 0;return 1;}
static inline int zov_factor_big_decimal(const char *token,int exponents){const char*p=token[0]=='+'?token+1:token;size_t n=strlen(p);if(!n||n>10000)return 0;for(size_t i=0;i<n;++i)if(p[i]<'0'||p[i]>'9')return 0;char*remaining=(char*)malloc(n+1),*quotient=(char*)malloc(n+1);if(!remaining||!quotient){free(remaining);free(quotient);return 0;}memcpy(remaining,p,n+1);while(remaining[0]=='0'&&remaining[1])memmove(remaining,remaining+1,strlen(remaining));printf("%s:",remaining);if(strcmp(remaining,"0")&&strcmp(remaining,"1")){size_t cap=32,count=0;uint32_t*f=(uint32_t*)malloc(cap*sizeof(*f));if(!f){free(remaining);free(quotient);return 0;}for(uint32_t d=2;d<=10000;++d)if(zov_factor_small_prime(d)){uint32_t rem;while((rem=zov_factor_decimal_div(remaining,quotient,d))==0){strcpy(remaining,quotient);if(count==cap){cap*=2;uint32_t*q=(uint32_t*)realloc(f,cap*sizeof(*f));if(!q){free(f);free(remaining);free(quotient);return 0;}f=q;}f[count++]=d;}}char extra[128][40];size_t extras=0;if(strcmp(remaining,"1")){zov_u128 rest;if(strlen(remaining)<=39&&zov_u128_parse(remaining,&rest)){zov_u128 more[128];size_t m=0;if(!zov_u128_collect(rest,more,&m)){free(f);free(remaining);free(quotient);return 0;}zov_u128_sort(more,m);for(size_t i=0;i<m;++i){if(more[i].hi){free(f);free(remaining);free(quotient);return 0;}if(more[i].lo<=UINT32_MAX){if(count==cap){cap*=2;uint32_t*q=(uint32_t*)realloc(f,cap*sizeof(*f));if(!q){free(f);free(remaining);free(quotient);return 0;}f=q;}f[count++]=(uint32_t)more[i].lo;}else if(extras<128)zov_u128_string(more[i],extra[extras++]);}}else{free(f);free(remaining);free(quotient);return 0;}}for(size_t i=0;i<count;){size_t j=i+1;while(j<count&&f[j]==f[i])++j;printf(" %u",f[i]);if(exponents&&j-i>1)printf("^%zu",j-i);else if(!exponents)for(size_t k=i+1;k<j;++k)printf(" %u",f[i]);i=j;}for(size_t i=0;i<extras;++i)printf(" %s",extra[i]);free(f);}free(remaining);free(quotient);putchar('\n');return !ferror(stdout);}
static inline int zov_factor_one(const char *token, int exponents) {
    if (!token || !*token) return 1;
    const char *digits=token[0]=='+'?token+1:token;
    zov_u128 n;
    if(!zov_u128_parse(digits,&n)) {
        int decimal=*digits!=0;for(const unsigned char*q=(const unsigned char*)digits;*q;++q)if(*q<'0'||*q>'9'){decimal=0;break;}
        if(decimal)return zov_factor_big_decimal(token,exponents)?0:1;
        goto invalid;
    }
    char number[40];zov_u128_string(n,number);
    printf("%s:",number);
    if (n.hi || n.lo > 1) {
      if(!n.hi) {
        uint64_t factors[64]; size_t count = 0;
        if (!zov_factor_collect(n.lo, factors, &count)) return 1;
        zov_factor_sort(factors, count);
        if (exponents) for (size_t i = 0; i < count;) { size_t j = i + 1; while (j < count && factors[j] == factors[i]) ++j; printf(" %llu", (unsigned long long)factors[i]); if (j - i > 1) printf("^%zu", j - i); i = j; }
        else for (size_t i = 0; i < count; ++i) printf(" %llu", (unsigned long long)factors[i]);
      } else {
        zov_u128 factors[128];size_t count=0;
        if(!zov_u128_collect(n,factors,&count))return 1;
        zov_u128_sort(factors,count);
        for(size_t i=0;i<count;){size_t j=i+1;while(j<count&&zov_u128_cmp(factors[j],factors[i])==0)++j;char value[40];zov_u128_string(factors[i],value);printf(" %s",value);if(exponents&&j-i>1)printf("^%zu",j-i);else if(!exponents)for(size_t k=i+1;k<j;++k)printf(" %s",value);i=j;}
      }
    }
    putchar('\n'); return ferror(stdout) ? 1 : 0;
invalid:
    fprintf(stderr, "factor: '%s' is not a valid positive integer\n", token); return 1;
}
static inline int zov_factor_stdin(int exponents) {
    size_t cap=128,n=0; char *token=(char*)malloc(cap); if(!token)return 1; int errors=0,c;
    while((c=getchar())!=EOF){if(isspace((unsigned char)c)){if(n){token[n]=0;if(zov_factor_one(token,exponents))errors=1;n=0;}}else{if(n+1>=cap){if(cap>SIZE_MAX/2){free(token);return 1;}size_t next=cap*2;char*p=(char*)realloc(token,next);if(!p){free(token);return 1;}token=p;cap=next;}token[n++]=(char)c;}}
    if(n){token[n]=0;if(zov_factor_one(token,exponents))errors=1;}
    free(token);return errors;
}
static inline int zov_factor_run(int argc, int8_t **argv) {
    int exponents = 0, options = 1, errors = 0, operands = 0;
    for (int i = 0; i < argc; ++i) {
        const char *a = (const char *)argv[i];
        if (options && !strcmp(a, "--")) { options = 0; continue; }
        if (options && (!strcmp(a, "--help") || !strcmp(a, "--version"))) { if (!strcmp(a, "--help")) puts("Usage: factor [OPTION] [NUMBER]...\nPrint the prime factors of each specified integer NUMBER.  If none\nare specified on the command line, read them from standard input.\n\n  -h, --exponents\n         print repeated factors in form p^e unless e is 1\n      --help\n         display this help and exit\n      --version\n         output version information and exit\n\nReport bugs to: bug-coreutils@gnu.org\nGNU coreutils home page: <https://www.gnu.org/software/coreutils/>\nGeneral help using GNU software: <https://www.gnu.org/gethelp/>\nReport any translation bugs to <https://translationproject.org/team/>\nFull documentation <https://www.gnu.org/software/coreutils/factor>\nor available locally via: info '(coreutils) factor invocation'"); else puts("factor (zovutils) 0.10.18"); return 0; }
        if (options && (!strcmp(a, "-h") || !strcmp(a, "--exponents"))) { exponents = 1; continue; }
        if (options && a[0] == '-' && a[1] == '-') { fprintf(stderr, "factor: unrecognized option '%s'\nTry 'factor --help' for more information.\n", a); return 1; }
        if (options && a[0] == '-' && a[1]) { fprintf(stderr, "factor: invalid option -- '%c'\nTry 'factor --help' for more information.\n", a[1]); return 1; }
        ++operands; if (zov_factor_one(a, exponents)) errors = 1;
    }
    if (!operands) errors=zov_factor_stdin(exponents);
    return errors || ferror(stdout) ? 1 : 0;
}
#endif
