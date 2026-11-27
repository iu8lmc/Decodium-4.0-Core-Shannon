#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QVector>
#include <atomic>
#include <cassert>
#include <iostream>
void bridgeLog(QString const&){} bool isFt2LinkApplicationMode(QString m){m=m.trimmed().toUpper();return m=="FT2-LINK"||m=="FT2LINK"||m=="D4LINK"||m=="D4 LINK";}
struct Input{};struct Legacy{int monitoringCount=0,rearms=0;void setMonitoring(bool){++monitoringCount;}void rearmMonitoring(QString const&){++rearms;}};
class DecodiumBridge:public QObject {public:
 bool m_monitoring=true,m_transmitting=false,m_tuning=false,m_ft2LinkTxActive=false,m_tciAudioCaptureActive=false,m_legacyPcmSpectrumFeed=false,m_asyncDecodePending=false;
 bool rtl=false,legacy=false,modern=false,dedicated=false,sstv=false;QString m_mode="FT8";Input* m_soundInput=nullptr;Legacy* m_legacyBackend=nullptr;
 qint64 m_ft2LinkPostTxAckGuardUntilMs=0,m_lastTxEndMs=0,m_audioWatchdogIgnoreUntilMs=0,m_lastAudioHealthMs=0,m_lastAudioWatchdogRestartMs=0,m_audioUnhealthyStartMs=0,m_lastAudioWatchdogLogMs=0,m_lastLegacyPcmSampleMs=1,m_lastPanadapterFrameMs=0,m_lastSpectrumRecoveryMs=0;quint64 m_audioWatchdogRecoverySerial=0;
 int m_wfRingPos=1,m_driftFrameCount=1,m_driftExpectedFrames=1,stops=0,starts=0,safeStarts=0,rtlStops=0,rtlStarts=0,rearms=0,syncs=0,status=0;std::atomic<quint64> m_asyncAudioPos{3};QVector<int> m_spectrumBuf{1},m_lastPanadapterData{1};QElapsedTimer m_driftClock;
 bool rtlSdrEnabled()const{return rtl;}bool usingLegacyBackendForRx()const{return legacy;}bool useModernSpectrumFeedWithLegacy()const{return modern;}bool useDedicatedModernAudioCaptureWithLegacy()const{return dedicated;}bool nativeSstvRxForcesDedicatedAudioCapture()const{return sstv;}
 void stopRtlSdrCapture(){++rtlStops;}void startRtlSdrCapture(){++rtlStarts;}void rearmLegacyPcmSpectrumFeed(QString const&){++rearms;}void stopAudioCapture(){++stops;}void startAudioCapture(bool safe=false){++starts;if(safe)++safeStarts;}void syncLegacyBackendTxState(){++syncs;}void resetRxPeriodAccumulation(bool){}void statusMessage(QString const&){++status;}
 void restartAudioCaptureFromWatchdog(QString const&);
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto now=QDateTime::currentMSecsSinceEpoch();
 DecodiumBridge guard;guard.m_mode="FT2-LINK";guard.m_ft2LinkTxActive=true;guard.restartAudioCaptureFromWatchdog("test");assert(guard.starts==0&&guard.m_audioWatchdogIgnoreUntilMs>=now+14900);
 DecodiumBridge tci;tci.m_tciAudioCaptureActive=true;tci.m_lastAudioHealthMs=now;tci.m_audioUnhealthyStartMs=1;tci.restartAudioCaptureFromWatchdog("tci");assert(tci.starts==0&&tci.m_audioUnhealthyStartMs==0);
 DecodiumBridge rtl;rtl.rtl=true;rtl.restartAudioCaptureFromWatchdog("rtl");assert(rtl.rtlStops==1&&rtl.rtlStarts==1);
 DecodiumBridge legacy;legacy.legacy=true;legacy.restartAudioCaptureFromWatchdog("legacy");assert(legacy.rearms==1&&legacy.starts==0);
 Legacy backend;DecodiumBridge direct;direct.legacy=true;direct.modern=true;direct.m_legacyBackend=&backend;direct.restartAudioCaptureFromWatchdog("direct");assert(direct.m_legacyPcmSpectrumFeed&&backend.monitoringCount==1&&backend.rearms==1&&direct.syncs==1);
 Input input;DecodiumBridge safe;safe.m_soundInput=&input;safe.m_mode="FT2";safe.restartAudioCaptureFromWatchdog("safe");assert(safe.starts==1&&safe.safeStarts==1&&safe.m_asyncAudioPos==0);safe.m_lastAudioHealthMs=QDateTime::currentMSecsSinceEpoch()+1000;
 DecodiumBridge stale;stale.m_soundInput=&input;stale.restartAudioCaptureFromWatchdog("stale");++stale.m_audioWatchdogRecoverySerial;
 DecodiumBridge escalation;escalation.m_soundInput=&input;escalation.restartAudioCaptureFromWatchdog("no-callback");
 QEventLoop loop;QTimer::singleShot(3700,&loop,&QEventLoop::quit);loop.exec();assert(safe.stops==0&&safe.starts==1&&stale.stops==0&&stale.starts==1);
#if defined(Q_OS_LINUX)
 assert(escalation.stops==0&&escalation.starts==1);
#else
 assert(escalation.stops==1&&escalation.starts==2);
#endif
 std::cout<<"watchdog FT2-Link, TCI, RTL, legacy, safe recovery and stale timer: OK\n";}
