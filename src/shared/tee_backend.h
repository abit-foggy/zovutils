#ifndef ZOVUTILS_TEE_BACKEND_H
#define ZOVUTILS_TEE_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

typedef struct { FILE *stream; const char *name; int standard; int active; } zov_tee_output;

static inline int zov_tee_mode(const char *value) {
    if (!strcmp(value,"warn")) return 1;
    if (!strcmp(value,"warn-nopipe")) return 2;
    if (!strcmp(value,"exit")) return 3;
    if (!strcmp(value,"exit-nopipe")) return 4;
    return 0;
}
static inline int zov_tee_write_error(zov_tee_output *out, int mode, int err) {
    int is_pipe=err==EPIPE;
    int warn=mode==1 || !is_pipe;
    int stop=mode==3 || (mode==4 && !is_pipe) || (mode==0 && !is_pipe);
    if(warn) fprintf(stderr,"tee: '%s': %s\n",out->standard?"standard output":out->name,strerror(err));
    if(out->standard) fflush(out->stream); else fclose(out->stream);
    out->active=0;
    return stop;
}
static inline int zov_tee_run(int argc,char**argv) {
    int append=0,ignore_int=0,end=0,nf=0,status=0,mode=0;
    const char **names=(const char**)malloc((size_t)(argc+1)*sizeof(char*));
    if(!names)return 1;
    for(int i=0;i<argc;i++) {
        const char*a=argv[i];
        if(!end&&!strcmp(a,"--")){end=1;continue;}
        if(!end&&(!strcmp(a,"--help")||!strcmp(a,"--version"))) {
            if(!strcmp(a,"--version"))puts("tee (zovutils) 0.10.51");
            else puts("Usage: tee [OPTION]... [FILE]...\nCopy standard input to each FILE, and also to standard output.\n  -a, --append              append to the given FILEs, do not overwrite\n  -i, --ignore-interrupts   ignore interrupt signals\n  -p                        operate in warn-nopipe mode with pipes\n      --output-error[=MODE] set behavior on write error\n      --help                display this help and exit\n      --version             output version information and exit");
            free(names);return 0;
        }
        if(!end&&(!strcmp(a,"-a")||!strcmp(a,"--append"))){append=1;continue;}
        if(!end&&(!strcmp(a,"-i")||!strcmp(a,"--ignore-interrupts"))){ignore_int=1;continue;}
        if(!end&&!strcmp(a,"-p")){mode=2;continue;}
        if(!end&&!strcmp(a,"--output-error")){mode=2;continue;}
        if(!end&&!strncmp(a,"--output-error=",15)){
            mode=zov_tee_mode(a+15);
            if(!mode){fprintf(stderr,"tee: invalid argument '%s' for '--output-error'\nValid arguments are:\n  - 'warn'\n  - 'warn-nopipe'\n  - 'exit'\n  - 'exit-nopipe'\nTry 'tee --help' for more information.\n",a+15);free(names);return 1;}
            continue;
        }
        if(!end&&a[0]=='-'&&strcmp(a,"-")){fprintf(stderr,"tee: unrecognized option '%s'\nTry 'tee --help' for more information.\n",a);free(names);return 1;}
        names[nf++]=a;
    }
    if(ignore_int)signal(SIGINT,SIG_IGN);
    signal(SIGPIPE,SIG_IGN);
    zov_tee_output *outs=(zov_tee_output*)calloc((size_t)nf+1,sizeof(*outs));
    if(!outs){free(names);return 1;}
    outs[0]=(zov_tee_output){stdout,"standard output",1,1};
    for(int i=0;i<nf;i++){
        FILE*f=fopen(names[i],append?"ab":"wb");
        if(!f){fprintf(stderr,"tee: %s: %s\n",names[i],strerror(errno));status=1;continue;}
        outs[i+1]=(zov_tee_output){f,names[i],0,1};
        setvbuf(f,NULL,_IONBF,0);
    }
    setvbuf(stdout,NULL,_IONBF,0);
    unsigned char buf[16384];size_t n;int stop=0;
    while(!stop&&(n=fread(buf,1,sizeof(buf),stdin))>0){
        for(int i=0;i<nf+1;i++)if(outs[i].active){
            errno=0;
            if(fwrite(buf,1,n,outs[i].stream)!=n){int err=errno?errno:EIO;status=1;if(zov_tee_write_error(&outs[i],mode,err))stop=1;}
        }
        if(mode==0||mode==2||mode==4){int active=0;for(int i=0;i<nf+1;i++)active+=outs[i].active;if(!active)stop=1;}
    }
    if(ferror(stdin))status=1;
    for(int i=0;i<nf+1;i++)if(outs[i].active){
        errno=0;
        if(fflush(outs[i].stream)!=0){int err=errno?errno:EIO;status=1;if(zov_tee_write_error(&outs[i],mode,err))stop=1;}
        if(outs[i].active&&!outs[i].standard){errno=0;if(fclose(outs[i].stream)!=0){int err=errno?errno:EIO;status=1;int pipe=err==EPIPE;int warn=mode==1||!pipe;if(warn)fprintf(stderr,"tee: '%s': %s\n",outs[i].name,strerror(err));}}
    }
    free(outs);free(names);return status;
}
#endif
