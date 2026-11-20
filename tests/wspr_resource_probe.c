/* Resource accounting for the actual embedded WSPR implementation.
 * Usage: probe PRIVATE_OUTPUT_DIR WAV EXPECTED_EXIT
 * Wraps this translation unit only; does not measure FFTW caches or process RSS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fftw3.h>
struct allocation { void* pointer; size_t bytes; };
static struct allocation allocations[1024];
static size_t outstanding;
static int open_files;
static void* counted_calloc(size_t n,size_t size) {
 void* p=calloc(n,size);
 if(p) for(int i=0;i<1024;i++) if(!allocations[i].pointer) {
  allocations[i]=(struct allocation){p,n*size};outstanding+=n*size;break;
 }
 return p;
}
static void counted_free(void* p) {
 if(p) for(int i=0;i<1024;i++) if(allocations[i].pointer==p) {
  outstanding-=allocations[i].bytes;allocations[i].pointer=NULL;break;
 }
 free(p);
}
static FILE* counted_fopen(const char* path,const char* mode) {
 FILE* f=fopen(path,mode);if(f)open_files++;return f;
}
static int counted_fclose(FILE* f) { int rc=fclose(f);if(rc==0)open_files--;return rc; }
#define calloc counted_calloc
#define free counted_free
#define fopen counted_fopen
#define fclose counted_fclose
#define WSPRD_EMBEDDED_ONLY
#ifndef WSPR_SOURCE_FILE
#define WSPR_SOURCE_FILE "../lib/wsprd/wsprd.c"
#endif
#include WSPR_SOURCE_FILE
#undef calloc
#undef free
#undef fopen
#undef fclose
struct cancel_probe { int calls; int threshold; };
static int cancel_after_checks(void* context) {
 struct cancel_probe* probe=(struct cancel_probe*)context;
 return ++probe->calls >= probe->threshold;
}
int main(int argc,char**argv) {
 if(argc<4 || argc>6)return 2;
 int failed=0;
 char c2path[1024];
 char hashpath[1024];
 int mixed=argc>=5 && strcmp(argv[4],"mixed")==0;
 if(argc==6 && !mixed)return 2;
 if(argc>=5) {
  snprintf(hashpath,sizeof(hashpath),"%s/hashtable.txt",argv[1]);
  FILE* f=fopen(hashpath,"wb"); if(!f)return 2;
  fputs("123 K1ABC FN42\n",f);fclose(f);
 }
 if(mixed && argc==6) {
  snprintf(c2path,sizeof(c2path),"%s",argv[5]);
 } else if(mixed) {
  snprintf(c2path,sizeof(c2path),"%s/260101_1222.c2",argv[1]);
  FILE* f=fopen(c2path,"wb"); if(!f)return 2;
  char name[14]={0}; int minutes=2; double frequency=14.0971; float zero=0;
  fwrite(name,1,sizeof(name),f);fwrite(&minutes,sizeof(minutes),1,f);
  fwrite(&frequency,sizeof(frequency),1,f);
  for(int i=0;i<90000;i++)fwrite(&zero,sizeof(zero),1,f);
  fclose(f);
 }
 for(int i=0;i<3;i++) {
  struct cancel_probe probe={0,3};
  if(argc>=5 && strcmp(argv[4],"early")==0)probe.threshold=1;
  if(argc>=5 && strcmp(argv[4],"after-read")==0)probe.threshold=2;
  wsprd_set_cancel_callback(argc>=5 && i==0 ? cancel_after_checks : NULL, &probe);
  char *args[]={"wsprd","-o","3","-a",argv[1],mixed && i>0 ? c2path : argv[2],NULL};
  int rc=wsprd_run(6,args);
  if(rc!=(argc>=5 && i==0 ? 2 : atoi(argv[3])))failed=1;
  if(argc>=5 && i==0) {
   char line[80]={0};FILE* f=fopen(hashpath,"rb");
   if(!f || !fgets(line,sizeof(line),f) || strcmp(line,"123 K1ABC FN42\n")!=0)failed=1;
   if(f)fclose(f);
  }
  fprintf(stderr,"AUDIT iteration=%d exit=%d outstandingBytes=%zu openFiles=%d\n",i+1,rc,outstanding,open_files);
 }
 wsprd_set_cancel_callback(NULL, NULL);
 return failed || outstanding || open_files ? 1 : 0;
}
