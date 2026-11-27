#include <QCoreApplication>
#include <QDateTime>
#include <QMap>
#include <QVariant>
#include <cmath>
#include <iostream>
static void bridgeLog(const QString&){}
#include "bridge_refactor_txprotection_helpers.inc"
class DecodiumBridge {public:
 QMap<QString,QVariant> settings;double m_rigSwr=1,m_rigPowerWatts=0;bool m_transmitting=false;QString m_mode="FT8";int m_wsprPowerDbm=37,stops=0,aborts=0,errors=0; qint64 m_lastWsprPowerGuardMs=0;
 QVariant getSetting(const QString&k,const QVariant&d)const{return settings.value(k,d);}void stopTx(){++stops;m_transmitting=false;}void abortAutoCallAfterSwrStop(const QString&){++aborts;}void errorMessage(const QString&){++errors;}void statusMessage(const QString&){}
 bool checkSwrAllowsTransmission(const QString&);void enforceSwrTransmissionLimit(const QString&);void enforceWsprPowerTransmissionLimit(const QString&);
};
#include "bridge_refactor_txprotection.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto check=[](bool ok,const char*n){if(!ok){std::cerr<<n<<" failed\n";std::exit(1);}std::cout<<n<<'\n';};DecodiumBridge b;b.m_rigSwr=4;check(b.checkSwrAllowsTransmission("test"),"swr-disabled");b.settings["CheckSWR"]=true;b.settings["SWRStopThreshold"]=2.5;check(!b.checkSwrAllowsTransmission("test")&&b.errors==1,"swr-block");b.m_rigSwr=2.5;check(b.checkSwrAllowsTransmission("test"),"swr-boundary");b.m_transmitting=true;b.m_rigSwr=2.6;b.enforceSwrTransmissionLimit("test");check(b.stops==1&&b.aborts==1&&!b.m_transmitting,"swr-stop");b.m_transmitting=true;b.m_mode="WSPR";b.settings["PWRandSWR"]=true;b.m_wsprPowerDbm=37;b.m_rigPowerWatts=6;b.enforceWsprPowerTransmissionLimit("test");check(b.stops==1,"power-safe");b.m_rigPowerWatts=20;b.enforceWsprPowerTransmissionLimit("test");check(b.stops==2&&!b.m_transmitting,"power-stop");b.m_transmitting=true;b.enforceWsprPowerTransmissionLimit("test");check(b.stops==2,"power-throttle");b.settings["SWRStopThreshold"]=10;b.m_rigSwr=5;check(b.checkSwrAllowsTransmission("test"),"swr-clamped");}
