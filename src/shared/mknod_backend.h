#ifndef ZOVUTILS_MKNOD_BACKEND_H
#define ZOVUTILS_MKNOD_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/sysmacros.h>
#elif defined(__APPLE__)
#include <sys/mkdev.h>
#endif
static inline int zov_mknod_run(int argc,char**argv){mode_t mode=0666;int have_mode=0,i=0;for(;i<argc;i++){const char*a=argv[i];if(!strcmp(a,"--help")){puts("Usage: mknod [OPTION]... NAME TYPE [MAJOR MINOR]\nCreate the special file NAME of the given TYPE.\n  -m, --mode=MODE  set file permission bits to MODE, not a=rw - umask\n  TYPE: b block device, c or u character device, p FIFO\n      --help       display this help and exit\n      --version    output version information and exit");return 0;}if(!strcmp(a,"--version")){puts("mknod (zovutils) 0.10.3");return 0;}if(!strcmp(a,"-m")||!strcmp(a,"--mode")){if(i+1>=argc){fprintf(stderr,"mknod: option requires an argument -- 'm'\n");return 1;}char*e;unsigned long v=strtoul(argv[++i],&e,8);if(*e||v>07777){fprintf(stderr,"mknod: invalid mode: '%s'\n",argv[i]);return 1;}mode=(mode_t)v;have_mode=1;continue;}if(!strncmp(a,"--mode=",7)){char*e;unsigned long v=strtoul(a+7,&e,8);if(*e||v>07777){fprintf(stderr,"mknod: invalid mode: '%s'\n",a+7);return 1;}mode=(mode_t)v;have_mode=1;continue;}if(a[0]=='-'&&strcmp(a,"-")){fprintf(stderr,"mknod: unrecognized option '%s'\nTry 'mknod --help' for more information.\n",a);return 1;}break;}int left=argc-i;if(left<2){fprintf(stderr,"mknod: missing operand\nTry 'mknod --help' for more information.\n");return 1;}const char*name=argv[i],*type=argv[i+1];if(strlen(type)!=1||!strchr("bcup",type[0])){fprintf(stderr,"mknod: invalid device type '%s'\n",type);return 1;}if(type[0]=='p'){if(left!=2){fprintf(stderr,"mknod: extra operand '%s'\nTry 'mknod --help' for more information.\n",argv[i+2]);return 1;}mode_t m=S_IFIFO|(mode&07777);if(mkfifo(name,m)<0){fprintf(stderr,"mknod: failed to create FIFO '%s': %s\n",name,strerror(errno));return 1;}return 0;}if(left!=4){fprintf(stderr,"mknod: missing operand\nTry 'mknod --help' for more information.\n");return 1;}char*e1,*e2;unsigned long maj=strtoul(argv[i+2],&e1,10),min=strtoul(argv[i+3],&e2,10);if(*e1||*e2||maj>0xffffffffUL||min>0xffffffffUL){fprintf(stderr,"mknod: invalid major/minor device number\n");return 1;}
#if defined(__linux__) || defined(__APPLE__)
mode_t kind=(type[0]=='b'?S_IFBLK:S_IFCHR);if(mknod(name,kind|(mode&07777),makedev(maj,min))<0){fprintf(stderr,"mknod: failed to create device '%s': %s\n",name,strerror(errno));return 1;}return 0;
#else
(void)have_mode;errno=ENOSYS;fprintf(stderr,"mknod: device nodes are not supported on this platform\n");return 1;
#endif
}
#endif
