#include <QCoreApplication>
#include <QDateTime>
#include <QThread>
#include <QVariant>
#include <QMap>
#include "StationTelemetryCodec.hpp"
#include <iostream>
static void bridgeLog(const QString&){}
static bool isGridTokenStrict(const QString&s){return s=="JM75";}
class DecodiumBridge:public QObject {public:
 bool m_tuning=false,m_manualTxHold=false,m_stationTelemetryPending=false,m_transmitting=false,defer=false,late=false,allowTx=false,m_telemetryTxActive=false;
 qint64 m_stationTelemetryPendingSinceMs=0,m_weatherFetchedAtMs=0;int m_mode=0,m_weatherTempC=19,m_weatherWindKmh=22,m_weatherWindDirDeg=90,m_stationPowerWatts=100,arms=0,starts=0;QString m_grid="JM75",m_pendingTelemetryHex,status,armReason;decodium::telemetry::SkyCondition m_weatherSky=decodium::telemetry::SkyCondition::Clear;QMap<QString,QVariant>settings;
 int effectivePeriodMsForMode(int)const{return 15000;}bool shouldDeferManualSyncTxStart()const{return defer;}
 bool isFt2AsyncTxStartTooLate(int*a,int*b,int*c){*a=3000;*b=2000;*c=750;return late;}
 void armStationTelemetryForNextSlot(const QString&r,qint64=-1){++arms;armReason=r;}
 QVariant getSetting(const QString&k,const QVariant&v)const{return settings.value(k,v);}
 void startTx(){++starts;m_transmitting=allowTx;}void statusMessage(const QString&s){status=s;}
 void sendStationTelemetry();
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge a;a.m_tuning=true;a.m_stationTelemetryPending=true;a.sendStationTelemetry();std::cout<<"tune|"<<a.m_stationTelemetryPending<<"|"<<a.starts<<'\n';DecodiumBridge b;b.m_stationTelemetryPending=true;b.m_stationTelemetryPendingSinceMs=QDateTime::currentMSecsSinceEpoch()-70000;b.sendStationTelemetry();std::cout<<"expired|"<<b.m_stationTelemetryPending<<"|"<<b.starts<<'\n';DecodiumBridge c;c.defer=true;c.sendStationTelemetry();std::cout<<"defer|"<<c.arms<<"|"<<c.starts<<'\n';DecodiumBridge d;d.late=true;d.sendStationTelemetry();std::cout<<"late|"<<d.arms<<"|"<<d.armReason.contains("FT2 async")<<'\n';DecodiumBridge e;e.m_grid="invalid";e.sendStationTelemetry();std::cout<<"grid|"<<e.starts<<'\n';DecodiumBridge f;f.allowTx=true;f.settings["WeatherApiEnable"]=true;f.m_weatherFetchedAtMs=QDateTime::currentMSecsSinceEpoch();f.sendStationTelemetry();decodium::telemetry::StationTelemetryFields fields;bool decoded=decodium::telemetry::decodeStationTelemetryHex(f.m_pendingTelemetryHex,fields);std::cout<<"send|"<<f.starts<<"|"<<f.m_telemetryTxActive<<"|"<<decoded<<"|"<<(fields.grid4=="JM75")<<"|"<<fields.tempC<<"|"<<fields.windSpeedKmh<<"|"<<fields.powerWatts<<'\n';DecodiumBridge g;g.sendStationTelemetry();std::cout<<"failed|"<<g.starts<<"|"<<g.m_telemetryTxActive<<"|"<<g.m_pendingTelemetryHex.isEmpty()<<"|"<<!g.status.isEmpty()<<'\n';DecodiumBridge h;h.allowTx=true;h.sendStationTelemetry();decodium::telemetry::decodeStationTelemetryHex(h.m_pendingTelemetryHex,fields);std::cout<<"weather-off|"<<(fields.tempC==decodium::telemetry::kTempUnknown)<<"|"<<fields.windSpeedKmh<<'\n';}
