#include <QCoreApplication>
#include <QTimer>
#include <QString>
#include <cassert>
#include <iostream>
#ifdef SIMULATE_WINDOWS
#define Q_OS_WIN 1
using HANDLE=void*;using DWORD=unsigned long;
constexpr DWORD REALTIME_PRIORITY_CLASS=0x100,HIGH_PRIORITY_CLASS=0x80,ABOVE_NORMAL_PRIORITY_CLASS=0x8000,NORMAL_PRIORITY_CLASS=0x20;
DWORD fakePriority=NORMAL_PRIORITY_CLASS;bool fakeSetOk=true;int setCalls=0;
HANDLE GetCurrentProcess(){return nullptr;}DWORD GetPriorityClass(HANDLE){return fakePriority;}DWORD GetLastError(){return 5;}bool SetPriorityClass(HANDLE,DWORD value){++setCalls;if(!fakeSetOk)return false;fakePriority=value;return true;}
#endif
void bridgeLog(QString const&){}
class DecodiumBridge{public:bool m_spectrumTimerPausedForTx=false,m_monitoring=true,m_transmitting=false,m_tuning=false,m_txAudioPriorityBoosted=false,m_lowCpuModeEnabled=false,pressure=false;quint32 m_txAudioPreviousPriorityClass=0;QTimer* m_spectrumTimer=nullptr;bool cpuPressureActive()const{return pressure;}void suspendNonAudioTxWork(QString const&);void resumeNonAudioTxWork(QString const&);void applyTxAudioSchedulingBoost(QString const&);void restoreTxAudioSchedulingBoost(QString const&);};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;QTimer timer;b.m_spectrumTimer=&timer;b.m_spectrumTimerPausedForTx=true;b.suspendNonAudioTxWork("test");assert(!b.m_spectrumTimerPausedForTx);b.m_spectrumTimerPausedForTx=true;b.resumeNonAudioTxWork("test");assert(!b.m_spectrumTimerPausedForTx&&timer.isActive());timer.stop();b.m_transmitting=true;b.m_spectrumTimerPausedForTx=true;b.resumeNonAudioTxWork("busy");assert(!timer.isActive());
#ifdef SIMULATE_WINDOWS
 b.m_transmitting=false;b.applyTxAudioSchedulingBoost("normal");assert(b.m_txAudioPriorityBoosted&&fakePriority==ABOVE_NORMAL_PRIORITY_CLASS&&b.m_txAudioPreviousPriorityClass==NORMAL_PRIORITY_CLASS);b.applyTxAudioSchedulingBoost("again");assert(setCalls==1);b.restoreTxAudioSchedulingBoost("done");assert(!b.m_txAudioPriorityBoosted&&fakePriority==NORMAL_PRIORITY_CLASS);b.m_lowCpuModeEnabled=true;b.applyTxAudioSchedulingBoost("lowcpu");assert(fakePriority==HIGH_PRIORITY_CLASS);fakeSetOk=false;b.restoreTxAudioSchedulingBoost("failure");assert(b.m_txAudioPriorityBoosted);fakeSetOk=true;b.restoreTxAudioSchedulingBoost("retry");assert(!b.m_txAudioPriorityBoosted&&fakePriority==NORMAL_PRIORITY_CLASS);fakeSetOk=false;b.applyTxAudioSchedulingBoost("failure");assert(!b.m_txAudioPriorityBoosted);
#else
 b.applyTxAudioSchedulingBoost("noop");b.restoreTxAudioSchedulingBoost("noop");assert(!b.m_txAudioPriorityBoosted);
#endif
 std::cout<<"TX scheduling spectrum and priority decisions: OK\n";}
