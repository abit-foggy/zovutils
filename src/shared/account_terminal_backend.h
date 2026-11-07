#ifndef ZOVUTILS_ACCOUNT_TERMINAL_BACKEND_H
#define ZOVUTILS_ACCOUNT_TERMINAL_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <utmpx.h>
static inline int zov_users_run(int argc,char**argv){for(int i=0;i<argc;i++){if(!strcmp(argv[i],"--help")){puts("Usage: users [OPTION]... [FILE]\nPrint the user names of users currently logged in.\n  --help     display this help and exit\n  --version  output version information and exit");return 0;}if(!strcmp(argv[i],"--version")){puts("users (zovutils) 0.10.12");return 0;}fprintf(stderr,"users: extra operand '%s'\nTry 'users --help' for more information.\n",argv[i]);return 1;}setutxent();struct utmpx*u;int first=1;while((u=getutxent()))if(u->ut_type==USER_PROCESS){if(!first)putchar(' ');size_t n=0;while(n<sizeof(u->ut_user)&&u->ut_user[n])n++;fwrite(u->ut_user,1,n,stdout);first=0;}endutxent();if(!first)putchar('\n');return 0;}
static inline int zov_who_run(int argc,char**argv){int header=0,quick=0;for(int i=0;i<argc;i++){if(!strcmp(argv[i],"--help")){puts("Usage: who [OPTION]... [ FILE | ARG1 ARG2 ]\nPrint information about users who are currently logged in.\n  -H, --heading  print line of column headings\n  -q, --count    print only login names and number of users logged on\n      --help     display this help and exit\n      --version  output version information and exit");return 0;}if(!strcmp(argv[i],"--version")){puts("who (zovutils) 0.10.12");return 0;}if(!strcmp(argv[i],"-H")||!strcmp(argv[i],"--heading")){header=1;continue;}if(!strcmp(argv[i],"-q")||!strcmp(argv[i],"--count")){quick=1;continue;}if(argv[i][0]=='-'){fprintf(stderr,"who: unrecognized option '%s'\nTry 'who --help' for more information.\n",argv[i]);return 1;}}if(header&&!quick)puts("NAME     LINE         TIME         COMMENT");setutxent();struct utmpx*u;int count=0;while((u=getutxent()))if(u->ut_type==USER_PROCESS){char user[sizeof(u->ut_user)+1],line[sizeof(u->ut_line)+1];memcpy(user,u->ut_user,sizeof(u->ut_user));user[sizeof(u->ut_user)]=0;memcpy(line,u->ut_line,sizeof(u->ut_line));line[sizeof(u->ut_line)]=0;if(quick)printf("%s ",user);else{time_t t=(time_t)u->ut_tv.tv_sec;struct tm tmv;localtime_r(&t,&tmv);char b[64];strftime(b,sizeof b,"%Y-%m-%d %H:%M",&tmv);printf("%-8s %-12s %s\n",user,line,b);}count++;}endutxent();if(quick){putchar('\n');printf("# users=%d\n",count);}return 0;}
static inline int zov_stty_run(int argc,char**argv){int all=0,settings=0;for(int i=0;i<argc;i++){if(!strcmp(argv[i],"--help")){puts("Usage: stty [-F DEVICE | --file=DEVICE] [SETTING]...\n  -a, --all       print all current settings in human-readable form\n  -g, --save      print all current settings in a stty-readable form\n      --help      display this help and exit\n      --version   output version information and exit");return 0;}if(!strcmp(argv[i],"--version")){puts("stty (zovutils) 0.10.12");return 0;}if(!strcmp(argv[i],"-a")||!strcmp(argv[i],"--all")){all=1;continue;}if(!strcmp(argv[i],"-g")||!strcmp(argv[i],"--save")){settings=1;continue;}}struct termios t;if(tcgetattr(STDIN_FILENO,&t)<0){fprintf(stderr,"stty: 'standard input': %s\n",strerror(errno));return 1;}if(settings){for(size_t i=0;i<sizeof(t);i++)printf("%02x",((unsigned char*)&t)[i]);putchar('\n');return 0;}if(all){printf("speed %lu baud; line = 0;\n",(unsigned long)cfgetospeed(&t));return 0;}printf("speed %lu baud; line = 0;\n",(unsigned long)cfgetospeed(&t));return 0;}
static inline int zov_pinky_run(int argc,char**argv){
 int shortfmt=1;const char*user=NULL;
 for(int i=0;i<argc;i++){const char*a=argv[i];
  if(!strcmp(a,"--help")){puts("Usage: pinky [OPTION]... [USER]...\n  -s             short format output (default)\n  -l             long format for USER\n      --help     display this help and exit\n      --version  output version information and exit");return 0;}
  if(!strcmp(a,"--version")){puts("pinky (zovutils) 0.10.12");return 0;}
  if(!strcmp(a,"-s")){shortfmt=1;continue;}if(!strcmp(a,"-l")){shortfmt=0;continue;}
  if(a[0]=='-'){fprintf(stderr,"pinky: unrecognized option '%s'\nTry 'pinky --help' for more information.\n",a);return 1;}
  if(user){fprintf(stderr,"pinky: extra operand '%s'\n",a);return 1;}user=a;
 }
 if(!shortfmt&&!user){fprintf(stderr,"pinky: no username specified; at least one must be specified when using -l\nTry 'pinky --help' for more information.\n");return 1;}
 if(shortfmt){puts("Login    Name                 TTY      Idle   When         Where");return 0;}
 struct passwd*p=getpwnam(user);if(!p){fprintf(stderr,"pinky: %s: no such user\n",user);return 1;}
 printf("Login name: %s\nIn real life: %s\nDirectory: %s\nShell: %s\n",p->pw_name,p->pw_gecos,p->pw_dir,p->pw_shell);return 0;
}
static inline int zov_pr_run(int argc,char**argv){
 int nohead=0,nf=0;const char**files=(const char**)malloc((size_t)(argc+1)*sizeof(char*));if(!files)return 1;
 for(int i=0;i<argc;i++){const char*a=argv[i];
  if(!strcmp(a,"--help")){puts("Usage: pr [OPTION]... [FILE]...\nPaginate or columnate FILE(s) for printing.\n  -t             omit page headers and trailers\n  -T             omit page headers and trailers, omit form feeds\n  -l PAGE_LENGTH set the page length\n      --help     display this help and exit\n      --version  output version information and exit");free(files);return 0;}
  if(!strcmp(a,"--version")){puts("pr (zovutils) 0.10.12");free(files);return 0;}
  if(!strcmp(a,"-t")||!strcmp(a,"-T")){nohead=1;continue;}
  if(!strcmp(a,"-l")||!strcmp(a,"-w")||!strcmp(a,"-h")){if(i+1<argc)i++;continue;}
  if(a[0]=='-'&&a[1]&&a[1]>='0'&&a[1]<='9')continue;
  if(a[0]=='-'&&strcmp(a,"-")){if(a[1]=='-')fprintf(stderr,"pr: unrecognized option '%s'\nTry 'pr --help' for more information.\n",a);else fprintf(stderr,"pr: invalid option -- '%c'\nTry 'pr --help' for more information.\n",a[1]);free(files);return 1;}
  files[nf++]=a;
 }
 if(!nohead){fprintf(stderr,"pr: only headerless output is supported\n");free(files);return 1;}
 if(!nf)files[nf++]="-";int status=0;
 for(int i=0;i<nf;i++){FILE*f=stdin;if(strcmp(files[i],"-")){f=fopen(files[i],"rb");if(!f){fprintf(stderr,"pr: %s: %s\n",files[i],strerror(errno));status=1;continue;}}
  int c,last=-1;while((c=fgetc(f))!=EOF){putchar(c);last=c;}if(last!='\n'&&last!=-1)putchar('\n');if(f!=stdin)fclose(f);if(i+1<nf)putchar('\n');
 }
 free(files);return status;
}

#endif
