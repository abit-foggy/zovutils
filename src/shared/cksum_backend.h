#ifndef ZOVUTILS_CKSUM_BACKEND_H
#define ZOVUTILS_CKSUM_BACKEND_H
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static inline uint32_t zov_cksum_byte(uint32_t crc, unsigned char byte) {
    crc ^= (uint32_t)byte << 24;
    for (int i=0;i<8;i++) crc = (crc & 0x80000000u) ? (crc << 1) ^ 0x04c11db7u : crc << 1;
    return crc;
}
static inline uint32_t zov_cksum_crc32b_byte(uint32_t crc, unsigned char byte) {
    crc ^= byte;
    for (int i=0;i<8;i++) crc = (crc & 1u) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
    return crc;
}
static inline int zov_cksum_run(const char *program, int argc, char **argv) {
    int is_cksum=!strcmp(program,"cksum"), algorithm=!strcmp(program,"cksum")?0:1, end_options=0, status=0, operands=0;
    char **files=(char**)malloc((size_t)(argc+1)*sizeof(char*));
    if(!files)return 1;
    for(int i=0;i<argc;i++){
        const char*a=argv[i];
        if(!end_options&&!strcmp(a,"--")){end_options=1;continue;}
        if(!end_options&&(!strcmp(a,"--help")||!strcmp(a,"--version"))){
            if(!strcmp(a,"--version")){printf("%s (zovutils) 0.10.56\n",program);free(files);return 0;}
            printf("Usage: %s [OPTION]... [FILE]...\nPrint CRC checksum and byte counts of each FILE.\nWith no FILE, or when FILE is -, read standard input.\n  -r, --algorithm=bsd   use BSD sum algorithm\n  -s, --algorithm=sysv  use System V sum algorithm\n      --help            display this help and exit\n      --version         output version information and exit\n",program);free(files);return 0;
        }
        if(!end_options&&!is_cksum&&(!strcmp(a,"-s")||!strcmp(a,"--sysv"))){algorithm=2;continue;}
        if(!end_options&&!is_cksum&&!strcmp(a,"-r")){algorithm=1;continue;}
        if(!end_options&&is_cksum&&(!strcmp(a,"-a")||!strcmp(a,"--algorithm"))){
            if(i+1>=argc){fprintf(stderr,"%s: option requires an argument -- 'a'\nTry '%s --help' for more information.\n",program,program);free(files);return 1;}
            a=argv[++i];
            if(!strcmp(a,"crc"))algorithm=0;
            else if(!strcmp(a,"bsd"))algorithm=1;
            else if(!strcmp(a,"sysv"))algorithm=2;
            else if(!strcmp(a,"crc32b"))algorithm=3;
            else {fprintf(stderr,"%s: invalid argument '%s' for '--algorithm'\nValid arguments are:\n  - 'bsd'\n  - 'crc'\n  - 'crc32b'\n  - 'sysv'\nTry '%s --help' for more information.\n",program,a,program);free(files);return 1;}
            continue;
        }
        if(!end_options&&is_cksum&&!strncmp(a,"--algorithm=",12)){
            const char *v=a+12;
            if(!strcmp(v,"crc"))algorithm=0;else if(!strcmp(v,"bsd"))algorithm=1;else if(!strcmp(v,"sysv"))algorithm=2;else if(!strcmp(v,"crc32b"))algorithm=3;
            else {fprintf(stderr,"%s: invalid argument '%s' for '--algorithm'\nValid arguments are:\n  - 'bsd'\n  - 'crc'\n  - 'crc32b'\n  - 'sysv'\nTry '%s --help' for more information.\n",program,v,program);free(files);return 1;}
            continue;
        }
        if(!end_options&&a[0]=='-'&&strcmp(a,"-")){
            if(is_cksum&&a[1]&&a[1]!='-')fprintf(stderr,"%s: invalid option -- '%c'\nTry '%s --help' for more information.\n",program,a[1],program);
            else fprintf(stderr,"%s: unrecognized option '%s'\nTry '%s --help' for more information.\n",program,a,program);
            free(files);return 1;
        }
        files[operands++]=(char*)a;
    }
    int implicit_stdin=!operands; if(implicit_stdin)files[operands++]="-";
    for(int fi=0;fi<operands;fi++){
        const char*name=files[fi];FILE*in=stdin;
        if(strcmp(name,"-")!=0){in=fopen(name,"rb");if(!in){fprintf(stderr,"%s: %s: %s\n",program,name,strerror(errno));status=1;continue;}}
        uint32_t crc=algorithm==3?0xffffffffu:0;uint64_t sum=0,len=0;unsigned char buf[16384];size_t n;
        while((n=fread(buf,1,sizeof(buf),in))>0){for(size_t j=0;j<n;j++){unsigned char b=buf[j];len++;if(algorithm==0)crc=zov_cksum_byte(crc,b);else if(algorithm==3)crc=zov_cksum_crc32b_byte(crc,b);else if(algorithm==2)sum+=b;else{sum=(sum>>1)+((sum&1)<<15);sum+=b;sum&=0xffff;}}}
        if(ferror(in)){fprintf(stderr,"%s: %s: read error: %s\n",program,name,strerror(errno));status=1;}
        if(in!=stdin)fclose(in);
        if(algorithm==0){uint64_t l=len;while(l){crc=zov_cksum_byte(crc,(unsigned char)l);l>>=8;}crc=~crc;if(implicit_stdin)printf("%u %llu\n",crc,(unsigned long long)len);else printf("%u %llu %s\n",crc,(unsigned long long)len,name);}
        else if(algorithm==3){crc=~crc;if(implicit_stdin)printf("%u %llu\n",crc,(unsigned long long)len);else printf("%u %llu %s\n",crc,(unsigned long long)len,name);}
        else if(algorithm==2){sum=(sum&0xffff)+(sum>>16);sum=(sum&0xffff)+(sum>>16);if(implicit_stdin)printf("%u %llu\n",(unsigned)sum,(unsigned long long)((len+511)/512));else printf("%u %llu %s\n",(unsigned)sum,(unsigned long long)((len+511)/512),name);}
        else if(implicit_stdin)printf("%05u %5llu\n",(unsigned)sum,(unsigned long long)((len+1023)/1024));else printf("%05u %5llu %s\n",(unsigned)sum,(unsigned long long)((len+1023)/1024),name);
    }
    free(files);return status;
}
#endif
