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
#include <sys/stat.h>
#include <limits.h>
#include <utmpx.h>
static inline int zov_users_run(int argc,char**argv){for(int i=0;i<argc;i++){if(!strcmp(argv[i],"--help")){puts("Usage: users [OPTION]... [FILE]\nPrint the user names of users currently logged in.\n  --help     display this help and exit\n  --version  output version information and exit");return 0;}if(!strcmp(argv[i],"--version")){puts("users (zovutils) 0.10.32");return 0;}fprintf(stderr,"users: extra operand '%s'\nTry 'users --help' for more information.\n",argv[i]);return 1;}setutxent();struct utmpx*u;int first=1;while((u=getutxent()))if(u->ut_type==USER_PROCESS){if(!first)putchar(' ');size_t n=0;while(n<sizeof(u->ut_user)&&u->ut_user[n])n++;fwrite(u->ut_user,1,n,stdout);first=0;}endutxent();if(!first)putchar('\n');return 0;}
static inline int zov_who_run(int argc,char**argv){int header=0,quick=0;for(int i=0;i<argc;i++){if(!strcmp(argv[i],"--help")){puts("Usage: who [OPTION]... [ FILE | ARG1 ARG2 ]\nPrint information about users who are currently logged in.\n  -H, --heading  print line of column headings\n  -q, --count    print only login names and number of users logged on\n      --help     display this help and exit\n      --version  output version information and exit");return 0;}if(!strcmp(argv[i],"--version")){puts("who (zovutils) 0.10.32");return 0;}if(!strcmp(argv[i],"-H")||!strcmp(argv[i],"--heading")){header=1;continue;}if(!strcmp(argv[i],"-q")||!strcmp(argv[i],"--count")){quick=1;continue;}if(argv[i][0]=='-'){fprintf(stderr,"who: unrecognized option '%s'\nTry 'who --help' for more information.\n",argv[i]);return 1;}}if(header&&!quick)puts("NAME     LINE         TIME         COMMENT");setutxent();struct utmpx*u;int count=0;while((u=getutxent()))if(u->ut_type==USER_PROCESS){char user[sizeof(u->ut_user)+1],line[sizeof(u->ut_line)+1];memcpy(user,u->ut_user,sizeof(u->ut_user));user[sizeof(u->ut_user)]=0;memcpy(line,u->ut_line,sizeof(u->ut_line));line[sizeof(u->ut_line)]=0;if(quick)printf("%s%s",count?" ":"",user);else{time_t t=(time_t)u->ut_tv.tv_sec;struct tm tmv;localtime_r(&t,&tmv);char b[64];strftime(b,sizeof b,"%b %e %H:%M",&tmv);printf("%-8s %-12s %s\n",user,line,b);}count++;}endutxent();if(quick){putchar('\n');printf("# users=%d\n",count);}return 0;}
static inline int zov_stty_run(int argc,char**argv){int all=0,settings=0;for(int i=0;i<argc;i++){if(!strcmp(argv[i],"--help")){puts("Usage: stty [-F DEVICE | --file=DEVICE] [SETTING]...\n  -a, --all       print all current settings in human-readable form\n  -g, --save      print all current settings in a stty-readable form\n      --help      display this help and exit\n      --version   output version information and exit");return 0;}if(!strcmp(argv[i],"--version")){puts("stty (zovutils) 0.10.32");return 0;}if(!strcmp(argv[i],"-a")||!strcmp(argv[i],"--all")){all=1;continue;}if(!strcmp(argv[i],"-g")||!strcmp(argv[i],"--save")){settings=1;continue;}}struct termios t;if(tcgetattr(STDIN_FILENO,&t)<0){fprintf(stderr,"stty: 'standard input': %s\n",strerror(errno));return 1;}if(settings){for(size_t i=0;i<sizeof(t);i++)printf("%02x",((unsigned char*)&t)[i]);putchar('\n');return 0;}if(all){printf("speed %lu baud; line = 0;\n",(unsigned long)cfgetospeed(&t));return 0;}printf("speed %lu baud; line = 0;\n",(unsigned long)cfgetospeed(&t));return 0;}
static inline int zov_pinky_run(int argc,char**argv){
 int shortfmt=1,header=1,show_name=1,show_host=1,show_idle=1,omit_home=0,omit_project=0,omit_plan=0;const char*users[4096];int nusers=0;
 for(int i=0;i<argc;i++){const char*a=argv[i];
  if(!strcmp(a,"--help")){puts("Usage: pinky [OPTION]... [USER]...\n  -l             produce long format output for USERs\n  -b             omit home directory and shell in long format\n  -h             omit project file in long format\n  -p             omit plan file in long format\n  -s             do short format output (default)\n  -f             omit headings in short format\n  -w             omit full name in short format\n  -i             omit full name and remote host in short format\n  -q             omit full name, remote host and idle time in short format\n      --lookup   attempt to canonicalize hostnames via DNS\n      --help     display this help and exit\n      --version  output version information and exit");return 0;}
  if(!strcmp(a,"--version")){puts("pinky (zovutils) 0.10.32");return 0;}
  if(!strcmp(a,"--lookup"))continue;
  if(a[0]=='-'&&a[1]&&a[1]!='-'){int valid=1;for(size_t j=1;a[j];j++){if(a[j]=='s')shortfmt=1;else if(a[j]=='l')shortfmt=0;else if(a[j]=='f')header=0;else if(a[j]=='w')show_name=0;else if(a[j]=='i'){show_name=0;show_host=0;}else if(a[j]=='q'){show_name=0;show_host=0;show_idle=0;}else if(a[j]=='b')omit_home=1;else if(a[j]=='h')omit_project=1;else if(a[j]=='p')omit_plan=1;else valid=0;}if(valid)continue;}
  if(a[0]=='-'){fprintf(stderr,"pinky: unrecognized option '%s'\nTry 'pinky --help' for more information.\n",a);return 1;}
  if(nusers>=4096){fprintf(stderr,"pinky: too many users\n");return 1;}users[nusers++]=a;
 }
 if(!shortfmt&&!nusers){fprintf(stderr,"pinky: no username specified; at least one must be specified when using -l\nTry 'pinky --help' for more information.\n");return 1;}
 if(shortfmt){if(header){if(show_name)puts("Login    Name                 TTY      Idle   When         Where");else{fputs("Login     TTY      ",stdout);if(show_idle)fputs("Idle   ",stdout);printf("%-12s", "When");if(show_host)fputs(" Where",stdout);putchar('\n');}}setutxent();struct utmpx*u;time_t now=time(NULL);while((u=getutxent()))if(u->ut_type==USER_PROCESS){char login[sizeof(u->ut_user)+1],line[sizeof(u->ut_line)+1],host[sizeof(u->ut_host)+1],realname[256],idle[32]="",when[32]="",device[PATH_MAX];size_t n=0;while(n<sizeof(u->ut_user)&&u->ut_user[n])n++;memcpy(login,u->ut_user,n);login[n]=0;if(nusers){int matched=0;for(int k=0;k<nusers;k++)if(!strcmp(users[k],login))matched=1;if(!matched)continue;}n=0;while(n<sizeof(u->ut_line)&&u->ut_line[n])n++;memcpy(line,u->ut_line,n);line[n]=0;n=0;while(n<sizeof(u->ut_host)&&u->ut_host[n])n++;memcpy(host,u->ut_host,n);host[n]=0;struct passwd*p=getpwnam(login);snprintf(realname,sizeof(realname),"%s",p&&p->pw_gecos?p->pw_gecos:login);char*comma=strchr(realname,',');if(comma)*comma=0;if(!show_name)realname[0]=0;if(!show_host)host[0]=0;struct tm tmv;time_t logged=(time_t)u->ut_tv.tv_sec;if(localtime_r(&logged,&tmv))strftime(when,sizeof(when),"%b %e %H:%M",&tmv);snprintf(device,sizeof(device),"/dev/%s",line);struct stat st;if(show_idle&&now>=logged&&stat(device,&st)==0){long long seconds=(long long)now-(long long)st.st_atime;if(seconds>=60){long long minutes=seconds/60;if(minutes>=1440)snprintf(idle,sizeof(idle),"%lldd",minutes/1440);else snprintf(idle,sizeof(idle),"%02lld:%02lld",minutes/60,minutes%60);}}char tty[sizeof(line)+2];snprintf(tty,sizeof(tty),"%s%s",(!strcmp(line,"console")||access(device,W_OK)!=0)?"*":"",line);if(show_name)printf("%-8.8s %-19.19s %-8.8s  %-5.5s  %-12.12s",login,realname,tty,idle,when);else if(show_idle)printf("%-8.8s %-8.8s  %-6.6s %-12.12s",login,tty,idle,when);else printf("%-8.8s %-8.8s  %-12.12s",login,tty,when);if(*host)printf(" %s",host);putchar('\n');}endutxent();return 0;}
 for(int i=0;i<nusers;i++){struct passwd*p=getpwnam(users[i]);if(!p){fprintf(stderr,"pinky: %s: no such user\n",users[i]);return 1;}printf("Login name: %-28sIn real life:  %s\n",p->pw_name,p->pw_gecos?p->pw_gecos:"");if(!omit_home)printf("Directory: %-29sShell:  %s\n",p->pw_dir?p->pw_dir:"",p->pw_shell?p->pw_shell:"");(void)omit_project;(void)omit_plan;putchar('\n');}return 0;
}
static inline int zov_pr_run(int argc,char**argv){
 int nohead=0,nf=0;const char**files=(const char**)malloc((size_t)(argc+1)*sizeof(char*));if(!files)return 1;
 for(int i=0;i<argc;i++){const char*a=argv[i];
  if(!strcmp(a,"--help")){puts("Usage: pr [OPTION]... [FILE]...\nPaginate or columnate FILE(s) for printing.\n  -t             omit page headers and trailers\n  -T             omit page headers and trailers, omit form feeds\n  -l PAGE_LENGTH set the page length\n      --help     display this help and exit\n      --version  output version information and exit");free(files);return 0;}
  if(!strcmp(a,"--version")){puts("pr (zovutils) 0.10.32");free(files);return 0;}
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
