#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QString>
#include <QVector>
#include <atomic>
#include <cassert>
#include <iostream>
#include "AudioModeChangeCapturePolicy.h"
void bridgeLog(QString const&){} float rxInputGainFromLevel(double v){return float(v/50.0);}bool isNativePcmKeyboardMode(QString const& v){return v=="CW"||v=="JTTY"||v=="SSB";}
struct Input{float gain=0;void setInputGain(float v){gain=v;}};struct Rtl{bool isActive()const{return false;}};
struct Legacy{bool on=false;bool monitoring()const{return on;}void setMonitoring(bool v){on=v;}};
class DecodiumBridge:public QObject {public:
 bool m_monitoring=true,m_transmitting=false,m_tuning=false,m_tciAudioCaptureActive=false,m_monitorRequested=true,m_asyncDecodePending=false,targetLegacy=false,dedicated=false,nativeSstv=false;
 Input* m_soundInput=nullptr;Rtl* m_rtlSdrInput=nullptr;Legacy* m_legacyBackend=nullptr;QString m_mode="FT8";double m_rxInputLevel=50;
 quint64 m_modeChangeAudioRecoverySerial=0,m_periodTimerSessionId=7; qint64 m_audioWatchdogIgnoreUntilMs=0,m_audioUnhealthyStartMs=1,m_lastPanadapterFrameMs=0,m_lastSpectrumRecoveryMs=1;int m_wfRingPos=1,m_driftFrameCount=1,m_driftExpectedFrames=1,stops=0,starts=0,startRxCount=0,syncs=0;std::atomic<quint64> m_asyncAudioPos{5};QVector<int> m_spectrumBuf{1},m_lastPanadapterData{1};QElapsedTimer m_driftClock;
 bool usingLegacyBackendForRx()const{return targetLegacy;}bool useDedicatedModernAudioCaptureWithLegacy()const{return dedicated;}bool nativeSstvRxForcesDedicatedAudioCapture()const{return nativeSstv;}bool legacyBackendAvailable()const{return m_legacyBackend;}
 void stopAudioCapture(){++stops;m_tciAudioCaptureActive=false;}void startAudioCapture(){++starts;}void startRx(){++startRxCount;m_monitoring=true;}void syncLegacyBackendState(){++syncs;}
 void resetRxPeriodAccumulation(bool){}void resetTimeSyncDecodeMetrics(){}
 void restartAudioCaptureForModeChange(QString const&);void scheduleMonitorRecovery(QString const&,quint64,bool);void scheduleModeChangeMonitorRecovery(QString const&,QString const&,quint64,bool);
};
#include "bridge_refactor_under_test.inc"
void wait(int ms){QEventLoop e;QTimer::singleShot(ms,&e,&QEventLoop::quit);e.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;Input in;b.m_soundInput=&in;b.m_mode="FT2";b.restartAudioCaptureForModeChange("CW");assert(b.stops==0&&b.starts==0&&in.gain==1&&b.m_asyncAudioPos==0&&b.m_modeChangeAudioRecoverySerial==1);
 b.targetLegacy=true;b.restartAudioCaptureForModeChange("CW");assert(b.stops==1&&b.starts==0&&b.m_audioWatchdogIgnoreUntilMs-QDateTime::currentMSecsSinceEpoch()>14000);
 b.targetLegacy=false;b.m_tciAudioCaptureActive=true;b.restartAudioCaptureForModeChange("FT8");wait(10);assert(b.stops==2&&b.starts==1);
 b.m_tciAudioCaptureActive=true;b.restartAudioCaptureForModeChange("FT8");++b.m_modeChangeAudioRecoverySerial;wait(10);assert(b.starts==1);
 b.m_monitoring=false;b.scheduleMonitorRecovery("test",8,true);wait(400);assert(b.startRxCount==0);b.scheduleModeChangeMonitorRecovery("FT8","FT4",7,true);wait(400);assert(b.startRxCount==1);
 Legacy legacy;b.m_legacyBackend=&legacy;b.targetLegacy=true;b.scheduleMonitorRecovery("legacy",7,true);wait(400);assert(legacy.on&&b.syncs==1);
 std::cout<<"mode capture preserve, legacy handoff, stale reopen and monitor recovery: OK\n";}
