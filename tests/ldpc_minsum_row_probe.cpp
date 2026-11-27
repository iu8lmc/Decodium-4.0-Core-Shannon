// Validate leave-one-out min-sum for all rows, including floating specials.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>

float sgn(float v) { return v >= 0.0f ? 1.0f : -1.0f; }
__attribute__((noinline)) void original(const float* in,int n,float* out) {
 for(int i=0;i<n;++i){float sign=1.0f,mag=1.0e30f;
  for(int j=0;j<n;++j)if(i!=j){sign*=sgn(-in[j]);mag=std::min(mag,std::fabs(in[j]));}
  out[i]=-0.75f*sign*mag;}
}
__attribute__((noinline)) void candidate(const float* in,int n,float* out) {
 float s=1.0f,m1=1.0e30f,m2=1.0e30f;
 for(int j=0;j<n;++j){float v=in[j],a=std::fabs(v);s*=sgn(-v);
  if(a<m1){m2=m1;m1=a;}else if(a<m2)m2=a;}
 for(int i=0;i<n;++i){float v=in[i];out[i]=-0.75f*(s*sgn(-v))*(std::fabs(v)==m1?m2:m1);}
}
int main(){
 std::mt19937 rng(0x6b3df901u);float in[7],a[7],b[7];
 for(int t=0;t<1000000;++t){int n=6+t%2;
  for(int i=0;i<n;++i){std::uint32_t bits=rng();std::memcpy(in+i,&bits,4);
   if(t%37==0)in[i]=static_cast<int>(rng()%9)-4;
   if(t%79==0)in[i]=(i%2?0.0f:-0.0f);
   if(t%101==0)in[i]=std::numeric_limits<float>::quiet_NaN();}
  original(in,n,a);candidate(in,n,b);
  if(std::memcmp(a,b,n*sizeof(float))){std::fprintf(stderr,"mismatch %d\n",t);return 1;}}
 volatile float guard=0;
 for(int pass=0;pass<4;++pass){bool old=pass==0||pass==3;auto t=std::chrono::steady_clock::now();
  for(int i=0;i<3000000;++i){in[i%7]=static_cast<float>((i%67)-33);if(old)original(in,7,a);else candidate(in,7,a);guard+=a[i%7];}
  double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();std::printf("%s %.3f ms\n",old?"old":"new",ms);}
 std::printf("1000000 rows exact; guard %f\n",guard);
}
