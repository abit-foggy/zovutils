#ifndef ZOVUTILS_PTX_BACKEND_H
#define ZOVUTILS_PTX_BACKEND_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
typedef struct { char *word; char *left; char *right; char *file; unsigned long line; } zov_ptx_entry;
static inline int zov_ptx_cmp(const void *aa,const void *bb){const zov_ptx_entry*a=(const zov_ptx_entry*)aa,*b=(const zov_ptx_entry*)bb;int c=strcasecmp(a->word,b->word);if(c)return c;return strcmp(a->word,b->word);}
static inline int zov_ptx_run(int argc,char**argv){
 int all=0,reverse=0,files=0,nf=0;const char**names=(const char**)malloc((size_t)(argc+1)*sizeof(char*));if(!names)return 1;
 for(int i=0;i<argc;i++){const char*a=argv[i];if(!strcmp(a,"--version")){puts("ptx (zovutils) 0.10.0");free(names);return 0;}if(!strcmp(a,"--help")){puts("Usage: ptx [OPTION]... [INPUT]...\nProduce a permuted index of file contents.\n  -A, --auto-reference  output automatically generated references\n  -R, --right-side-refs put references on the right\n      --help             display this help and exit\n      --version          output version information and exit");free(names);return 0;}if(!strcmp(a,"-A")||!strcmp(a,"--auto-reference")){all=1;continue;}if(!strcmp(a,"-R")||!strcmp(a,"--right-side-refs")){reverse=1;continue;}if(a[0]=='-'&&strcmp(a,"-")){fprintf(stderr,"ptx: unrecognized option '%s'\nTry 'ptx --help' for more information.\n",a);free(names);return 1;}names[nf++]=a;}
 if(!nf)names[nf++]="-";zov_ptx_entry*entries=NULL;size_t used=0,cap=0;int status=0;
 for(int fi=0;fi<nf;fi++){FILE*f=!strcmp(names[fi],"-")?stdin:fopen(names[fi],"rb");if(!f){fprintf(stderr,"ptx: %s: %s\n",names[fi],strerror(errno));status=1;continue;}char*line=NULL;size_t lc=0;ssize_t got;unsigned long lineno=0;
  while((got=getline(&line,&lc,f))>=0){lineno++;size_t n=(size_t)got;size_t p=0;while(p<n){while(p<n&&!isalnum((unsigned char)line[p]))p++;size_t b=p;while(p<n&&isalnum((unsigned char)line[p]))p++;size_t e=p;if(e==b)continue;if(used==cap){size_t nc=cap?cap*2:32;zov_ptx_entry*q=(zov_ptx_entry*)realloc(entries,nc*sizeof(*q));if(!q){status=1;break;}entries=q;cap=nc;}zov_ptx_entry*x=&entries[used++];x->word=strndup(line+b,e-b);x->left=strndup(line,b);x->right=strdup(line+e);x->file=strdup(names[fi]);x->line=lineno;if(!x->word||!x->left||!x->right||!x->file){status=1;break;}}free(line);line=NULL;lc=0;}
  free(line);if(f!=stdin)fclose(f);
 }
 if(used)qsort(entries,used,sizeof(*entries),zov_ptx_cmp);
 for(size_t i=0;i<used;i++){zov_ptx_entry*x=&entries[i];if(all){if(reverse)printf("%s:%lu: ",x->file,x->line);else printf("%s:%lu: ",x->file,x->line);}size_t left=strlen(x->left);if(left>38)left=38;for(size_t j=left;j<38;j++)putchar(' ');fwrite(x->left,1,left,stdout);printf(" %s ",x->word);fputs(x->right,stdout);if(x->right[0]&&x->right[strlen(x->right)-1]!='\n')putchar('\n');}
 for(size_t i=0;i<used;i++){free(entries[i].word);free(entries[i].left);free(entries[i].right);free(entries[i].file);}free(entries);free(names);return status;
}
#endif
