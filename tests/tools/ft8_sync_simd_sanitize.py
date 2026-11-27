#!/usr/bin/env python3
"""Compare every vector FT8 sync value with scalar under ASan/UBSan."""
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
variant=(root/'Detector/FtxSync.cpp').read_text()
anchor='    scores[lane]=finish();flags[lane]=lcq_out ? 1 : 0;'
check='''
    bool referenceCq=false;
    float reference=sync8var_scan_metric(s,nh1,i+lane,j,1,false,false,referenceCq);
    if(std::memcmp(&scores[lane],&reference,sizeof(float))!=0 || flags[lane]!=(referenceCq?1:0)) {
      std::fprintf(stderr,"FT8 sync mismatch i=%d j=%d lane=%d old=%a new=%a\\n",i,j,lane,reference,scores[lane]);
      std::abort();
    }
'''
if variant.count(anchor)!=1:raise RuntimeError('unexpected prototype structure')
variant=variant.replace('#include <algorithm>',
    '#include <algorithm>\n#include <cstdio>\n#include <cstdlib>\n#include <cstring>',1).replace(anchor,anchor+check,1)
main='''
#include <random>
#include <limits>
int main() {
  constexpr int nh1=96,nhsym=372;std::mt19937 rng(20261115);
  std::uniform_real_distribution<float> value(0.0f,100.0f);
  std::vector<float> data(nh1*nhsym),scores(nh1*125);
  std::vector<signed char> flags(nh1*125);
  for(int k=0;k<300;++k) {
    if(k%30==0)for(auto& v:data)v=value(rng);
    int lo=1+static_cast<int>(rng()%30),hi=std::min(nh1-16,lo+4+static_cast<int>(rng()%45));
    int jlo=-62+static_cast<int>(rng()%100),jhi=std::min(62,jlo+static_cast<int>(rng()%25));
    ftx_sync8var_scan_c(data.data(),nh1,nhsym,lo,hi,jlo,jhi,1,0,0,scores.data(),flags.data());
  }
  for(float special:{0.0f,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN(),1e38f,1e-40f}) {
    std::fill(data.begin(),data.end(),0.0f);data[nh1*120+20]=special;
    ftx_sync8var_scan_c(data.data(),nh1,nhsym,1,nh1-16,-62,62,1,0,0,scores.data(),flags.data());
  }
  std::puts("FT8 SIMD scores and CQ flags match scalar under ASan/UBSan");
}
'''
with tempfile.TemporaryDirectory(prefix='decodium-ft8-sync-sanitize-') as tmp:
    cpp=Path(tmp)/'test.cpp';exe=Path(tmp)/'test';cpp.write_text(variant+main)
    subprocess.run(['g++','-O1','-g','-std=c++17','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=120)
