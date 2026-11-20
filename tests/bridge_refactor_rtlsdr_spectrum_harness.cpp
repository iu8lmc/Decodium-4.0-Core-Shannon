#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QPointer>
#include <QRunnable>
#include <QThread>
#include <QThreadPool>
#include <QVector>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include "src/rtl/RtlSdrRfSpectrum.h"
class DecodiumBridge:public QObject {public:
 static constexpr int RTL_IQ_RING_SHORTS=32768;QVector<short> m_rtlSdrIqRing;int m_rtlSdrIqCount=0,m_rtlSdrIqWritePos=0,m_rtlSdrRfSampleRate=48000;quint32 m_rtlSdrRfCenterFrequencyHz=14074000;std::atomic<bool> m_rtlSdrSpectrumBusy{false};std::atomic<uint64_t> m_rtlSdrSpectrumSerial{0};qint64 m_lastPanadapterFrameMs=0,m_lastRtlSdrRfLogMs=QDateTime::currentMSecsSinceEpoch();QVector<float> m_lastPanadapterData;float m_lastPanMinDb=0,m_lastPanMaxDb=0,m_lastPanFreqMin=0,m_lastPanFreqMax=0;double m_rtlSdrRfSelectedFrequencyHz=14074000;bool view=true;int published=0,signalsCount=0;
 bool rtlSdrRfView()const{return view;}void publishRemoteWaterfallFrame(const QVector<float>&,float,float,float,float){++published;}void panadapterDataReady(const QVector<float>&,float,float,float,float){++signalsCount;}void processRtlSdrSpectrum();
};
#include "bridge_refactor_rtlsdr_spectrum.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};DecodiumBridge b;b.processRtlSdrSpectrum();check(!b.m_rtlSdrSpectrumBusy,"empty-gate");b.m_rtlSdrIqRing.fill(0,b.RTL_IQ_RING_SHORTS);for(int i=0;i<b.RTL_IQ_RING_SHORTS/2;++i){double a=2*3.141592653589793*128*i/4096;b.m_rtlSdrIqRing[2*i]=short(12000*std::cos(a));b.m_rtlSdrIqRing[2*i+1]=short(12000*std::sin(a));}b.m_rtlSdrIqCount=b.RTL_IQ_RING_SHORTS;b.m_rtlSdrSpectrumBusy=true;b.processRtlSdrSpectrum();check(b.m_rtlSdrSpectrumSerial==0,"busy-gate");b.m_rtlSdrSpectrumBusy=false;b.processRtlSdrSpectrum();QElapsedTimer timer;timer.start();while(b.signalsCount==0&&timer.elapsed()<5000){app.processEvents();QThread::msleep(1);}check(b.signalsCount==1&&b.published==1&&!b.m_rtlSdrSpectrumBusy&&!b.m_lastPanadapterData.isEmpty(),"real-fft-publish");check(b.m_lastPanFreqMin<b.m_rtlSdrRfCenterFrequencyHz&&b.m_lastPanFreqMax>b.m_rtlSdrRfCenterFrequencyHz,"frequency-span");b.processRtlSdrSpectrum();b.view=false;QThreadPool::globalInstance()->waitForDone();app.processEvents();check(b.signalsCount==1&&!b.m_rtlSdrSpectrumBusy,"stale-view-suppressed");}
