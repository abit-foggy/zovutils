#ifndef ZOVUTILS_HOSTID_BACKEND_H
#define ZOVUTILS_HOSTID_BACKEND_H
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static inline int zov_hostid_run(int argc,char**argv){for(int i=0;i<argc;i++){if(!strcmp(argv[i],"--help")){puts("Usage: hostid [OPTION]\nPrint the numeric identifier for the current host.\n  --help     display this help and exit\n  --version  output version information and exit");return 0;}if(!strcmp(argv[i],"--version")){puts("hostid (zovutils) 0.10.60");return 0;}if(argv[i][0]=='-')fprintf(stderr,"hostid: unrecognized option '%s'\nTry 'hostid --help' for more information.\n",argv[i]);else fprintf(stderr,"hostid: extra operand '%s'\nTry 'hostid --help' for more information.\n",argv[i]);return 1;}printf("%08lx\n",(unsigned long)gethostid()&0xffffffffUL);return 0;}
#endif
