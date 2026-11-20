#include <QCoreApplication>
#include <QDateTime>
#include <QString>
#include <QStringList>
#include <cassert>
#include <iostream>
QStringList logs;
void bridgeLog(QString const& s){logs<<s;}
QString audioDeviceIdSettingForLog(QString s){s=s.trimmed();return s.isEmpty()?QStringLiteral("<unset>"):s;}
bool isFt2LinkApplicationMode(QString mode){mode=mode.trimmed().toUpper();return mode=="FT2-LINK"||mode=="FT2LINK"||mode=="D4LINK"||mode=="D4 LINK";}
struct Rtl {bool running=false;bool isRunning()const{return running;}};
class DecodiumBridge {public:
 void* m_soundInput=nullptr;bool m_tciAudioCaptureActive=false,m_monitoring=false,m_pendingRxAudioStartupHealthLog=false,m_transmitting=false,m_tuning=false,m_rxAudioSuspendedForTx=false;
 Rtl* m_rtlSdrInput=nullptr;qint64 m_audioUnhealthyStartMs=0,m_audioOverdriveStartMs=0,m_lastAudioHealthMs=0,m_rxAudioStartupStartMs=0,m_audioWatchdogIgnoreUntilMs=0,m_lastAudioOverdriveWarningMs=0,m_lastAudioWatchdogLogMs=0;
 double m_lastAudioHealthRms=0,m_lastAudioHealthPeak=0,m_rxInputLevel=50;int m_lastAudioHealthClippedSamples=0,m_lastAudioHealthSamples=0,adjusts=0,warnings=0;QString m_activeRxInputDeviceName,m_activeRxInputDeviceId,m_mode="FT8";
 void updateAutoRxInputLevel(double,double,int,int,qint64){++adjusts;}void statusMessage(QString const&){++warnings;}void handleAudioHealth(double,double,int,int,int);
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;b.m_audioUnhealthyStartMs=1;b.handleAudioHealth(0,0,0,0,100);assert(b.m_audioUnhealthyStartMs==0&&b.adjusts==0);
 Rtl rtl;rtl.running=true;b.m_monitoring=true;b.m_rtlSdrInput=&rtl;b.m_pendingRxAudioStartupHealthLog=true;b.m_rxAudioStartupStartMs=QDateTime::currentMSecsSinceEpoch()-2000;b.m_mode="FT2-LINK";b.handleAudioHealth(0,0,0,0,100);assert(!b.m_pendingRxAudioStartupHealthLog&&!logs.isEmpty()&&logs.last().contains("startup health"));b.m_rtlSdrInput=nullptr;b.adjusts=0;
 b.m_monitoring=true;b.m_tciAudioCaptureActive=true;b.m_mode="FT2-LINK";b.handleAudioHealth(0,0,0,0,100);assert(b.m_audioUnhealthyStartMs==0&&b.adjusts==1);
 b.m_mode="Q65-60";b.handleAudioHealth(0,0,0,0,100);assert(b.m_audioUnhealthyStartMs==0&&b.adjusts==2);
 b.m_mode="FT8";b.m_audioUnhealthyStartMs=QDateTime::currentMSecsSinceEpoch()-9000;b.handleAudioHealth(0,0,0,0,100);assert(!logs.isEmpty()&&logs.last().contains("capture retained")&&b.adjusts==3);
 auto now=QDateTime::currentMSecsSinceEpoch();b.m_audioOverdriveStartMs=now-4000;b.m_lastAudioOverdriveWarningMs=0;b.m_audioUnhealthyStartMs=now-9000;b.handleAudioHealth(.5,.99,200,80,100);assert(b.warnings==1&&b.m_lastAudioHealthClippedSamples==80);
 b.m_transmitting=true;b.handleAudioHealth(.5,.99,200,80,100);assert(b.m_audioUnhealthyStartMs==0&&b.m_audioOverdriveStartMs==0&&b.adjusts==4);
 b.m_transmitting=false;b.handleAudioHealth(.05,.5,100,0,100);assert(b.m_audioUnhealthyStartMs==0&&b.m_audioOverdriveStartMs==0&&b.adjusts==5);
 std::cout<<"audio health capture gate, quiet modes, clipping, overdrive and TX hold: OK\n";}
