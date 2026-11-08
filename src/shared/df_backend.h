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
static inline void zov_df_human(unsigned long long bytes,int base,char*out,size_t cap){static const char units[]="BKMGTPE";unsigned long long divisor=1;int unit=0;while(unit<6&&bytes/divisor>=(unsigned long long)base){if(divisor>(~0ULL)/(unsigned long long)base)break;divisor*=(unsigned long long)base;unit++;}unsigned long long value=(bytes+divisor-1)/divisor;if(unit&&value>=(unsigned long long)base&&unit<6){divisor*=(unsigned long long)base;value=(bytes+divisor-1)/divisor;unit++;}if(unit)snprintf(out,cap,"%llu%c",value,units[unit]);else snprintf(out,cap,"%llu",value);}
static inline void zov_df_source_type(const char*target,char*fs,size_t fsn,char*type,size_t typen){snprintf(fs,fsn,"%s",target);type[0]=0;
#if defined(__linux__)
 FILE*f=setmntent("/proc/self/mounts","r");if(f){struct mntent*m;while((m=getmntent(f)))if(!strcmp(m->mnt_dir,target)){snprintf(fs,fsn,"%s",m->mnt_fsname);snprintf(type,typen,"%s",m->mnt_type);break;}endmntent(f);}
#elif defined(__APPLE__)
 struct statfs*m;int n=getmntinfo(&m,MNT_NOWAIT);for(int i=0;i<n;i++)if(!strcmp(m[i].f_mntonname,target)){snprintf(fs,fsn,"%s",m[i].f_mntfromname);snprintf(type,typen,"%s",m[i].f_fstypename);break;}
#endif
}
static inline int zov_df_one(const char*path,int posix,int inodes,int print_type,int human,int si,int source_width,int type_width,int blocks_width,int block_used_width,int block_free_width){struct statvfs v;if(statvfs(path,&v)<0){fprintf(stderr,"df: %s: %s\n",path,strerror(errno));return 1;}char mount[4096];const char*target=zov_df_mount(path,mount,sizeof mount);char fs[256],type[64]="";
 zov_df_source_type(target,fs,sizeof fs,type,sizeof type);
 if(inodes){unsigned long long total=(unsigned long long)v.f_files,avail=(unsigned long long)v.f_ffree,used=total>=avail?total-avail:0,pct=(used+avail)?(used*100+used+avail-1)/(used+avail):0;printf("%-14s %7llu %5llu %7llu %4llu%% %s\n",fs,total,used,avail,pct,target);return 0;}
 unsigned long long size=(unsigned long long)(v.f_frsize?v.f_frsize:v.f_bsize),total_bytes=(unsigned long long)v.f_blocks*size,used_bytes=(unsigned long long)(v.f_blocks-v.f_bfree)*size,avail_bytes=(unsigned long long)v.f_bavail*size,unit=1024,blocks=total_bytes/unit,used=used_bytes/unit,avail=avail_bytes/unit,pct=(used+avail)?(used*100+used+avail-1)/(used+avail):0;if(human){char hs[32],hu[32],ha[32];int base=si?1000:1024;zov_df_human(total_bytes,base,hs,sizeof hs);zov_df_human(used_bytes,base,hu,sizeof hu);zov_df_human(avail_bytes,base,ha,sizeof ha);if(print_type)printf("%-14s %-5s %4s %5s %5s %3llu%% %s\n",fs,type,hs,hu,ha,pct,target);else printf("%-15s %4s %5s %5s %3llu%% %s\n",fs,hs,hu,ha,pct,target);}else if(print_type)printf("%-*s %-*s %*llu %*llu %*llu %3llu%% %s\n",source_width,fs,type_width,type,blocks_width,blocks,block_used_width,used,block_free_width,avail,pct,target);else printf("%-14s %11llu %6llu %9llu %7llu%% %s\n",fs,blocks,used,avail,pct,target);(void)posix;return 0;}
static inline int zov_df_used_width(const char*path){struct statvfs v;if(statvfs(path,&v)<0)return 5;unsigned long long size=(unsigned long long)(v.f_frsize?v.f_frsize:v.f_bsize),used=(unsigned long long)(v.f_blocks-v.f_bfree)*size/1024;int width=0;do{width++;used/=10;}while(used);return width>5?width:5;}
static inline int zov_df_digits(unsigned long long n){int width=1;while(n>=10){n/=10;width++;}return width;}
static inline int zov_df_run(int argc,char**argv){int posix=0,inodes=0,print_type=0,human=0,si=0,nf=0,used_width=5,inode_total_width=7,inode_used_width=5,inode_free_width=7,source_width=14,type_width=4,blocks_width=9,block_used_width=6,block_free_width=9;const char**paths=(const char**)malloc((size_t)(argc+1)*sizeof(char*));if(!paths)return 1;for(int i=0;i<argc;i++){const char*a=argv[i];if(!strcmp(a,"--help")){puts("Usage: df [OPTION]... [FILE]...\nShow information about the file system on which each FILE resides.\n  -i, --inodes       list inode information instead of block usage\n  -P, --portability  use the POSIX output format\n  -T, --print-type   print file system type\n  -h, --human-readable  print sizes in powers of 1024\n  -H, --si           print sizes in powers of 1000\n      --help         display this help and exit\n      --version      output version information and exit");free(paths);return 0;}if(!strcmp(a,"--version")){puts("df (zovutils) 0.10.40");free(paths);return 0;}if(!strcmp(a,"-P")||!strcmp(a,"--portability")){posix=1;continue;}if(!strcmp(a,"-i")||!strcmp(a,"--inodes")){inodes=1;continue;}if(!strcmp(a,"-T")||!strcmp(a,"--print-type")){print_type=1;continue;}if(!strcmp(a,"-h")||!strcmp(a,"--human-readable")){human=1;continue;}if(!strcmp(a,"-H")||!strcmp(a,"--si")){human=1;si=1;continue;}if(a[0]=='-'&&strcmp(a,"-")){fprintf(stderr,"df: unrecognized option '%s'\nTry 'df --help' for more information.\n",a);free(paths);return 1;}paths[nf++]=a;}if(!nf)paths[nf++]=".";for(int i=0;i<nf;i++){int width=zov_df_used_width(paths[i]);if(width>used_width)used_width=width;if(print_type&&!human){char mount[4096],fs[256],type[64];const char*target=zov_df_mount(paths[i],mount,sizeof mount);zov_df_source_type(target,fs,sizeof fs,type,sizeof type);int a=(int)strlen(fs),b=(int)strlen(type);if(a>source_width)source_width=a;if(b>type_width)type_width=b;struct statvfs tv;if(statvfs(paths[i],&tv)==0){unsigned long long size=(unsigned long long)(tv.f_frsize?tv.f_frsize:tv.f_bsize),tb=(unsigned long long)tv.f_blocks*size/1024,tu=(unsigned long long)(tv.f_blocks-tv.f_bfree)*size/1024,ta=(unsigned long long)tv.f_bavail*size/1024;int c=zov_df_digits(tb),d=zov_df_digits(tu),e=zov_df_digits(ta);if(c>blocks_width)blocks_width=c;if(d>block_used_width)block_used_width=d;if(e>block_free_width)block_free_width=e;}}if(inodes){struct statvfs v;if(statvfs(paths[i],&v)==0){unsigned long long total=(unsigned long long)v.f_files,free_count=(unsigned long long)v.f_ffree,used_count=total>=free_count?total-free_count:0;int a=zov_df_digits(total),b=zov_df_digits(used_count),c=zov_df_digits(free_count);if(a>inode_total_width)inode_total_width=a;if(b>inode_used_width)inode_used_width=b;if(c>inode_free_width)inode_free_width=c;}}}if(inodes)printf("%-14s %*s %*s %*s IUse%% Mounted on\n","Filesystem",inode_total_width,"Inodes",inode_used_width,"IUsed",inode_free_width,"IFree");else if(human)puts("Filesystem      Size  Used Avail Use% Mounted on");else if(print_type){printf("%-*s %-*s %*s %*s %*s %3s Mounted on\n",source_width,"Filesystem",type_width,"Type",blocks_width,"1K-blocks",block_used_width,"Used",block_free_width,"Available","Use%");}else{printf("Filesystem     1024-blocks");for(int i=0;i<used_width-3;i++)putchar(' ');puts("Used Available Capacity Mounted on");}int status=0;for(int i=0;i<nf;i++)if(zov_df_one(paths[i],posix,inodes,print_type,human,si,source_width,type_width,blocks_width,block_used_width,block_free_width))status=1;free(paths);return status;}
#endif
