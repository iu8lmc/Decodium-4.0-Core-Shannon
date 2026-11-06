// SPDX-License-Identifier: GPL-3.0-or-later
// Developer-only executable of QSSTV's FAC receiver and deinterleaver.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
int *deinterleaver(int, int, int, int);
int msdhardfac(double*,double*,int,double*,int,double*,int,int,int*,int*,int,int,double*);
int main() {
    int* perm=deinterleaver(0,1,90,21);
    if (!perm) return 1;
    char bits[128]; int count=0;
    while (std::scanf("%127s",bits)==1) {
        if (std::strlen(bits)!=48) return 2;
        double real[45], imag[45], snr[45], out[48]{};
        for(int i=0;i<45;++i) { if(std::scanf("%lf%lf", &real[i],&imag[i])!=2) return 3; snr[i]=1.0; }
        double lengths[2]={0,48}; int protection[2]={0,6};
        for(int variant=0;variant<2;++variant) {
            double testReal[45], testImag[45];
            std::memcpy(testReal,real,sizeof(real)); std::memcpy(testImag,imag,sizeof(imag));
            if (variant==1) { testReal[7]=-testReal[7]; testImag[7]=-testImag[7]; }
            msdhardfac(testReal,testImag,45,snr,0,lengths,2,0,perm,protection,4,0,out);
            for(int i=0;i<48;++i) if (!std::isfinite(out[i]) || int(std::lround(out[i])) != bits[i]-'0') {
                std::fprintf(stderr,"FAC case %d variant %d bit %d mismatch got %.2f expected %c\n",count,variant,i,out[i],bits[i]); return 4;
            }
        }
        ++count;
    }
    std::free(perm);
    if(count!=288) return 5;
    std::printf("QSSTV FAC receiver decoded %d Decodium vectors, clean and one corrupted cell\n",count);
}
