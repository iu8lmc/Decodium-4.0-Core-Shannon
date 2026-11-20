#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QList>
#include <QThread>
#include <QString>
#include <atomic>
#include <cassert>
#include <iostream>
void diagWarn(QString const&){}
#define DIAG_WARN(x) diagWarn(x)
class DecodiumBridge {public:
 qint64 m_cpuPressureUntilMs=0,m_lastUiStallTickMs=0,m_lastUiStallLogMs=0;QElapsedTimer m_uiStallClock;QList<qint64> m_recentShortStalls;
 bool m_monitoring=false,m_transmitting=false,m_tuning=false,m_legacyPcmSpectrumFeed=false,m_decoding=false,render=true;std::atomic<bool> m_panadapterComputeBusy{false};QString m_mode="FT8";int mild=0,severe=0;
 bool mainWindowRenderActive()const{return render;}bool useModernSpectrumFeedWithLegacy()const{return false;}int periodMsForMode(QString const&)const{return 15000;}
 void noteCpuPressure(QString const&,int,bool s){if(s)++severe;else ++mild;}
 bool isUiStallActive(int,int)const;void updateUiStallDiagnostics();
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;assert(!b.isUiStallActive(500,5000));b.m_cpuPressureUntilMs=QDateTime::currentMSecsSinceEpoch()+1000;assert(b.isUiStallActive(500,5000));b.m_cpuPressureUntilMs=0;
 b.updateUiStallDiagnostics();QThread::msleep(10);b.updateUiStallDiagnostics();b.m_recentShortStalls<<1;b.render=false;QThread::msleep(380);b.updateUiStallDiagnostics();assert(b.m_recentShortStalls.empty()&&b.mild==0);
 b.render=true;b.m_monitoring=true;for(int i=0;i<4;i++){QThread::msleep(380);b.updateUiStallDiagnostics();}assert(b.mild==1&&b.m_recentShortStalls.empty());
 QThread::msleep(1120);b.updateUiStallDiagnostics();assert(b.severe==1);b.m_monitoring=false;QThread::msleep(650);b.updateUiStallDiagnostics();assert(b.severe==1&&b.mild==1);
 std::cout<<"UI stall windows, hidden reset, short-stall burst and severe gating: OK\n";}
