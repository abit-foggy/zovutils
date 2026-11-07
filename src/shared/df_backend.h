#ifndef ZOVUTILS_DF_BACKEND_H
#define ZOVUTILS_DF_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__linux__)
#include <mntent.h>
#endif
#if defined(__APPLE__)
#include <sys/mount.h>
#endif
static inline const char*zov_df_mount(const char*path,char*buf,size_t n){
#if defined(__linux__)
 FILE*f=setmntent("/proc/self/mounts","r");if(!f)return "/";struct mntent*m;size_t best=0;strcpy(buf,"/");while((m=getmntent(f))){size_t z=strlen(m->mnt_dir);if(z>=best&&!strncmp(path,m->mnt_dir,z)&&(path[z]==0||path[z]=='/'||z==1)){best=z;snprintf(buf,n,"%s",m->mnt_dir);}}endmntent(f);return buf;
#elif defined(__APPLE__)
 struct statfs*m;int nfs=getmntinfo(&m,MNT_NOWAIT);size_t best=0;strcpy(buf,"/");for(int i=0;i<nfs;i++){size_t z=strlen(m[i].f_mntonname);if(z>=best&&!strncmp(path,m[i].f_mntonname,z)){best=z;snprintf(buf,n,"%s",m[i].f_mntonname);}}return buf;
#else
 (void)path;(void)n;strcpy(buf,"/");return buf;
#endif
}
static inline int zov_df_one(const char*path,int posix){struct statvfs v;if(statvfs(path,&v)<0){fprintf(stderr,"df: %s: %s\n",path,strerror(errno));return 1;}char mount[4096];const char*target=zov_df_mount(path,mount,sizeof mount);char fs[256];
#if defined(__linux__)
 FILE*f=setmntent("/proc/self/mounts","r");snprintf(fs,sizeof fs,"%s",target);if(f){struct mntent*m;while((m=getmntent(f)))if(!strcmp(m->mnt_dir,target)){snprintf(fs,sizeof fs,"%s",m->mnt_fsname);break;}endmntent(f);}
#elif defined(__APPLE__)
 struct statfs*m;int n=getmntinfo(&m,MNT_NOWAIT);snprintf(fs,sizeof fs,"%s",target);for(int i=0;i<n;i++)if(!strcmp(m[i].f_mntonname,target)){snprintf(fs,sizeof fs,"%s",m[i].f_mntfromname);break;}
#else
 snprintf(fs,sizeof fs,"%s",target);
#endif
 unsigned long long unit=1024,blocks=(unsigned long long)v.f_blocks*(unsigned long long)(v.f_frsize?v.f_frsize:v.f_bsize)/unit,used=(unsigned long long)(v.f_blocks-v.f_bfree)*(unsigned long long)(v.f_frsize?v.f_frsize:v.f_bsize)/unit,avail=(unsigned long long)v.f_bavail*(unsigned long long)(v.f_frsize?v.f_frsize:v.f_bsize)/unit;unsigned long long pct=(used+avail)?(used*100+used+avail-1)/(used+avail):0;printf("%-9s     %12llu %6llu %9llu %7llu%% %s\n",fs,blocks,used,avail,pct,target);(void)posix;return 0;}
static inline int zov_df_run(int argc,char**argv){int posix=0,nf=0;const char**paths=(const char**)malloc((size_t)(argc+1)*sizeof(char*));if(!paths)return 1;for(int i=0;i<argc;i++){const char*a=argv[i];if(!strcmp(a,"--help")){puts("Usage: df [OPTION]... [FILE]...\nShow information about the file system on which each FILE resides.\n  -P, --portability  use the POSIX output format\n  -h, --human-readable  print sizes in powers of 1024\n      --help         display this help and exit\n      --version      output version information and exit");free(paths);return 0;}if(!strcmp(a,"--version")){puts("df (zovutils) 0.10.11");free(paths);return 0;}if(!strcmp(a,"-P")||!strcmp(a,"--portability")){posix=1;continue;}if(a[0]=='-'&&strcmp(a,"-")){fprintf(stderr,"df: unrecognized option '%s'\nTry 'df --help' for more information.\n",a);free(paths);return 1;}paths[nf++]=a;}if(!nf)paths[nf++]=".";puts("Filesystem     1024-blocks   Used Available Capacity Mounted on");int status=0;for(int i=0;i<nf;i++)if(zov_df_one(paths[i],posix))status=1;free(paths);return status;}
#endif
