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
static inline int zov_groups_run(int argc,char**argv){int numeric=0;const char*user=NULL;for(int i=0;i<argc;i++){const char*a=argv[i];if(!strcmp(a,"--help")){puts("Usage: groups [OPTION]... [USERNAME]...\nPrint the groups a user is in.\n  -n, --numeric     print numeric group IDs instead of names\n      --help        display this help and exit\n      --version     output version information and exit");return 0;}if(!strcmp(a,"--version")){puts("groups (zovutils) 0.10.35");return 0;}if(a[0]=='-'&&strcmp(a,"-")){fprintf(stderr,"groups: unrecognized option '%s'\nTry 'groups --help' for more information.\n",a);return 1;}if(user){fprintf(stderr,"groups: extra operand '%s'\nTry 'groups --help' for more information.\n",a);return 1;}user=a;}struct passwd*pw=user?getpwnam(user):getpwuid(getuid());if(user&&!pw){fprintf(stderr,"groups: '%s': no such user\n",user);return 1;}gid_t primary=user?pw->pw_gid:getgid();gid_t*gs=NULL;int n=0;if(user){int cap=16;gs=(gid_t*)malloc((size_t)cap*sizeof(gid_t));if(!gs)return 1;n=cap;if(getgrouplist(pw->pw_name,primary,gs,&n)<0){if(n<=cap){fprintf(stderr,"groups: cannot get groups: %s\n",strerror(errno));free(gs);return 1;}gid_t*larger=(gid_t*)realloc(gs,(size_t)n*sizeof(gid_t));if(!larger){free(gs);return 1;}gs=larger;cap=n;if(getgrouplist(pw->pw_name,primary,gs,&n)<0){fprintf(stderr,"groups: cannot get groups: %s\n",strerror(errno));free(gs);return 1;}}}else{n=getgroups(0,NULL);if(n<0){fprintf(stderr,"groups: cannot get groups: %s\n",strerror(errno));return 1;}gs=(gid_t*)malloc((size_t)(n?n:1)*sizeof(gid_t));if(!gs)return 1;if(n&&getgroups(n,gs)<0){fprintf(stderr,"groups: cannot get groups: %s\n",strerror(errno));free(gs);return 1;}}if(user)printf("%s : ",user);zov_group_print(primary,numeric);for(int i=0;i<n;i++)if(gs[i]!=primary){putchar(' ');zov_group_print(gs[i],numeric);}putchar('\n');free(gs);return 0;}
#endif
