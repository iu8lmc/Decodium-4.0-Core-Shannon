#!/usr/bin/env python3
"""Check every SIMD score against scalar under ASan/UBSan."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
source=(root/'Detector/FtxSync.cpp').read_text()
anchor='              if (sync > best) { best = sync; best_i = i0 + j * ibstp; }'
if source.count(anchor)!=1:raise RuntimeError('unexpected SIMD test structure')
check='''
        float reference=sync4_metric_templates(cd0,np,i0+j*ibstp,tpl);
        if(std::memcmp(&sync,&reference,sizeof(float))!=0) {
          std::fprintf(stderr,"mismatch i0=%d step=%d lane=%d old=%a new=%a\\n",i0,ibstp,j,reference,sync);
          std::abort();
        }
'''
variant=source.replace('#include <algorithm>',
    '#include <algorithm>\n#include <cstdio>\n#include <cstdlib>\n#include <cstring>',1).replace(anchor,check+anchor,1)
main='''
#include <random>
#include <vector>
#include <limits>
int main() {
  constexpr int n=5000; std::mt19937 rng(20261115);
  std::uniform_real_distribution<float> sample(-2.0f,2.0f);
  std::vector<Complex> data(n);std::array<Complex,64> tweak;
  for(auto& x:data)x=Complex(sample(rng),sample(rng));
  for(auto& x:tweak)x=Complex(sample(rng),sample(rng));
  int steps[]={1,2,4,7};
  for(int i=0;i<5000;++i) {
    int lo=static_cast<int>(rng()%2800)-700;
    int hi=std::min(2100,lo+static_cast<int>(rng()%350));
    float score;int best;
    ftx_sync2d_scan_c(data.data(),n,lo,hi,steps[rng()%4],tweak.data(),rng()%2,&score,&best);
  }
  for(float special:{0.0f,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
                     std::numeric_limits<float>::quiet_NaN(),1e38f,1e-40f}) {
    std::fill(data.begin(),data.end(),Complex{});data[500]=Complex(special,0.0f);
    float score;int best;ftx_sync2d_scan_c(data.data(),n,-100,2100,4,tweak.data(),1,&score,&best);
  }
  std::puts("SIMD scores match scalar; 5000 ranges and special values passed");
}
'''
with tempfile.TemporaryDirectory(prefix='decodium-sync-sanitize-') as tmp:
    cpp=Path(tmp)/'test.cpp';exe=Path(tmp)/'test';cpp.write_text(variant+main)
    subprocess.run(['g++','-O1','-g','-std=c++17','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=120)
