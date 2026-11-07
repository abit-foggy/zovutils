#ifndef ZOVUTILS_HASH_CHECK_BACKEND_H
#define ZOVUTILS_HASH_CHECK_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

typedef int (*zov_checksum_digest_fn)(const char *, int, unsigned char *, size_t *);

static inline void zov_checksum_print(const char *program, const char *label,
                                      const unsigned char *digest, size_t len,
                                      const char *name, int tag, int zero) {
    char hex[257];
    for(size_t i=0;i<len;i++)sprintf(hex+2*i,"%02x",digest[i]);
    hex[len*2]=0;
    if(tag)printf("%s (%s) = %s",label,name,hex);
    else printf("%s  %s",hex,name);
    putchar(zero?0:'\n');
    (void)program;
}

static inline int zov_checksum_hex(const char *s, size_t n) {
    for (size_t i=0;i<n;i++) if (!isxdigit((unsigned char)s[i])) return 0;
    return 1;
}

static inline int zov_checksum_parse_line(char *line, size_t len, const char *label,
                                          size_t digest_len, char **filename,
                                          const char **expected) {
    while (len && (line[len-1]=='\n' || line[len-1]=='\r' || line[len-1]==0)) len--;
    line[len]=0;
    if (len && line[0]=='\\') { memmove(line,line+1,len); len--; }
    size_t hexlen=digest_len*2;
    if (len>=hexlen+2 && zov_checksum_hex(line,hexlen) && line[hexlen]==' ' &&
        (line[hexlen+1]==' ' || line[hexlen+1]=='*')) {
        *filename=line+hexlen+2; *expected=line; return **filename!=0;
    }
    size_t label_len=strlen(label);
    if (len>label_len+5 && !strncmp(line,label,label_len) && line[label_len]==' ' && line[label_len+1]=='(') {
        char *close=NULL;
        for (size_t i=label_len+2;i+3<len;i++) if (line[i]==')'&&line[i+1]==' '&&line[i+2]=='=') close=line+i;
        if (close) {
            char *hash=close+3; while (*hash==' ') hash++;
            if ((size_t)(line+len-hash)==hexlen && zov_checksum_hex(hash,hexlen)) {
                *close=0; *filename=line+label_len+2; *expected=hash; return **filename!=0;
            }
        }
    }
    return 0;
}

static inline int zov_checksum_check_run(const char *program, const char *label, int algorithm,
                                          size_t digest_len, int argc, char **argv,
                                          zov_checksum_digest_fn digest) {
    int status_mode=0, quiet=0, warn=0, strict=0, ignore_missing=0, zero=0, nf=0;
    const char **manifests=(const char **)malloc((size_t)(argc+1)*sizeof(char *));
    if(!manifests)return 1;
    for(int i=0;i<argc;i++){
        const char *a=argv[i];
        if(!strcmp(a,"--")){for(i++;i<argc;i++)manifests[nf++]=argv[i];break;}
        if(!strcmp(a,"-c")||!strcmp(a,"--check"))continue;
        if(!strcmp(a,"--status")){status_mode=1;continue;}
        if(!strcmp(a,"--quiet")||!strcmp(a,"--silent")){quiet=1;continue;}
        if(!strcmp(a,"--warn")){warn=1;continue;}
        if(!strcmp(a,"--strict")){strict=1;continue;}
        if(!strcmp(a,"--ignore-missing")){ignore_missing=1;continue;}
        if(!strcmp(a,"-z")||!strcmp(a,"--zero")){zero=1;continue;}
        if(!strcmp(a,"--tag")){fprintf(stderr,"%s: the --tag option is meaningless when verifying checksums\nTry '%s --help' for more information.\n",program,program);free(manifests);return 1;}
        if(!strcmp(a,"-b")||!strcmp(a,"--binary")||!strcmp(a,"-t")||!strcmp(a,"--text")){continue;}
        if(a[0]=='-'&&strcmp(a,"-")){fprintf(stderr,"%s: unrecognized option '%s'\nTry '%s --help' for more information.\n",program,a,program);free(manifests);return 1;}
        manifests[nf++]=a;
    }
    int implicit=!nf;if(implicit)manifests[nf++]="-";
    unsigned long good=0,bad=0,unreadable=0,formatted=0,verified=0;
    int failed=0;
    for(int fi=0;fi<nf;fi++){
        const char *manifest=manifests[fi]; FILE *mf=stdin;
        if(strcmp(manifest,"-")){mf=fopen(manifest,"rb");if(!mf){fprintf(stderr,"%s: %s: %s\n",program,manifest,strerror(errno));failed=1;continue;}}
        char *line=NULL;size_t cap=0;ssize_t got;unsigned long line_no=0;int delim=zero?0:'\n';
        while((got=getdelim(&line,&cap,delim,mf))>=0){
            line_no++;
            char *name=NULL;const char *expected=NULL;
            if(!zov_checksum_parse_line(line,(size_t)got,label,digest_len,&name,&expected)){
                if(warn||strict){if(implicit)fprintf(stderr,"%s: 'standard input': %lu: improperly formatted %s checksum line\n",program,line_no,label);else fprintf(stderr,"%s: %s: %lu: improperly formatted %s checksum line\n",program,manifest,line_no,label);}
                if(strict)failed=1;
                continue;
            }
            formatted++;
            unsigned char actual[128];size_t actual_len=0;errno=0;
            if(digest(name,algorithm,actual,&actual_len)!=0){
                if(!(ignore_missing&&errno==ENOENT)){
                    fprintf(stderr,"%s: %s: %s\n",program,name,strerror(errno));
                    if(!status_mode){printf("%s: FAILED open or read",name);putchar(zero?0:'\n');}
                    unreadable++;failed=1;
                }
                continue;
            }
            verified++;
            char hex[257];for(size_t j=0;j<actual_len;j++)sprintf(hex+2*j,"%02x",actual[j]);hex[actual_len*2]=0;
            int matches=actual_len==digest_len;
            for(size_t j=0;matches&&j<digest_len*2;j++)if(tolower((unsigned char)hex[j])!=tolower((unsigned char)expected[j]))matches=0;
            if(matches){good++;if(!quiet&&!status_mode){printf("%s: OK",name);putchar(zero?0:'\n');}}
            else{bad++;failed=1;if(!status_mode){printf("%s: FAILED",name);putchar(zero?0:'\n');}}
        }
        free(line);
        if(ferror(mf)){fprintf(stderr,"%s: %s: read error: %s\n",program,manifest,strerror(errno));failed=1;}
        if(mf!=stdin)fclose(mf);
    }
    if(!formatted){if(implicit)fprintf(stderr,"%s: 'standard input': no properly formatted checksum lines found\n",program);else fprintf(stderr,"%s: %s: no properly formatted checksum lines found\n",program,manifests[0]);failed=1;}
    else if(ignore_missing&&!verified){if(implicit)fprintf(stderr,"%s: 'standard input': no file was verified\n",program);else fprintf(stderr,"%s: %s: no file was verified\n",program,manifests[0]);failed=1;}
    if(!status_mode&&bad)fprintf(stderr,"%s: WARNING: %lu computed checksum%s did NOT match\n",program,bad,bad==1?"":"s");
    if(!status_mode&&unreadable)fprintf(stderr,"%s: WARNING: %lu listed file%s could not be read\n",program,unreadable,unreadable==1?"":"s");
    free(manifests);(void)good;return failed?1:0;
}
#endif
