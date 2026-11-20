#include <QCoreApplication>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QVector>
#include <atomic>
#include <cmath>
#include <iostream>
static void bridgeLog(const QString&){}
static int itone[1]={42},stored=0;
static void storeBridgePrecomputedTxWave(const QString&,const QVector<float>&w){stored=w.size();}
static bool isFt2LinkApplicationMode(QString mode){return mode.trimmed().compare("FT2-LINK",Qt::CaseInsensitive)==0;}
struct Cat{bool enabled=true,online=true,tune=false,capture=false,quick=false;QString port="tci";int starts=0,stops=0;double period=0;bool tciAudioEnabled()const{return enabled;}bool connected()const{return online;}QString portType()const{return port;}void setRigTune(bool v){tune=v;}void startRigTxAudio(const QString&,unsigned,double,double,double,bool,bool,double,double p){++starts;period=p;}void stopRigTxAudio(bool q){++stops;quick=q;}void setRigAudio(bool on,double p,int){capture=on;period=p;}};
struct Wav{int count=0;void feedSample(short){++count;}};
class DecodiumBridge{public:
 static constexpr int ASYNC_BUF_SIZE=1024;Cat* m_hamlibCat=nullptr;Wav* m_wavManager=nullptr;QString m_catBackend="hamlib",m_mode="FT8";bool m_tciAudioCaptureActive=false,m_rxAudioSuspendedForTx=false,m_monitoring=false,m_transmitting=false,m_tuning=false; qint64 m_audioUnhealthyStartMs=0,m_audioWatchdogIgnoreUntilMs=0,m_lastTciAudioLogMs=0;QMutex m_audioBufferMutex;QVector<short> m_audioBuffer;short m_asyncAudio[ASYNC_BUF_SIZE]{};std::atomic<quint64> m_asyncAudioPos{0},m_driftExpectedFrames{0};double m_audioLevel=0;int levels=0,health=0,ft2=0;
 int periodMsForMode(const QString&)const{return 15000;}void statusMessage(const QString&){}void audioLevelChanged(){++levels;}void handleAudioHealth(double,double,int,int,int){++health;}void ft2LinkRxSamplesReady(const QVector<short>&,quint64){++ft2;}
 bool usingTciAudioInput()const;bool startTciTxAudioStream(const QVector<float>&,const QString&,unsigned,double,double,double,bool,double);bool startTciTuneAudioStream(double);void stopTciTxAudioStream(bool);void startTciAudioCapture();void stopTciAudioCapture();void onTciPcmSamplesReady(const QVector<short>&);
};
#include "bridge_refactor_tciaudio.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};DecodiumBridge b;check(!b.usingTciAudioInput(),"no-cat");Cat cat;b.m_hamlibCat=&cat;check(b.usingTciAudioInput(),"port-type");cat.online=false;check(!b.usingTciAudioInput(),"offline");cat.online=true;b.startTciAudioCapture();check(cat.capture&&b.m_tciAudioCaptureActive&&cat.period==15,"capture-start");Wav wav;b.m_wavManager=&wav;b.onTciPcmSamplesReady({100,-100});check(b.m_audioBuffer.isEmpty(),"monitor-gate");b.m_monitoring=true;b.onTciPcmSamplesReady({100,-100,32767});check(b.m_audioBuffer.size()==3&&wav.count==3&&b.m_asyncAudioPos==3&&b.m_driftExpectedFrames==3&&b.health==1&&b.ft2==0,"pcm-ft8");b.m_mode="FT2-LINK";b.onTciPcmSamplesReady({100});check(b.ft2==1,"pcm-ft2");b.m_transmitting=true;b.onTciPcmSamplesReady({100});check(b.m_audioBuffer.size()==4,"tx-gate");b.m_transmitting=false;check(!b.startTciTxAudioStream({},"FT8",79,1920,1500,-1,false,0),"empty-wave");check(b.startTciTxAudioStream({0.1f,0.2f},"FT8",79,1920,1500,-1,false,0)&&stored==2&&cat.period==0.1,"wave-tx-simulated");check(b.startTciTuneAudioStream(1500)&&cat.tune,"tune-simulated");b.stopTciTxAudioStream(true);check(!cat.tune&&cat.stops==1&&cat.quick,"tx-stop-simulated");b.stopTciAudioCapture();check(!cat.capture&&!b.m_tciAudioCaptureActive,"capture-stop");}
