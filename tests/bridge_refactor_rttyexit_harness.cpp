#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QString>
#include <cassert>
#include <iostream>
#include "RttyRxRecovery.h"
void bridgeLog(QString const&){}float rxInputGainFromLevel(double v){return float(v/50.0);}
struct Sink{bool discarded=true;void setDiscardSamples(bool v){discarded=v;}};
class DecodiumBridge:public QObject{public:
 decodium::audio::RttyRxRecovery* m_rttyRxRecovery=nullptr;bool m_monitorRequested=true,m_monitoring=true,m_transmitting=false,m_tuning=false,m_decoPortRemoteKeyed=false,m_decoPortUseRemote=false,m_tciAudioCaptureActive=false,m_rxAudioSuspendedForTx=true,m_rxAudioKeptOpenForTx=true,m_autoRxInputLevel=false,legacy=false,dedicated=false;double m_lastAudioHealthRms=0,m_lastAudioHealthPeak=0,m_rxInputLevel=50;int m_audioInputChannel=0,starts=0,watchdogs=0,rearms=0; qint64 m_lastAudioHealthMs=0;QString m_mode="FT8",m_catMode="USB";Sink* m_audioSink=nullptr;
 bool activeCatReportsPttActive()const{return false;}bool usingTciAudioInput()const{return false;}bool rtlSdrEnabled()const{return false;}bool usingLegacyBackendForRx()const{return legacy;}bool useDedicatedModernAudioCaptureWithLegacy()const{return dedicated;}void startRx(){++rearms;m_monitoring=true;}void startAudioCapture(bool safe){assert(safe);++starts;}void restartAudioCaptureFromWatchdog(QString const&){++watchdogs;}
 void scheduleRttyExitRxRecovery();
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);Sink sink;DecodiumBridge local;local.m_audioSink=&sink;DecodiumBridge legacy;legacy.legacy=true;DecodiumBridge remote;remote.m_decoPortUseRemote=true;DecodiumBridge rtty;rtty.m_mode="RTTY";local.scheduleRttyExitRxRecovery();legacy.scheduleRttyExitRxRecovery();remote.scheduleRttyExitRxRecovery();rtty.scheduleRttyExitRxRecovery();QEventLoop e;QTimer::singleShot(450,&e,&QEventLoop::quit);e.exec();assert(local.starts==1&&local.watchdogs==0&&!local.m_rxAudioSuspendedForTx&&!local.m_rxAudioKeptOpenForTx&&!sink.discarded);assert(legacy.watchdogs==1&&legacy.starts==0);assert(remote.starts==0&&remote.watchdogs==0&&rtty.starts==0&&rtty.watchdogs==0);std::cout<<"RTTY exit local and legacy recovery; remote and RTTY cancellation: OK\n";}
