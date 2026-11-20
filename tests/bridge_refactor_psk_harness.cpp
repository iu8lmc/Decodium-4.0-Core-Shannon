#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QSettings>
#include <QTemporaryDir>
#include <QVariantMap>
#include <cmath>
#include <iostream>
namespace decodium{void beginActiveSettingsProfile(QSettings&s){s.beginGroup("test-profile");}}
static QString normalizedBaseCall(QString s){return s.trimmed().toUpper().section('/',0,0);}static bool isGridTokenStrict(QString s){return s.size()==4;}
static bool messageIsUnresolvedGridOnlyDecode(const QString&m,QString*){return m.contains("<...>");}static bool isFst4wUnresolvedHashDecode(const QString&mode,const QString&m){return mode.startsWith("FST4W")&&m.contains("<...>");}static bool decodiumPskDebugEnabled(){return false;}static QString extractRightCallsign(const QString&m){return m.split(' ').value(1);}static int secondsFromUtcDisplayToken(QString s){auto t=QTime::fromString(s,"hhmmss");return t.isValid()?t.msecsSinceStartOfDay()/1000:-1;}
struct Reporter{bool enabled=false,connected=false;QString call,antenna;QStringList spots;void setEnabled(bool b){enabled=b;}bool isConnected()const{return connected;}void setLocalStation(const QString&c,const QString&,const QString&,const QString&a,const QString&){call=c;antenna=a;}void addSpot(const QString&c,const QString&,quint64,const QString&,int){spots<<c;}};
struct Map{QVariantMap decay;QVariantMap sourceDecayMinutes()const{return decay;}void setSourceDecayMinutes(const QVariantMap&d){decay=d;}};
class DecodiumBridge:public QObject{public:
 Reporter*m_pskReporter=nullptr;Map*m_mapIntelligenceService=nullptr;bool m_pskReporterEnabled=false,m_catConnected=false;QString m_callsign="9H1SR",m_grid="JM75",m_mode="FT8",m_stationAntenna,m_stationRigInfo,m_catRigName,m_catBackend;double m_frequency=14074000;int m_pskReporterTimeSpanMinutes=15,m_pskMapSpotWindowMinutes=15,events=0;
 QString version()const{return "1.0.737";}QObject*catManagerObj()const{return nullptr;}int periodMsForMode(const QString&)const{return 15000;}void syncSettingToLegacyIni(const QString&,bool){}void pskReporterEnabledChanged(){++events;}void pskReporterConnectedChanged(){++events;}void pskReporterTimeSpanMinutesChanged(){++events;}void pskMapSpotWindowMinutesChanged(){++events;}void settingValueChanged(const QString&,int){}
 bool pskReporterConnected()const;void setPskReporterEnabled(bool);void setPskReporterTimeSpanMinutes(int);void syncPskMapSpotWindowToMap();void setPskMapSpotWindowMinutes(int);QString pskReporterProgramInfo()const;QString pskReporterAntennaInfo()const;QString pskReporterRigInfo()const;void refreshPskReporterLocalStation();void maybeQueuePskReporterSpot(const QVariantMap&,const QString&,bool,const QString&,const QString&,const QString&);void replayPskReporterRecentEntries(const QVariantList&,const QString&);
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir t;if(!t.isValid())return 1;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,t.path());DecodiumBridge b;Reporter p;Map m;b.m_pskReporter=&p;b.m_mapIntelligenceService=&m;b.setPskReporterTimeSpanMinutes(17);b.setPskMapSpotWindowMinutes(58);if(b.m_pskReporterTimeSpanMinutes!=15||b.m_pskMapSpotWindowMinutes!=60||m.decay.value("psk").toInt()!=60)return 2;
 b.m_stationAntenna="  Test dipole  ";b.setPskReporterEnabled(true);if(!p.enabled||p.call!="9H1SR"||p.antenna!="Test dipole"||b.pskReporterRigInfo()!="No CAT control")return 3;
 QVariantMap entry{{"dxCallsign","K1ABC"},{"dxGrid","FN42"},{"mode","FT8"}};b.maybeQueuePskReporterSpot(entry,"CQ K1ABC FN42",true,"1500","-10","FT8");if(p.spots!=QStringList{"K1ABC"})return 4;
 auto bad=entry;bad["partialDecode"]=true;b.maybeQueuePskReporterSpot(bad,"CQ K1ABC FN42",true,"1500","-10","FT8");bad=entry;bad["dxCallsign"]="9H1SR";b.maybeQueuePskReporterSpot(bad,"9H1SR K1ABC -10",false,"1500","-10","FT8");if(p.spots.size()!=1)return 5;
 QString recent=QDateTime::currentDateTimeUtc().time().toString("hhmmss"),old=QDateTime::currentDateTimeUtc().time().addSecs(-600).toString("hhmmss");QVariantMap fresh=entry;fresh["time"]=recent;fresh["message"]="CQ K1ABC FN42";fresh["freq"]="1500";fresh["db"]="-10";QVariantMap stale=fresh;stale["time"]=old;QVariantMap tx=fresh;tx["isTx"]=true;b.replayPskReporterRecentEntries({fresh,stale,tx},"test");if(p.spots.size()!=2)return 6;b.setPskReporterEnabled(false);b.maybeQueuePskReporterSpot(entry,"CQ K1ABC FN42",true,"1500","-10","FT8");if(p.spots.size()!=2||p.enabled)return 7;
 std::cout<<"PSK settings, station identity, valid/partial/self filtering and replay freshness: OK\n";
}
