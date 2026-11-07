#ifndef ZOVUTILS_CHROOT_BACKEND_H
#define ZOVUTILS_CHROOT_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
static inline int zov_chroot_run(int argc,char**argv){int i=0;while(i<argc&&argv[i][0]=='-'&&strcmp(argv[i],"-")){if(!strcmp(argv[i],"--help")){puts("Usage: chroot NEWROOT [COMMAND [ARG]...]\nRun COMMAND with root directory set to NEWROOT.\n      --help     display this help and exit\n      --version  output version information and exit");return 0;}if(!strcmp(argv[i],"--version")){puts("chroot (zovutils) 0.10.14");return 0;}fprintf(stderr,"chroot: unrecognized option '%s'\nTry 'chroot --help' for more information.\n",argv[i]);return 125;}if(i>=argc){fprintf(stderr,"chroot: missing operand\nTry 'chroot --help' for more information.\n");return 125;}const char*root=argv[i++];if(chroot(root)<0){fprintf(stderr,"chroot: cannot change root directory to '%s': %s\n",root,strerror(errno));return 125;}if(chdir("/")<0){fprintf(stderr,"chroot: cannot change directory to '/': %s\n",strerror(errno));return 125;}if(i<argc)execvp(argv[i],argv+i);else{const char*shell=getenv("SHELL");if(!shell||!*shell)shell="/bin/sh";execl(shell,shell,(char*)NULL);}fprintf(stderr,"chroot: failed to run command '%s': %s\n",i<argc?argv[i]:"/bin/sh",strerror(errno));return errno==ENOENT?127:126;}
#endif
