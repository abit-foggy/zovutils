#ifndef ZOVUTILS_IDENTITY_BACKEND_H
#define ZOVUTILS_IDENTITY_BACKEND_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
static inline int zov_whoami_run(int argc,char**argv){for(int i=0;i<argc;i++){if(!strcmp(argv[i],"--help")){puts("Usage: whoami [OPTION]...\nPrint the user name associated with the current effective user ID.\n  --help     display this help and exit\n  --version  output version information and exit");return 0;}if(!strcmp(argv[i],"--version")){puts("whoami (zovutils) 0.10.61");return 0;}if(argv[i][0]=='-')fprintf(stderr,"whoami: unrecognized option '%s'\nTry 'whoami --help' for more information.\n",argv[i]);else fprintf(stderr,"whoami: extra operand '%s'\nTry 'whoami --help' for more information.\n",argv[i]);return 1;}struct passwd*p=getpwuid(geteuid());if(!p){fprintf(stderr,"whoami: cannot find name for user ID %lu\n",(unsigned long)geteuid());return 1;}puts(p->pw_name);return 0;}
#if defined(__APPLE__)
#include <mach-o/arch.h>
#include <sys/sysctl.h>
static inline const char *zov_uname_processor(void) {
    cpu_type_t cputype;
    size_t size = sizeof(cputype);
    if (sysctlbyname("hw.cputype", &cputype, &size, NULL, 0) == 0) {
        const NXArchInfo *ai = NXGetArchInfoFromCpuType(cputype, CPU_SUBTYPE_MULTIPLE);
        if (ai && ai->name) return ai->name;
    }
#if defined(__x86_64__) || defined(__i386__)
    return "i386";
#elif defined(__aarch64__) || defined(__arm64__) || defined(__arm__)
    return "arm";
#else
    return "unknown";
#endif
}
#define ZOV_UNAME_OS "Darwin"
#else
static inline const char *zov_uname_processor(void) { return "unknown"; }
#define ZOV_UNAME_OS "GNU/Linux"
#endif
static inline int zov_uname_run(int argc,char**argv){struct utsname u;if(uname(&u)!=0){fprintf(stderr,"uname: cannot get system name: %s\n",strerror(errno));return 1;}int fields[8]={0},all=0,explicit=0;for(int i=0;i<argc;i++){const char*a=argv[i];if(!strcmp(a,"--help")){puts("Usage: uname [OPTION]...\nPrint certain system information.\n  -a, --all                print all information, in the following order,\n                             except omit -p and -i if unknown\n  -s, --kernel-name        print the kernel name\n  -n, --nodename           print the network node hostname\n  -r, --kernel-release     print the kernel release\n  -v, --kernel-version     print the kernel version\n  -m, --machine            print the machine hardware name\n  -p, --processor          print the processor type (non-portable)\n  -i, --hardware-platform  print the hardware platform (non-portable)\n  -o, --operating-system   print the operating system\n      --help               display this help and exit\n      --version            output version information and exit");return 0;}if(!strcmp(a,"--version")){puts("uname (zovutils) 0.10.61");return 0;}if(a[0]!='-'||!a[1]){fprintf(stderr,"uname: extra operand '%s'\nTry 'uname --help' for more information.\n",a);return 1;}if(!strcmp(a,"--all")){all=explicit=1;continue;}if(!strcmp(a,"--kernel-name")){fields[0]=explicit=1;continue;}if(!strcmp(a,"--nodename")){fields[1]=explicit=1;continue;}if(!strcmp(a,"--kernel-release")){fields[2]=explicit=1;continue;}if(!strcmp(a,"--kernel-version")){fields[3]=explicit=1;continue;}if(!strcmp(a,"--machine")){fields[4]=explicit=1;continue;}if(!strcmp(a,"--processor")){fields[5]=explicit=1;continue;}if(!strcmp(a,"--hardware-platform")){fields[6]=explicit=1;continue;}if(!strcmp(a,"--operating-system")){fields[7]=explicit=1;continue;}if(a[1]=='-'&&a[2]){fprintf(stderr,"uname: unrecognized option '%s'\nTry 'uname --help' for more information.\n",a);return 1;}for(size_t j=1;a[j];j++){if(a[j]=='a'){all=explicit=1;continue;}int k=a[j]=='s'?0:a[j]=='n'?1:a[j]=='r'?2:a[j]=='v'?3:a[j]=='m'?4:a[j]=='p'?5:a[j]=='i'?6:a[j]=='o'?7:-1;if(k<0){fprintf(stderr,"uname: invalid option -- '%c'\nTry 'uname --help' for more information.\n",a[j]);return 1;}fields[k]=1;explicit=1;}}const char*processor=zov_uname_processor();const char*vals[8]={u.sysname,u.nodename,u.release,u.version,u.machine,processor,"unknown",ZOV_UNAME_OS};if(!explicit){fields[0]=1;}else if(all){for(int i=0;i<5;i++)fields[i]=1;if(strcmp(processor,"unknown")!=0)fields[5]=1;fields[7]=1;}int first=1;for(int i=0;i<8;i++)if(fields[i]){if(!first)putchar(' ');fputs(vals[i],stdout);first=0;}putchar('\n');return 0;}
#endif
