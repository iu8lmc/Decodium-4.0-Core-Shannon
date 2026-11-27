#include <QCoreApplication>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
static void bridgeLog(const QString&){}
static bool isFt2LinkApplicationMode(QString mode){return mode.trimmed().compare("FT2-LINK",Qt::CaseInsensitive)==0;}
struct RtlSdrDsp{enum class Demodulator {WeakSignal,Other};};struct RtlSdrRfSpectrum{static constexpr int kFftSize=4;};
struct Input {bool running=true;RtlSdrDsp::Demodulator demod=RtlSdrDsp::Demodulator::WeakSignal;struct Config{RtlSdrDsp::Demodulator demodulator;};bool isRunning()const{return running;}Config activeConfig()const{return {demod};}};
struct Wav {int count=0;void feedSample(short){++count;}};
class DecodiumBridge {public:
 static constexpr int ASYNC_BUF_SIZE=1024,RTL_IQ_RING_SHORTS=16;Input* m_rtlSdrInput=nullptr;Wav* m_wavManager=nullptr;bool m_monitoring=false,m_transmitting=false,m_tuning=false,enabled=true;QString m_mode="FT8";QMutex m_audioBufferMutex;QVector<short> m_audioBuffer,m_rtlSdrIqRing;short m_asyncAudio[ASYNC_BUF_SIZE]{};std::atomic<quint64> m_asyncAudioPos{0},m_driftExpectedFrames{0};double m_audioLevel=0;int health=0,levels=0,ft2=0,m_rtlSdrRfSampleRate=0,m_rtlSdrIqWritePos=0,m_rtlSdrIqCount=0,rateSignals=0,viewSignals=0;
 bool rtlSdrEnabled()const{return enabled;}void audioLevelChanged(){++levels;}void handleAudioHealth(double,double,int,int,int){++health;}void ft2LinkRxSamplesReady(const QVector<short>&,quint64){++ft2;}void rtlSdrRfSampleRateChanged(){++rateSignals;}void rtlSdrRfViewChanged(){++viewSignals;}
 void onRtlSdrPcmSamplesReady(const QVector<short>&);void onRtlSdrIqSamplesReady(const QVector<short>&,int,quint32);
};
#include "bridge_refactor_rtlsdr_samples.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};DecodiumBridge b;Input in;Wav wav;b.m_rtlSdrInput=&in;b.m_wavManager=&wav;b.onRtlSdrPcmSamplesReady({1,2});check(b.m_audioBuffer.isEmpty(),"monitor-gate");b.m_monitoring=true;in.demod=RtlSdrDsp::Demodulator::Other;b.onRtlSdrPcmSamplesReady({1,2});check(b.m_audioBuffer.isEmpty(),"demod-gate");in.demod=RtlSdrDsp::Demodulator::WeakSignal;b.onRtlSdrPcmSamplesReady({100,-100,32767});check(b.m_audioBuffer.size()==3&&wav.count==3&&b.health==1&&b.m_asyncAudioPos==3&&b.m_driftExpectedFrames==3&&b.ft2==0,"pcm-ft8");b.m_mode="FT2-LINK";b.onRtlSdrPcmSamplesReady({4});check(b.ft2==1,"pcm-ft2");b.m_transmitting=true;b.onRtlSdrPcmSamplesReady({4});check(b.m_audioBuffer.size()==4,"tx-gate");b.m_transmitting=false;b.onRtlSdrIqSamplesReady({1,2,3},48000,14000000);check(b.m_rtlSdrIqRing.isEmpty(),"short-iq");b.onRtlSdrIqSamplesReady({1,2,3,4,5,6,7,8,9,10},48000,14000000);check(b.m_rtlSdrIqCount==10&&b.m_rtlSdrIqWritePos==10&&b.rateSignals==1&&b.viewSignals==1,"iq-start");b.onRtlSdrIqSamplesReady({11,12,13,14,15,16,17,18,19,20},48000,14000000);check(b.m_rtlSdrIqCount==16&&b.m_rtlSdrIqWritePos==4&&b.m_rtlSdrIqRing[0]==17&&b.m_rtlSdrIqRing[4]==5&&b.rateSignals==1,"iq-wrap");b.onRtlSdrIqSamplesReady({1,2,3,4,5,6,7,8},0,14000000);check(b.m_rtlSdrIqWritePos==4,"invalid-rate");}
