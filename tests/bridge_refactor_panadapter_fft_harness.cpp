#include <QCoreApplication>
#include <QDebug>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <vector>
#include <iostream>
#ifdef FFTW3_SINGLE_FOUND
#include "Detector/FftCompat.hpp"
#endif
#include "bridge_refactor_panadapter_fft_helper.inc"
class DecodiumBridge{public:static constexpr int PANADAPTER_FFT_SIZE=4096,SAMPLE_RATE=12000;struct PanadapterFrameResult{QVector<float> values;float minDb=0,maxDb=0;};static PanadapterFrameResult computePanadapterFrame(const QVector<short>&,int,int);};
#include "bridge_refactor_panadapter_fft.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};check(DecodiumBridge::computePanadapterFrame({1,2},200,3000).values.isEmpty(),"short-input");QVector<short> samples;for(int i=0;i<4096;++i)samples.append(short(16000*std::sin(2*3.141592653589793*512*i/4096)));auto frame=DecodiumBridge::computePanadapterFrame(samples,200,3000);auto peak=std::max_element(frame.values.cbegin(),frame.values.cend());check(!frame.values.isEmpty()&&std::abs(int(std::distance(frame.values.cbegin(),peak))-444)<=1,"tone-peak");check(std::isfinite(frame.minDb)&&std::isfinite(frame.maxDb)&&frame.maxDb>=frame.minDb,"levels");check(DecodiumBridge::computePanadapterFrame(samples,4000,200).values.isEmpty(),"invalid-band");auto second=DecodiumBridge::computePanadapterFrame(samples,200,3000);check(second.values==frame.values,"repeat-workspace");}
