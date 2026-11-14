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
int main(int argc,char**argv) {
 if(argc!=4)return 2;
 int failed=0;
 for(int i=0;i<3;i++) {
  char *args[]={"wsprd","-o","3","-a",argv[1],argv[2],NULL};
  int rc=wsprd_run(6,args);
  if(rc!=atoi(argv[3]))failed=1;
  fprintf(stderr,"AUDIT iteration=%d exit=%d outstandingBytes=%zu openFiles=%d\n",i+1,rc,outstanding,open_files);
 }
 return failed || outstanding || open_files ? 1 : 0;
}
