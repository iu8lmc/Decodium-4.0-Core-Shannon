#include <QCoreApplication>
#include <QByteArray>
#include <QVector>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <complex>
#include <limits>
#include <vector>
#include <iostream>
struct Remote {bool running=true,enabled=true;int calls=0;QByteArray row;bool isRunning()const{return running;}bool waterfallEnabled()const{return enabled;}void publishWaterfallRow(const QByteArray&r,int,int,int,int,const QString&){row=r;++calls;}};
class DecodiumBridge{public:
 static constexpr int SPECTRUM_FFT_SIZE=512,SAMPLE_RATE=12000,PANADAPTER_FFT_SIZE=4096;QVector<float> m_spectrumBuf,m_lastPanadapterData;int m_nfa=200,m_nfb=3000,m_spectrumRefLevel=0,m_spectrumDynRange=100,m_rxFrequency=1500,m_txFrequency=1500;QString m_mode="FT8";Remote* m_remoteServer=nullptr;std::atomic<bool> m_panadapterComputeBusy{false};std::atomic<uint64_t> m_panadapterComputeSerial{1};bool m_monitoring=true,m_transmitting=false,m_tuning=false;float m_lastPanMinDb=0,m_lastPanMaxDb=0,m_lastPanFreqMin=0,m_lastPanFreqMax=0;int signalsCount=0;
 void panadapterDataReady(const QVector<float>&,float,float,float,float){++signalsCount;}QVector<float> computeSpectrum()const;void publishRemoteWaterfallFrame(const QVector<float>&,float,float,float,float);void finishPanadapterFrame(QVector<float>,float,float,int,float,float,uint64_t);
};
#include "bridge_refactor_spectrum.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};std::vector<std::complex<float>> impulse(8);impulse[0]={1,0};fft_inplace(impulse);check(std::all_of(impulse.begin(),impulse.end(),[](auto v){return std::abs(v-std::complex<float>(1,0))<0.00001f;}),"fft-impulse");DecodiumBridge b;check(b.computeSpectrum().isEmpty(),"insufficient-input");for(int i=0;i<512;++i)b.m_spectrumBuf.append(std::sin(2*3.141592653589793*64*i/512));auto bins=b.computeSpectrum();auto it=std::max_element(bins.cbegin(),bins.cend());check(!bins.isEmpty()&&std::distance(bins.cbegin(),it)==56,"fft-tone");b.m_nfa=4000;b.m_nfb=200;check(b.computeSpectrum().isEmpty(),"empty-band");Remote r;b.m_remoteServer=&r;b.publishRemoteWaterfallFrame({-100,-50,0,std::numeric_limits<float>::quiet_NaN()},-100,0,200,3000);check(r.calls==1&&r.row.size()==4&&static_cast<unsigned char>(r.row[0])==0&&static_cast<unsigned char>(r.row[1])==128&&static_cast<unsigned char>(r.row[2])==255&&static_cast<unsigned char>(r.row[3])==0,"remote-quantize");b.publishRemoteWaterfallFrame({1,2},0,1,3000,200);check(r.calls==1,"invalid-span");b.m_panadapterComputeBusy=true;b.finishPanadapterFrame({1,2},0,10,4096,200,3000,2);check(!b.m_panadapterComputeBusy&&b.signalsCount==0,"stale-serial");b.m_transmitting=true;b.finishPanadapterFrame({1,2},0,10,4096,200,3000,1);check(b.signalsCount==0,"tx-gate");b.m_transmitting=false;b.m_lastPanadapterData={0,0};b.finishPanadapterFrame({1,2},0,10,1024,200,3000,1);check(b.signalsCount==1&&r.calls==2&&std::abs(b.m_lastPanadapterData[0]-0.7f)<0.0001f,"smooth-and-publish");}
