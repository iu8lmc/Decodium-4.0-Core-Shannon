#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QPointer>
#include <QStandardPaths>
#include <QThread>
#include <QVariant>
#include <QVector>
#include <atomic>
#include <iostream>
#define connect(...) ((void)0)
static void bridgeLog(const QString&){}
struct QAudioDevice {QByteArray identifier;QByteArray id()const{return identifier;}QString description()const{return QString::fromUtf8(identifier);}};
static bool tryResolveAudioDeviceByIdentity(const QList<QAudioDevice>&devices,const QString&name,const QString&,QAudioDevice*out,QString*reason){for(auto const&d:devices)if(d.description()==name){*out=d;*reason="name";return true;}return false;}
struct RtlSdrDsp{enum class Demodulator{WeakSignal,Other};static constexpr int kReceiverAudioSampleRate=48000;};
struct RtlSdrInput{struct Config{RtlSdrDsp::Demodulator demodulator=RtlSdrDsp::Demodulator::Other;};bool running=true;Config config;bool isRunning()const{return running;}Config activeConfig()const{return config;}};
struct RtlSdrAudioOutput:QObject{static std::atomic<int> starts,stops,samples;void start(const QAudioDevice&,int){++starts;}void stop(const QString&){++stops;}void enqueueSamples(const QVector<short>&s,int){samples+=s.size();}};
std::atomic<int> RtlSdrAudioOutput::starts{0},RtlSdrAudioOutput::stops{0},RtlSdrAudioOutput::samples{0};
class DecodiumBridge:public QObject{public:
 bool m_rtlSdrAudioStopping=false,m_rtlSdrAudioPlaybackRequested=false,m_monitoring=false,m_transmitting=false,m_tuning=false,enabled=true,audioEnabled=true;QThread* m_rtlSdrAudioThread=nullptr;RtlSdrAudioOutput* m_rtlSdrAudioOutput=nullptr;RtlSdrInput* m_rtlSdrInput=nullptr;QString requestedName,m_rtlSdrStatus;QByteArray m_rtlSdrAudioPlaybackDeviceId;int m_rtlSdrAudioPlaybackSampleRate=0,statusCount=0;qint64 m_rtlSdrAudioRetryAfterMs=0;QAudioDevice defaultDevice;
 QVariant getSetting(const QString&k,const QVariant&d)const{return k=="RtlSdrAudioEnabled"?QVariant(audioEnabled):k=="RtlSdrAudioOutputDevice"?QVariant(requestedName):d;}QAudioDevice cachedDefaultAudioOutput(const QString&,bool)const{return defaultDevice;}QList<QAudioDevice> cachedAudioOutputs(const QString&,bool)const{return {defaultDevice};}bool rtlSdrEnabled()const{return enabled;}void rtlSdrStatusChanged(){++statusCount;}void statusMessage(const QString&){}void errorMessage(const QString&){}
 void startRtlSdrAudioPlayback(int);void stopRtlSdrAudioPlayback(const QString&);void onRtlSdrAudioSamplesReady(const QVector<short>&,int);
};
#include "bridge_refactor_rtlsdr_playback.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};DecodiumBridge b;b.startRtlSdrAudioPlayback(44100);check(!b.m_rtlSdrAudioThread,"rate-gate");b.startRtlSdrAudioPlayback(48000);check(!b.m_rtlSdrAudioThread&&b.m_rtlSdrAudioRetryAfterMs>0&&b.statusCount==1,"missing-device");b.m_rtlSdrAudioRetryAfterMs=0;b.defaultDevice.identifier="test-output";RtlSdrInput input;b.m_rtlSdrInput=&input;b.m_monitoring=true;b.audioEnabled=false;b.onRtlSdrAudioSamplesReady({1,2},48000);check(!b.m_rtlSdrAudioThread,"disabled-gate");b.audioEnabled=true;input.config.demodulator=RtlSdrDsp::Demodulator::WeakSignal;b.onRtlSdrAudioSamplesReady({1,2},48000);check(!b.m_rtlSdrAudioThread,"weak-signal-gate");input.config.demodulator=RtlSdrDsp::Demodulator::Other;b.requestedName="missing-name";b.onRtlSdrAudioSamplesReady({1,2,3},48000);QElapsedTimer t;t.start();while(RtlSdrAudioOutput::samples.load()!=3&&t.elapsed()<3000){app.processEvents();QThread::msleep(1);}check(b.m_rtlSdrAudioPlaybackRequested&&RtlSdrAudioOutput::starts==1&&RtlSdrAudioOutput::samples==3&&b.m_rtlSdrAudioPlaybackDeviceId=="test-output","fallback-and-enqueue");b.m_transmitting=true;b.onRtlSdrAudioSamplesReady({4},48000);check(RtlSdrAudioOutput::samples==3,"tx-gate");auto* out=b.m_rtlSdrAudioOutput;b.stopRtlSdrAudioPlayback("test");check(!b.m_rtlSdrAudioThread&&!b.m_rtlSdrAudioPlaybackRequested&&RtlSdrAudioOutput::stops==1,"stop");delete out;}
