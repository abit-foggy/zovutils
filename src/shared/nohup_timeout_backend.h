#ifndef ZOVUTILS_NOHUP_TIMEOUT_BACKEND_H
#define ZOVUTILS_NOHUP_TIMEOUT_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
static inline int zov_nohup_run(int argc,char**argv){int i=0;for(;i<argc;i++){if(!strcmp(argv[i],"--help")){puts("Usage: nohup COMMAND [ARG]...\nRun COMMAND, ignoring hangup signals.\n      --help     display this help and exit\n      --version  output version information and exit");return 0;}if(!strcmp(argv[i],"--version")){puts("nohup (zovutils) 0.10.59");return 0;}if(argv[i][0]=='-'&&strcmp(argv[i],"-")){fprintf(stderr,"nohup: unrecognized option '%s'\nTry 'nohup --help' for more information.\n",argv[i]);return 125;}break;}if(i>=argc){fprintf(stderr,"nohup: missing operand\nTry 'nohup --help' for more information.\n");return 125;}signal(SIGHUP,SIG_IGN);signal(SIGQUIT,SIG_IGN);if(isatty(STDIN_FILENO)){int fd=open("/dev/null",O_RDONLY);if(fd>=0){dup2(fd,STDIN_FILENO);close(fd);}}if(isatty(STDOUT_FILENO)){int fd=open("nohup.out",O_WRONLY|O_CREAT|O_APPEND,0600);const char*name="nohup.out";if(fd<0){const char*h=getenv("HOME");if(h){size_t n=strlen(h)+12;char*p=(char*)malloc(n);if(p){snprintf(p,n,"%s/nohup.out",h);fd=open(p,O_WRONLY|O_CREAT|O_APPEND,0600);if(fd>=0)name=p;else free(p);}}}if(fd<0){fprintf(stderr,"nohup: failed to open 'nohup.out': %s\n",strerror(errno));return 125;}dup2(fd,STDOUT_FILENO);if(isatty(STDERR_FILENO))dup2(fd,STDERR_FILENO);close(fd);fprintf(stderr,"nohup: appending output to '%s'\n",name);}else if(isatty(STDERR_FILENO)){dup2(STDOUT_FILENO,STDERR_FILENO);}execvp(argv[i],argv+i);fprintf(stderr,"nohup: failed to run command '%s': %s\n",argv[i],strerror(errno));return errno==ENOENT?127:126;}
static inline int zov_timeout_signal(const char*s){if(!strcmp(s,"TERM")||!strcmp(s,"SIGTERM"))return SIGTERM;if(!strcmp(s,"KILL")||!strcmp(s,"SIGKILL"))return SIGKILL;if(!strcmp(s,"INT")||!strcmp(s,"SIGINT"))return SIGINT;if(!strcmp(s,"HUP")||!strcmp(s,"SIGHUP"))return SIGHUP;if(!strcmp(s,"QUIT")||!strcmp(s,"SIGQUIT"))return SIGQUIT;if(!strcmp(s,"ALRM")||!strcmp(s,"SIGALRM"))return SIGALRM;if(!strcmp(s,"USR1")||!strcmp(s,"SIGUSR1"))return SIGUSR1;if(!strcmp(s,"USR2")||!strcmp(s,"SIGUSR2"))return SIGUSR2;char*e;long n=strtol(s,&e,10);return *e||n<=0||n>=NSIG?-1:(int)n;}
static inline int zov_timeout_duration(const char *value,double *seconds) {
    char *end=NULL;errno=0;double amount=strtod(value,&end);
    if(errno||end==value||amount<0)return 0;
    double scale=1.0;
    if(*end=='s')end++;else if(*end=='m'){scale=60;end++;}else if(*end=='h'){scale=3600;end++;}else if(*end=='d'){scale=86400;end++;}
    if(*end||amount*scale>86400.0*365.0)return 0;
    *seconds=amount*scale;return 1;
}
static inline const char *zov_timeout_signal_name(int sig) {
    switch(sig){case SIGTERM:return "TERM";case SIGKILL:return "KILL";case SIGINT:return "INT";case SIGHUP:return "HUP";case SIGQUIT:return "QUIT";case SIGALRM:return "ALRM";case SIGUSR1:return "USR1";case SIGUSR2:return "USR2";default:return "signal";}
}
static inline int zov_timeout_wait_status(int st) {
    if(WIFEXITED(st))return WEXITSTATUS(st);
    if(WIFSIGNALED(st))return 128+WTERMSIG(st);
    return 125;
}
static inline void zov_timeout_sleep(void) {
    struct timespec pause={0,10000000};while(nanosleep(&pause,&pause)<0&&errno==EINTR){}
}
static inline int zov_timeout_run(int argc,char**argv) {
    int sig=SIGTERM,i=0,preserve=0,verbose=0,foreground=0;
    double kill_after=-1.0;
    for(;i<argc;i++){
        const char*a=argv[i];
        if(!strcmp(a,"--help")){puts("Usage: timeout [OPTION] DURATION COMMAND [ARG]...\nStart COMMAND, and kill it if still running after DURATION.\n  -f, --foreground       do not create a separate process group\n  -k, --kill-after=DURATION  send KILL if COMMAND is still running\n  -p, --preserve-status  exit with the same status as COMMAND\n  -s, --signal=SIGNAL    specify the signal to send on timeout\n  -v, --verbose          diagnose the signal sent\n      --help             display this help and exit\n      --version          output version information and exit");return 0;}
        if(!strcmp(a,"--version")){puts("timeout (zovutils) 0.10.59");return 0;}
        if(!strcmp(a,"-p")||!strcmp(a,"--preserve-status")){preserve=1;continue;}
        if(!strcmp(a,"-f")||!strcmp(a,"--foreground")){foreground=1;continue;}
        if(!strcmp(a,"-v")||!strcmp(a,"--verbose")){verbose=1;continue;}
        if(!strcmp(a,"-s")||!strcmp(a,"--signal")){if(i+1>=argc){fprintf(stderr,"timeout: option requires an argument -- 's'\nTry 'timeout --help' for more information.\n");return 125;}sig=zov_timeout_signal(argv[++i]);if(sig<0){fprintf(stderr,"timeout: invalid signal '%s'\n",argv[i]);return 125;}continue;}
        if(!strncmp(a,"--signal=",9)){sig=zov_timeout_signal(a+9);if(sig<0){fprintf(stderr,"timeout: invalid signal '%s'\n",a+9);return 125;}continue;}
        if(!strcmp(a,"-k")||!strcmp(a,"--kill-after")){if(i+1>=argc||!zov_timeout_duration(argv[++i],&kill_after)){fprintf(stderr,"timeout: invalid time interval '%s'\n",i<argc?argv[i]:"");return 125;}continue;}
        if(!strncmp(a,"--kill-after=",13)){if(!zov_timeout_duration(a+13,&kill_after)){fprintf(stderr,"timeout: invalid time interval '%s'\n",a+13);return 125;}continue;}
        if(a[0]=='-'&&strcmp(a,"-")){fprintf(stderr,"timeout: unrecognized option '%s'\nTry 'timeout --help' for more information.\n",a);return 125;}
        break;
    }
    if(i+1>=argc){fprintf(stderr,"timeout: missing operand\nTry 'timeout --help' for more information.\n");return 125;}
    double seconds;
    if(!zov_timeout_duration(argv[i],&seconds)){fprintf(stderr,"timeout: invalid time interval '%s'\nTry 'timeout --help' for more information.\n",argv[i]);return 125;}
    pid_t pid=fork();
    if(pid<0){fprintf(stderr,"timeout: fork: %s\n",strerror(errno));return 125;}
    if(pid==0){if(!foreground)setpgid(0,0);execvp(argv[i+1],argv+i+1);fprintf(stderr,"timeout: failed to run command '%s': %s\n",argv[i+1],strerror(errno));_exit(errno==ENOENT?127:126);}
    if(!foreground)setpgid(pid,pid);
    double waited=0.0;int st=0;
    for(;;){pid_t r=waitpid(pid,&st,WNOHANG);if(r==pid)return zov_timeout_wait_status(st);if(r<0){if(errno==EINTR)continue;return 125;}if(waited>=seconds)break;zov_timeout_sleep();waited+=0.01;}
    if(verbose)fprintf(stderr,"timeout: sending signal %s to command '%s'\n",zov_timeout_signal_name(sig),argv[i+1]);
    pid_t target=foreground?pid:-pid;
    if(kill(target,sig)<0&&errno!=ESRCH)return 125;
    double grace=0.0;
    if(kill_after>=0.0){
        for(;;){pid_t r=waitpid(pid,&st,WNOHANG);if(r==pid)return preserve?zov_timeout_wait_status(st):124;if(r<0){if(errno==EINTR)continue;return 125;}if(grace>=kill_after)break;zov_timeout_sleep();grace+=0.01;}
        if(verbose)fprintf(stderr,"timeout: sending signal KILL to command '%s'\n",argv[i+1]);
        kill(target,SIGKILL);
    }
    while(waitpid(pid,&st,0)<0){if(errno==EINTR)continue;return 125;}
    return preserve?zov_timeout_wait_status(st):124;
}
#endif
