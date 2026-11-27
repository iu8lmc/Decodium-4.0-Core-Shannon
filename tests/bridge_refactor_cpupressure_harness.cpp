#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <iostream>
#include "FtRuntimeAdaptivePolicy.hpp"
namespace decodium {void beginActiveSettingsProfile(QSettings&s){s.beginGroup("test-profile");}}
static constexpr int kMaxFtDecodeThreads=24;
static int ftAutoUiReserveForCores(int c){return c>1?1:0;}
static int autoFtThreadCountForCores(int c){return qBound(1,c-ftAutoUiReserveForCores(c),kMaxFtDecodeThreads);}
static int adaptiveInteractiveFtThreadCount(int n){return n;}
static void applyFtOpenMpThreadLimit(int){}
static void bridgeLog(const QString&){}
extern "C" void ftx_ft2_set_sync_candidates_c(int){} extern "C" void ftx_ft4_set_sync_candidates_c(int){}
struct Worker{int cancel=0;void cancelCurrentDecode(){++cancel;}};struct Legacy{int penalty=0;void setFt8DeepThreadPenalty(bool b){penalty=b?1:0;}};
class DecodiumBridge:public QObject {public:
 qint64 m_cpuPressureUntilMs=0,m_cpuPressureSevereUntilMs=0,m_lastCpuPressureLogMs=0,m_ft4AdaptiveLimitUntilMs=0,m_ft4LatencyGuardUntilMs=0,m_lastFt4AdaptiveLogMs=0,m_lastFt4LatencyGuardLogMs=0,m_lastUiStallTickMs=0,m_lastGpuPanadapterProbeMs=0,m_gpuPanadapterStableSinceMs=0,m_gpuPanadapterRetryNotBeforeMs=0;
 int m_ftThreads=8,m_cpuPressureEventCount=0,m_cpuPressureSevereEventCount=0,m_ft8DeepFollowupCooldownSlots=0,m_ft4LateCallbackStreak=0,m_gpuPanadapterStallRetryCount=0;
 bool m_ftThreadsAuto=false,m_lowCpuModeEnabled=false,m_monitoring=false,m_transmitting=false,m_tuning=false,legacyAvailable=false;QString m_mode="FT8";
 std::atomic_bool m_mainWindowRenderActive{true},m_forceGpuPanadapterFft{true},m_gpuPanadapterFftStallGuard{false};std::atomic<quint64> m_mainWindowRenderEpoch{0};QElapsedTimer m_uiStallClock;QVector<qint64> m_recentShortStalls;QMutex m_ft8MicroStallGuardMutex;decodium::decode::Ft8MicroStallGuard m_ft8MicroStallGuard;QHash<quint64,int> m_ft8PendingDeepFollowups,m_ft8PendingSubpassHarvest;QTimer*m_spectrumTimer=nullptr;Worker*m_ft8Worker=nullptr;Legacy*m_legacyBackend=nullptr;
 bool mainWindowRenderActive()const{return m_mainWindowRenderActive.load();}bool legacyBackendAvailable()const{return legacyAvailable;}bool useModernSpectrumFeedWithLegacy()const{return false;}void setGpuPanadapterFftAvailable(bool,const QString&){}void cpuPressureEventCountChanged(){}void cpuPressureSevereEventCountChanged(){}
 bool cpuPressureActive()const;bool cpuPressureSevereActive()const;int effectiveFtThreadLimit()const;int effectiveFtThreadLimitForDecode()const;void setMainWindowRenderActive(bool);void noteMainThreadMicroStall(qint64);void noteFt8AdaptivePeriod(qint64);bool ft4AdaptiveCpuLimitActive(qint64=0)const;void activateFt4AdaptiveCpuLimit(qint64,const QString&,qint64);bool ft4LatencyGuardActive(qint64=0)const;void activateFt4LatencyGuard(qint64,const QString&);int effectiveSpectrumTimerIntervalMs()const;void applyLowCpuRuntimeProfile(const QString&);void noteCpuPressure(const QString&,int,bool);
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir tmp;if(!tmp.isValid())return 1;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,tmp.path());DecodiumBridge b;Worker worker;Legacy legacy;b.m_ft8Worker=&worker;b.m_legacyBackend=&legacy;b.legacyAvailable=true;b.m_monitoring=true;
 int normal=b.effectiveFtThreadLimit();if(normal<1||normal>24)return 2;b.noteCpuPressure("test",2000,true);if(!b.cpuPressureActive()||!b.cpuPressureSevereActive()||worker.cancel!=1||b.m_ft8DeepFollowupCooldownSlots!=8||b.m_cpuPressureEventCount!=1||b.m_cpuPressureSevereEventCount!=1)return 3;
 b.noteCpuPressure("again",1000,true);if(b.m_cpuPressureEventCount!=1||b.m_cpuPressureSevereEventCount!=1)return 4;
 qint64 now=QDateTime::currentMSecsSinceEpoch();b.activateFt4AdaptiveCpuLimit(now,"test",5);if(!b.ft4AdaptiveCpuLimitActive(now+999)||b.ft4AdaptiveCpuLimitActive(now+1000))return 5;b.activateFt4LatencyGuard(now,"test");if(!b.ft4LatencyGuardActive(now+119999)||b.ft4LatencyGuardActive(now+120000))return 6;
 b.m_cpuPressureUntilMs=0;b.m_cpuPressureSevereUntilMs=0;b.m_lowCpuModeEnabled=true;if(b.effectiveFtThreadLimit()>2||b.effectiveSpectrumTimerIntervalMs()!=500)return 7;
 b.setMainWindowRenderActive(false);b.noteMainThreadMicroStall(1000);if(b.m_ft8MicroStallGuard.active())return 8;b.setMainWindowRenderActive(true);b.noteMainThreadMicroStall(100);b.noteMainThreadMicroStall(100);b.noteMainThreadMicroStall(800);if(!b.m_ft8MicroStallGuard.active()||legacy.penalty!=1||!b.m_gpuPanadapterFftStallGuard.load())return 9;
 for(int period=1;period<=17;++period)b.noteFt8AdaptivePeriod(period);if(b.m_ft8MicroStallGuard.active()||legacy.penalty!=0)return 10;
 QTimer timer;b.m_spectrumTimer=&timer;b.applyLowCpuRuntimeProfile("test");if(timer.interval()!=500)return 11;
 std::cout<<"pressure and cancellation; FT4 guards; low CPU; stall guard activation and recovery: OK\n";
}
