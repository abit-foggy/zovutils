#ifndef ZOVUTILS_GROUPS_BACKEND_H
#define ZOVUTILS_GROUPS_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
static inline void zov_group_print(gid_t g,int numeric){struct group*p=numeric?NULL:getgrgid(g);if(p)fputs(p->gr_name,stdout);else printf("%lu",(unsigned long)g);}
static inline int zov_groups_run(int argc,char**argv){int numeric=0;const char*user=NULL;for(int i=0;i<argc;i++){const char*a=argv[i];if(!strcmp(a,"--help")){puts("Usage: groups [OPTION]... [USERNAME]...\nPrint the groups a user is in.\n  -n, --numeric     print numeric group IDs instead of names\n      --help        display this help and exit\n      --version     output version information and exit");return 0;}if(!strcmp(a,"--version")){puts("groups (zovutils) 0.10.22");return 0;}if(a[0]=='-'&&strcmp(a,"-")){fprintf(stderr,"groups: unrecognized option '%s'\nTry 'groups --help' for more information.\n",a);return 1;}if(user){fprintf(stderr,"groups: extra operand '%s'\nTry 'groups --help' for more information.\n",a);return 1;}user=a;}struct passwd*pw=user?getpwnam(user):getpwuid(getuid());if(user&&!pw){fprintf(stderr,"groups: '%s': no such user\n",user);return 1;}gid_t primary=pw?pw->pw_gid:getgid();int cap=getgroups(0,NULL);if(cap<0){fprintf(stderr,"groups: cannot get groups: %s\n",strerror(errno));return 1;}gid_t*gs=(gid_t*)malloc((size_t)(cap+1)*sizeof(gid_t));if(!gs)return 1;int n=getgroups(cap,gs);if(n<0){fprintf(stderr,"groups: cannot get groups: %s\n",strerror(errno));free(gs);return 1;}if(user)printf("%s : ",user);zov_group_print(primary,numeric);int printed=1;for(int i=0;i<n;i++)if(gs[i]!=primary){putchar(' ');zov_group_print(gs[i],numeric);printed++;}(void)printed;putchar('\n');free(gs);return 0;}
#endif
