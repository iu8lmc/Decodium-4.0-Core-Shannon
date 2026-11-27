#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QVariant>
#include <atomic>
#include <iostream>
#include "DecodiumBridgeQsoCacheInternal.h"
static int snapshots=0;
static void bridgeLog(const QString&) {}
static int countAdifRecordsLightweight(const QString& path) { return QFile::exists(path) ? 2 : 0; }
static QsoLogSnapshot buildSnapshot(const QString& path,const QString&) {
 ++snapshots; QFileInfo info(path); QsoLogSnapshot s; s.path=path;s.modified=info.lastModified();s.size=info.exists()?info.size():0;
 s.rows={QVariantMap{{"call","K1ABC"},{"grid","FN42"},{"band","20m"},{"mode","FT8"},{"comment","ALPHA"},{"dateTime","2026-11-07 12:00"}},QVariantMap{{"call","IU8LMC"},{"grid","JN70"},{"band","40m"},{"mode","SSB"},{"comment","BETA"},{"dateTime","2026-11-06 12:00"}}};s.stats={{"totalQsos",2}};return s;
}
#define buildQsoLogSnapshot buildSnapshot
static QVariantMap statsFromRows(const QVariantList& rows) {return {{"totalQsos",rows.size()}};}
#define qsoStatsFromRows statsFromRows
class DecodiumBridge:public QObject {
public:
 QString path,m_grid; mutable int m_qsoCountCache=-1;mutable QMutex m_qsoSearchCacheMutex;mutable bool m_qsoSearchCacheReady=false,m_qsoSearchWarmupInProgress=false; mutable QString m_qsoSearchCachePath; mutable QDateTime m_qsoSearchCacheModified; mutable qint64 m_qsoSearchCacheSize=-1;mutable QVariantList m_qsoSearchCacheRows;mutable QVariantMap m_qsoSearchCacheStats;std::atomic<quint64> m_qsoSearchCacheGeneration{0};int notifications=0;
 QString effectiveAdifLogPath()const{return path;}void qsoLogCacheChanged(){++notifications;}
 int qsoCount()const;void invalidateQsoSearchCache();void warmLogCacheAsync();QVariantList searchQsos(const QString&,const QString&,const QString&,const QString&,const QString&)const;QVariantMap getQsoStats()const;
};
#include "bridge_refactor_qso_cache.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir dir;DecodiumBridge b;b.path=dir.filePath("test.adi");QFile f(b.path);if(!f.open(QIODevice::WriteOnly))return 2;f.write("test");f.close();
 auto check=[](bool ok,const char* name){if(!ok){std::cerr<<name<<" failed\n";std::exit(1);}std::cout<<name<<"\n";};
 check(b.qsoCount()==2,"count");check(b.searchQsos("","","","","").size()==2,"all");check(b.searchQsos("k1abc","20M","ft8","2026-11-07","2026-11-07").size()==1,"filters");check(b.searchQsos("beta","20M","","","").isEmpty(),"filter-miss");check(b.searchQsos("fn42","","","","").size()==1,"grid");check(b.getQsoStats().value("totalQsos").toInt()==2,"stats");check(snapshots==1,"cache-hit");b.invalidateQsoSearchCache();check(b.searchQsos("","","","","").size()==2&&snapshots==2,"invalidate");b.invalidateQsoSearchCache();b.warmLogCacheAsync();QElapsedTimer t;t.start();while(b.notifications==0&&t.elapsed()<5000){app.processEvents();QThread::msleep(1);}check(b.notifications==1&&b.getQsoStats().value("totalQsos").toInt()==2,"warmup");
}
