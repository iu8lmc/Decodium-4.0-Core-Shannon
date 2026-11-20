#include <QtCore>
#include <cstdio>
#include "Radio.hpp"
namespace decodium {void beginActiveSettingsProfile(QSettings& s){s.beginGroup("lab");}}
static void bridgeLog(QString const&){}
class DecodiumBridge {public:
 int m_maxQsoPerCall=0,notifications=0;QDate m_qsoCountDay;QHash<QString,int> m_qsoCountByKey;QString filePath;
 QString defaultAdifLogPath()const{return filePath;}void maxQsoPerCallChanged(){++notifications;}
 void setMaxQsoPerCall(int);void loadQsoCountFromLog();
};
#include "bridge_refactor_qsolimit.inc"
static QString field(QString n,QString v){return QString("<%1:%2>%3").arg(n).arg(v.size()).arg(v);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir dir;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());DecodiumBridge b;b.filePath=dir.filePath("qso.adi");
 auto row=[](QString d,QString c,QString band,QString mode,QString sub){return field("QSO_DATE",d)+field("CALL",c)+field("BAND",band)+field("MODE",mode)+field("SUBMODE",sub)+"<EOR>\n";};
 QString today=QDateTime::currentDateTimeUtc().date().toString("yyyyMMdd"),old=QDateTime::currentDateTimeUtc().date().addDays(-1).toString("yyyyMMdd");
 QFile f(b.filePath);if(!f.open(QIODevice::WriteOnly))return 2;f.write((row(today,"k1abc/p","20m","MFSK","FT2")+row(today,"K1ABC","20M","MFSK","FT2")+row(today,"9H1SR","40m","FT8","")+row(old,"K1ABC","20M","FT8","")+row(today,"","20M","FT8","")).toUtf8());f.close();
 for(int limit:{-1,1,1,8,0}){b.setMaxQsoPerCall(limit);b.loadQsoCountFromLog();auto keys=b.m_qsoCountByKey.keys();std::sort(keys.begin(),keys.end());printf("limit=%d notify=%d",b.m_maxQsoPerCall,b.notifications);for(auto const& k:keys)printf(" %s=%d",k.toUtf8().constData(),b.m_qsoCountByKey.value(k));QSettings settings(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium3");decodium::beginActiveSettingsProfile(settings);printf(" persisted=%d\n",settings.value("MaxQsoPerCall",-1).toInt());}
 b.filePath=dir.filePath("missing.adi");b.setMaxQsoPerCall(1);b.loadQsoCountFromLog();printf("missing=%d\n",int(b.m_qsoCountByKey.size()));
}
