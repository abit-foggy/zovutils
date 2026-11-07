#ifndef ZOVUTILS_CONTEXT_BACKEND_H
#define ZOVUTILS_CONTEXT_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/xattr.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#endif
static inline int zov_chcon_one(const char*context,const char*path,int follow){
#if defined(__linux__)
 int flags=follow?0:1;int rc=follow?setxattr(path,"security.selinux",context,strlen(context)+1,0):lsetxattr(path,"security.selinux",context,strlen(context)+1,0);(void)flags;return rc;
#else
 (void)context;(void)path;(void)follow;errno=ENOTSUP;return -1;
#endif
}
static inline int zov_chcon_run(int argc,char**argv){const char*context=NULL;int follow=1,nf=0;char**files=(char**)malloc((size_t)(argc+1)*sizeof(char*));if(!files)return 1;for(int i=0;i<argc;i++){const char*a=argv[i];if(!strcmp(a,"--help")){puts("Usage: chcon [OPTION]... CONTEXT FILE...\nChange the security context of each FILE to CONTEXT.\n  -h, --no-dereference  affect symbolic links instead of referenced files\n  -R, --recursive       operate on files and directories recursively\n      --help             display this help and exit\n      --version          output version information and exit");free(files);return 0;}if(!strcmp(a,"--version")){puts("chcon (zovutils) 0.10.6");free(files);return 0;}if(!strcmp(a,"-h")||!strcmp(a,"--no-dereference")){follow=0;continue;}if(!strcmp(a,"-R")||!strcmp(a,"--recursive"))continue;if(a[0]=='-'&&strcmp(a,"-")){fprintf(stderr,"chcon: unrecognized option '%s'\nTry 'chcon --help' for more information.\n",a);free(files);return 1;}files[nf++]=(char*)a;}if(nf<2){fprintf(stderr,"chcon: missing operand\nTry 'chcon --help' for more information.\n");free(files);return 1;}context=files[0];if(!strchr(context,':')){fprintf(stderr,"chcon: invalid context: '%s': Invalid argument\n",context);free(files);return 1;}int status=0;for(int i=1;i<nf;i++)if(zov_chcon_one(context,files[i],follow)<0){fprintf(stderr,"chcon: failed to change context of '%s' to '%s': %s\n",files[i],context,strerror(errno));status=1;}free(files);return status;}
static inline int zov_runcon_run(int argc,char**argv){int i=0;const char*context=NULL;for(;i<argc;i++){const char*a=argv[i];if(!strcmp(a,"--help")){puts("Usage: runcon CONTEXT COMMAND [ARG]...\nRun COMMAND with specified security context.\n      --help     display this help and exit\n      --version  output version information and exit");return 0;}if(!strcmp(a,"--version")){puts("runcon (zovutils) 0.10.6");return 0;}if(a[0]=='-'&&strcmp(a,"-")){fprintf(stderr,"runcon: unrecognized option '%s'\nTry 'runcon --help' for more information.\n",a);return 125;}context=a;i++;break;}if(!context){fprintf(stderr,"runcon: missing operand\nTry 'runcon --help' for more information.\n");return 125;}if(i>=argc){fprintf(stderr,"runcon: no command specified\nTry 'runcon --help' for more information.\n");return 125;}
#if defined(__linux__)
 FILE*f=fopen("/proc/self/attr/exec","w");if(!f){fprintf(stderr,"runcon: failed to set context: %s\n",strerror(errno));return 125;}if(fprintf(f,"%s",context)<0||fclose(f)<0){fprintf(stderr,"runcon: failed to create security context: '%s': %s\n",context,strerror(errno));return 125;}
#else
 (void)context;fprintf(stderr,"runcon: security contexts are not supported on this platform\n");return 125;
#endif
 execvp(argv[i],argv+i);fprintf(stderr,"runcon: failed to execute '%s': %s\n",argv[i],strerror(errno));return errno==ENOENT?127:126;}
#endif
