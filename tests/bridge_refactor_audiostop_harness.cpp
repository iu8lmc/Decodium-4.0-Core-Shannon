#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include <QVector>
#include <functional>
#include <atomic>
#include <cstdlib>
#include <cassert>
#include <iostream>
#if defined(Q_OS_MACOS)
#include <execinfo.h>
#endif
void bridgeLog(QString const&){}
std::atomic<int> inputStops{0},inputDestroyed{0},sinkDestroyed{0},sinkClosed{0};
class SoundInput:public QObject{public:~SoundInput(){++inputDestroyed;}void stop(){++inputStops;}};
class Sink:public QObject{public:bool discarded=true,open=true;int resets=0;~Sink(){++sinkDestroyed;}void setDiscardSamples(bool v){discarded=v;}void resetDecimationPhase(){++resets;}void setSampleCallback(std::function<void()>){ }bool isOpen()const{return open;}void close(){open=false;++sinkClosed;}};
struct Rtl{bool active=false;bool isActive()const{return active;}};
class DecodiumBridge:public QObject{public:
 quint64 m_audioWatchdogRecoverySerial=0,m_modeChangeAudioRecoverySerial=0;qint64 m_lastAudioCaptureStartMs=1,m_audioUnhealthyStartMs=1,m_audioWatchdogIgnoreUntilMs=1;QString m_lastAudioCaptureStartKey="key";
 bool m_rxAudioSuspendedForTx=true,m_rxAudioKeptOpenForTx=true,m_rxAudioIoRecoveryQueued=true,m_tciAudioCaptureActive=false;Rtl* m_rtlSdrInput=nullptr;SoundInput* m_soundInput=nullptr;Sink* m_audioSink=nullptr;int rtlStops=0,tciStops=0;
 void stopRtlSdrCapture(){++rtlStops;}void stopTciAudioCapture(){++tciStops;}void stopAudioCapture();void teardownAudioCapture();
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);qunsetenv("DECODIUM_LAB_AUDIO_LIFECYCLE");DecodiumBridge b;SoundInput input;Sink sink;b.m_soundInput=&input;b.m_audioSink=&sink;b.stopAudioCapture();assert(inputStops==1&&!sink.discarded&&sink.resets==1&&!b.m_rxAudioSuspendedForTx&&!b.m_rxAudioKeptOpenForTx&&!b.m_rxAudioIoRecoveryQueued&&b.m_audioWatchdogRecoverySerial==1&&b.m_modeChangeAudioRecoverySerial==1&&b.m_lastAudioCaptureStartKey.isEmpty());
 Rtl rtl;rtl.active=true;b.m_rtlSdrInput=&rtl;b.stopAudioCapture();assert(b.rtlStops==1&&inputStops==1);rtl.active=false;b.m_tciAudioCaptureActive=true;b.stopAudioCapture();assert(b.tciStops==1&&inputStops==1);
 DecodiumBridge c;c.m_soundInput=new SoundInput;c.m_audioSink=new Sink;c.teardownAudioCapture();assert(c.m_soundInput==nullptr&&c.m_audioSink==nullptr&&c.rtlStops==1&&inputStops==2&&inputDestroyed==1&&sinkClosed==1&&sinkDestroyed==1);
 QThread worker;worker.start();DecodiumBridge d;d.m_soundInput=new SoundInput;d.m_soundInput->moveToThread(&worker);d.teardownAudioCapture();assert(d.m_soundInput==nullptr&&inputStops==3);worker.quit();assert(worker.wait(3000));
 std::cout<<"capture stop RTL/TCI/local, cancellation serials and threaded teardown: OK\n";}
