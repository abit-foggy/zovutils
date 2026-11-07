#ifndef ZOVUTILS_B2SUM_BACKEND_H
#define ZOVUTILS_B2SUM_BACKEND_H
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
typedef struct { uint64_t h[8], t; unsigned char b[128]; size_t used; } zov_b2ctx;
static inline uint64_t zb2_rotr(uint64_t x,unsigned n){return(x>>n)|(x<<(64-n));}
static inline uint64_t zb2_load(const unsigned char*p){uint64_t x=0;for(int i=7;i>=0;i--)x=(x<<8)|p[i];return x;}
static inline void zb2_store(unsigned char*p,uint64_t x){for(int i=0;i<8;i++){p[i]=(unsigned char)x;x>>=8;}}
static inline void zb2_compress(zov_b2ctx*c,int last){static const uint64_t iv[8]={0x6a09e667f3bcc908ULL,0xbb67ae8584caa73bULL,0x3c6ef372fe94f82bULL,0xa54ff53a5f1d36f1ULL,0x510e527fade682d1ULL,0x9b05688c2b3e6c1fULL,0x1f83d9abfb41bd6bULL,0x5be0cd19137e2179ULL};static const unsigned char s[12][16]={{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},{14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3},{11,8,12,0,5,2,15,13,10,14,3,6,7,1,9,4},{7,9,3,1,13,12,11,14,2,6,5,10,4,0,15,8},{9,0,5,7,2,4,10,15,14,1,11,12,6,8,3,13},{2,12,6,10,0,11,8,3,4,13,7,5,15,14,1,9},{12,5,1,15,14,13,4,10,0,7,6,3,9,2,8,11},{13,11,7,14,12,1,3,9,5,0,15,4,8,6,2,10},{6,15,14,9,11,3,0,8,12,2,13,7,1,4,10,5},{10,2,8,4,7,6,1,5,15,11,9,14,3,12,13,0},{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},{14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3}};uint64_t m[16],v[16];for(int i=0;i<16;i++)m[i]=zb2_load(c->b+i*8);for(int i=0;i<8;i++){v[i]=c->h[i];v[i+8]=iv[i];}v[12]^=c->t;if(last)v[14]=~v[14];
#define G(a,b,d,e,x,y) do{v[a]=v[a]+v[b]+(x);v[e]=zb2_rotr(v[e]^v[a],32);v[d]+=v[e];v[b]=zb2_rotr(v[b]^v[d],24);v[a]=v[a]+v[b]+(y);v[e]=zb2_rotr(v[e]^v[a],16);v[d]+=v[e];v[b]=zb2_rotr(v[b]^v[d],63);}while(0)
for(int r=0;r<12;r++){const unsigned char*q=s[r];G(0,4,8,12,m[q[0]],m[q[1]]);G(1,5,9,13,m[q[2]],m[q[3]]);G(2,6,10,14,m[q[4]],m[q[5]]);G(3,7,11,15,m[q[6]],m[q[7]]);G(0,5,10,15,m[q[8]],m[q[9]]);G(1,6,11,12,m[q[10]],m[q[11]]);G(2,7,8,13,m[q[12]],m[q[13]]);G(3,4,9,14,m[q[14]],m[q[15]]);}for(int i=0;i<8;i++)c->h[i]^=v[i]^v[i+8];
#undef G
}
static inline void zb2_init(zov_b2ctx*c){static const uint64_t iv[8]={0x6a09e667f3bcc908ULL,0xbb67ae8584caa73bULL,0x3c6ef372fe94f82bULL,0xa54ff53a5f1d36f1ULL,0x510e527fade682d1ULL,0x9b05688c2b3e6c1fULL,0x1f83d9abfb41bd6bULL,0x5be0cd19137e2179ULL};memset(c,0,sizeof(*c));memcpy(c->h,iv,sizeof(iv));c->h[0]^=0x01010040ULL;}
static inline void zb2_update(zov_b2ctx*c,const unsigned char*p,size_t n){while(n){if(c->used==128){c->t+=128;zb2_compress(c,0);c->used=0;}size_t z=128-c->used;if(z>n)z=n;memcpy(c->b+c->used,p,z);c->used+=z;p+=z;n-=z;}}
static inline void zb2_final(zov_b2ctx*c,unsigned char*out){c->t+=c->used;memset(c->b+c->used,0,128-c->used);zb2_compress(c,1);for(int i=0;i<8;i++)zb2_store(out+i*8,c->h[i]);}
static inline int zov_b2sum_run(int argc,char**argv){char**files=(char**)malloc((size_t)(argc+1)*sizeof(char*));if(!files)return 1;int nf=0,status=0;for(int i=0;i<argc;i++){if(!strcmp(argv[i],"--version")){puts("b2sum (zovutils) 0.8.0");free(files);return 0;}if(!strcmp(argv[i],"--help")){puts("Usage: b2sum [OPTION]... [FILE]...\nPrint or check BLAKE2b checksums.\nWith no FILE, or when FILE is -, read standard input.\n  --help     display this help and exit\n  --version  output version information and exit");free(files);return 0;}if(argv[i][0]=='-'&&strcmp(argv[i],"-")){fprintf(stderr,"b2sum: unrecognized option '%s'\nTry 'b2sum --help' for more information.\n",argv[i]);free(files);return 1;}files[nf++]=argv[i];}int implicit=!nf;if(implicit)files[nf++]="-";for(int j=0;j<nf;j++){const char*name=files[j];FILE*f=stdin;if(strcmp(name,"-")){f=fopen(name,"rb");if(!f){fprintf(stderr,"b2sum: %s: %s\n",name,strerror(errno));status=1;continue;}}zov_b2ctx c;zb2_init(&c);unsigned char b[16384],d[64];size_t n;while((n=fread(b,1,sizeof(b),f)))zb2_update(&c,b,n);if(ferror(f)){fprintf(stderr,"b2sum: %s: read error: %s\n",name,strerror(errno));status=1;}if(f!=stdin)fclose(f);zb2_final(&c,d);for(int i=0;i<64;i++)printf("%02x",d[i]);if(implicit)puts("  -");else printf("  %s\n",name);}free(files);return status;}
#endif
