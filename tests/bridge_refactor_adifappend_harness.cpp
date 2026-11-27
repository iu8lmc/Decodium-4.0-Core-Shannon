#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUrl>
#include <iostream>
struct QDesktopServices{inline static QUrl last;static bool openUrl(const QUrl& url){last=url;return true;}};
namespace decodium::decode_ui{struct WorkedFilterOptions{bool hideWorkedBand=false,hideWorkedToday=false,hideWorkedEver=false;};}
struct Map{int records=0;void appendAdifRecord(const QByteArray&){++records;}};
static QString bridgeAdifRecordText(const QString& call,const QString&,double,const QString&,const QDateTime&,const QDateTime&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,int,int){return "<CALL:5>"+call;}
class DecodiumBridge:public QObject {public:
 QString path,m_callsign="IU8LMC",m_grid="JM75",m_nextLogName,m_nextLogQth;QSet<QString>m_workedCalls;int m_qsoCountCache=0,worked=0,refresh=0,invalid=0,band=0,rx=0,signalCount=0;Map*m_mapIntelligenceService=nullptr;decodium::decode_ui::WorkedFilterOptions filters;
 QString ensureAdifLogPath(){return path;}QString effectiveAdifLogPath()const{return path;}void appendWorkedQso(const QString&,const QString&,double,const QString&,const QString&){++worked;}void refreshWorkedBeforeDecodeEntriesForCall(const QString&){++refresh;}void invalidateQsoSearchCache(){++invalid;}void warmLogCacheAsync(){}auto workedFilterOptions()const{return filters;}void invalidateDecodeUiPredicateCaches(){}void rebuildBandActivityModel(){++band;}void rebuildRxDecodeModel(){++rx;}void qsoCountChanged(){++signalCount;}void workedCountChanged(){++signalCount;}
 bool appendAdifRecord(const QString&,const QString&,double,const QString&,const QDateTime&,const QDateTime&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,const QString&,int,int);bool openActiveLogbookFolder()const;
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir tmp;DecodiumBridge b;Map map;b.path=tmp.filePath("log.adi");b.m_mapIntelligenceService=&map;b.filters.hideWorkedBand=true;QDateTime t=QDateTime::fromString("2026-11-07T12:00:00Z",Qt::ISODate);bool ok=b.appendAdifRecord("K1ABC","FN42",14074000,"FT8",t,t,"-10","-08","","","","","",0,0);QFile f(b.path);if(!f.open(QIODevice::ReadOnly))qFatal("missing output");auto data=f.readAll();std::cout<<"write|"<<ok<<"|"<<data.contains("<EOH>")<<"|"<<data.contains("<CALL:5>K1ABC")<<"|"<<data.contains("<EOR>")<<"|"<<b.m_workedCalls.contains("K1ABC")<<"|"<<b.m_qsoCountCache<<"|"<<b.worked<<"|"<<b.refresh<<"|"<<b.band<<"|"<<b.rx<<"|"<<map.records<<'\n';ok=b.appendAdifRecord("K2ABC","FN42",14074000,"FT8",t,t,"-10","-08","","","","","",0,0);f.close();if(!f.open(QIODevice::ReadOnly))qFatal("missing output");data=f.readAll();std::cout<<"append|"<<ok<<"|"<<data.count("<EOH>")<<"|"<<data.count("<EOR>")<<"|"<<b.m_qsoCountCache<<'\n';DecodiumBridge bad;bad.path=tmp.path();std::cout<<"failure|"<<bad.appendAdifRecord("K1ABC","FN42",1,"FT8",t,t,"","","","","","","",0,0)<<"|"<<bad.worked<<'\n';bool folderOk=b.openActiveLogbookFolder();if(!folderOk||!QDesktopServices::last.isLocalFile()||QDesktopServices::last.toLocalFile()!=tmp.path())qFatal("wrong existing logbook folder");b.path=tmp.filePath("new/subdir/log.adi");folderOk=b.openActiveLogbookFolder();if(!folderOk||!QDir(tmp.filePath("new/subdir")).exists()||QDesktopServices::last.toLocalFile()!=tmp.filePath("new/subdir"))qFatal("wrong new logbook folder");std::cout<<"folder|"<<folderOk<<'\n';}
