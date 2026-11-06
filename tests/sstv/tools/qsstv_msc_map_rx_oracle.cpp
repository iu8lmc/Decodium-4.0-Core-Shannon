// SPDX-License-Identifier: GPL-3.0-or-later
// Developer-only probe invoking QSSTV's actual six-frame MSC receive map.
#include <cstdio>
int mkmscmap(int,int,int,int,int);
extern int MSC_Demapper[6][2959];
int main() {
    constexpr int modulo=128;
    for(int mode=0;mode<3;++mode) for(int width=0;width<2;++width) for(int depth=0;depth<2;++depth) {
        const int count=mkmscmap(mode,width,depth,0,modulo);
        if(count<=0 || count>2959) return 1;
        std::printf("%d %d %d %d %d\n",mode,width,depth,count,modulo);
        for(int frame=0;frame<6;++frame) {
            for(int i=0;i<count;++i) std::printf("%d ",MSC_Demapper[frame][i]);
            std::putchar('\n');
        }
    }
}
