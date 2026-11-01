// SPDX-License-Identifier: GPL-3.0-or-later
// Developer-only harness around QSSTV's original MSC receive stage.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
int *deinterleaver(int,int,int,int);
int msdhardmsc(double*,double*,int,double*,int,double*,int,int,int,int*,int*,int,int,double*,double*,double*,double*,double*);
int main() {
    const int numer[13]={1,3,1,4,1,4,3,2,8,3,4,7,8};
    const int denom[13]={4,10,3,11,2,7,5,3,11,4,5,8,9};
    const int rates16[2][2]={{2,7},{4,9}};
    const int rates64[2][3]={{0,4,9},{2,7,10}};
    int n,mode,protection,variant, cases=0;
    char bits[12000];
    while(std::scanf("%d%d%d%d%11999s",&n,&mode,&protection,&variant,bits)==5) {
        if(n<20 || n>1000 || mode<0 || mode>2 || protection<0 || protection>1 || variant<0 || variant>1) return 1;
        std::vector<double> real(n),imag(n),snr(n,1.0),noise(2*n),output(12000),vspp(10);
        for(int i=0;i<n;++i) if(std::scanf("%lf%lf",&real[i],&imag[i])!=2) return 2;
        const int levels=mode+1;
        double lengths[6]={}; int rates[6]={};
        std::vector<int> deint(2*n*levels);
        for(int level=0;level<levels;++level) {
            const int rate=mode==0 ? 6 : (mode==1 ? rates16[protection][level] : rates64[protection][level]);
            rates[level]=rates[level+levels]=rate;
            lengths[level+levels]=numer[rate]*((2*n-12)/denom[rate]);
            const int t0=mode==0 ? 21 : (mode==1 ? (level==0 ? 13 : 21) : (level==0 ? 0 : (level==1 ? 13 : 21)));
            if (t0==0) for(int j=0;j<2*n;++j) deint[level*2*n+j]=j;
            else { int* p=deinterleaver(0,1,2*n,t0); if(!p) return 3;
                   std::copy(p,p+2*n,deint.begin()+level*2*n); std::free(p); }
        }
        // Variant B also tests recovery after a deterministic small perturbation.
        if (variant == 1) { real[7] += 0.015; imag[7] -= 0.015; }
        double iterations=0,variance=0;
        const int decoded=msdhardmsc(real.data(),imag.data(),n,snr.data(),0,lengths,levels,2,0,
            deint.data(),rates,4,1,output.data(),vspp.data(),&iterations,&variance,noise.data());
        const auto expected=std::string(bits);
        if(decoded!=int(expected.size())) { std::fprintf(stderr,"MSC length %d/%zu profile %d/%d/%d\n",decoded,expected.size(),n,mode,protection); return 4; }
        for(int i=0;i<decoded;++i) if(!std::isfinite(output[i]) || int(std::lround(output[i]))!=expected[std::size_t(i)]-'0') {
            std::fprintf(stderr,"MSC mismatch profile %d/%d/%d variant %d bit %d\n",n,mode,protection,variant,i); return 5;
        }
        ++cases;
    }
    if(cases!=72) return 6;
    std::printf("QSSTV MSC receiver decoded %d Decodium vectors, A clean and B perturbed\n",cases);
}
