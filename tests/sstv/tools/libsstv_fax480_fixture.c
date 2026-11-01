/* Developer-only libsstv FAX480 source; linked only against pinned reference. */
#include <libsstv.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    if (argc!=3 || sstv_init(malloc,free)!=SSTV_OK) return 1;
    const int variant=atoi(argv[1]);
    if (variant<0 || variant>1) return 2;
    const size_t count=512U*480U;
    uint8_t* pixels=malloc(count);
    if (!pixels) return 3;
    for (size_t i=0;i<count;++i) {
        const unsigned x=(unsigned)(i%512U), y=(unsigned)(i/512U);
        pixels[i]=(uint8_t)((((x/64U)+(y/80U)+(unsigned)variant*2U)%4U)*60U+35U);
    }
    sstv_image_t image;
    if (sstv_pack_image(&image,512,480,SSTV_FORMAT_Y,pixels)!=SSTV_OK) return 4;
    void* encoder=NULL;
    if (sstv_create_encoder(&encoder,image,SSTV_MODE_FAX480,12000U)!=SSTV_OK) return 5;
    FILE* file=fopen(argv[2],"wb");
    if (!file) return 6;
    int16_t samples[8192]; sstv_signal_t signal;
    if (sstv_pack_signal(&signal,SSTV_SAMPLE_INT16,8192U,samples)!=SSTV_OK) return 7;
    uint64_t total=0;
    for (;;) {
        const sstv_error_t status=sstv_encode(encoder,&signal);
        if (status!=SSTV_ENCODE_SUCCESSFUL && status!=SSTV_ENCODE_END) return 8;
        if (fwrite(samples,sizeof(samples[0]),signal.count,file)!=signal.count) return 9;
        total+=signal.count;
        if (status==SSTV_ENCODE_END) break;
    }
    fclose(file); sstv_delete_encoder(encoder); free(pixels);
    printf("variant=%d samples=%llu\n",variant,(unsigned long long)total);
    return 0;
}
