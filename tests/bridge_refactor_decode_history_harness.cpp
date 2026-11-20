#include <QtCore>
#include <QtSql>
#include <utility>
#include <cstdio>
static void bridgeLog(const QString&){}static void decodiumBridgeLogbookLog(const QString&){}
class DecodiumBridge {public:
 void pruneDecodeHistoryOlderThan(int);QSqlDatabase openHistoryReadDb()const;QVariantList queryDecodeHistory(QVariantMap const&)const;QVariantList queryDecodeSessions()const;QStringList queryDecodeBands()const;QStringList queryDecodeModes()const;QVariantMap queryDecodeStats()const;bool exportAdifFromHistory(QVariantMap const&,QString const&);
};
#include "bridge_refactor_decode_history.inc"
static void check(bool x,const char*s){if(!x){fprintf(stderr,"FAIL %s\n",s);exit(1);}puts(s);}
static void exec(QSqlQuery&q,const QString&s){if(!q.exec(s)){fprintf(stderr,"SQL %s\n",qPrintable(q.lastError().text()));exit(1);}}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir root;check(root.isValid(),"isolated");QSqlDatabase db=QSqlDatabase::addDatabase("QSQLITE");db.setDatabaseName(root.filePath("test.sqlite"));check(db.open(),"open");QSqlQuery q(db);
 exec(q,"CREATE TABLE sessions(id INTEGER PRIMARY KEY, started_utc INTEGER, ended_utc INTEGER, operator TEXT, station TEXT, decodium_ver TEXT)");exec(q,"CREATE TABLE decodes(id INTEGER PRIMARY KEY, ts_utc INTEGER, band TEXT, freq_hz INTEGER, mode TEXT, submode TEXT, callsign_dx TEXT, callsign_de TEXT, grid TEXT, snr_db INTEGER, dt_s REAL, df_hz INTEGER, message TEXT, session_id INTEGER)");
 exec(q,"INSERT INTO sessions VALUES(1,10,20,'TEST','TEST','1'),(2,10,20,'TEST','TEST','1'),(3,10,20,'TEST','TEST','1')");qint64 now=QDateTime::currentMSecsSinceEpoch();
 q.prepare("INSERT INTO decodes VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");for(int i=0;i<3;i++){QVariantList vals={i+1,now-(i==0?2LL*86400000:1000*i),i==2?"40M":"20M",14074000,i==2?"FT4":"FT8","",i==1?"K1ABC":"AB1CD","9H1SR","FN42",-12,.2,1200,i==1?"CQ K1ABC FN42":"CQ AB1CD FN42",i==0?1:2};for(int n=0;n<vals.size();++n)q.bindValue(n,vals[n]);check(q.exec(),"insert");}
 DecodiumBridge b;check(b.queryDecodeStats().value("total").toInt()==3,"stats");check(b.queryDecodeSessions().size()==3,"sessions");check(b.queryDecodeBands()==QStringList({"20M","40M"})&&b.queryDecodeModes()==QStringList({"FT4","FT8"}),"distinct");
 check(b.queryDecodeHistory({{"callsign","k1abc"}}).size()==1,"callsign");check(b.queryDecodeHistory({{"band","40m"},{"mode","ft4"}}).size()==1,"band-mode");check(b.queryDecodeHistory({{"sessionId",2},{"limit",1}}).size()==1,"limit-session");check(b.queryDecodeHistory({{"callsign","' OR 1=1 --"}}).isEmpty(),"parameterized");
 QString out=root.filePath("out.adi");check(b.exportAdifFromHistory({{"callsign","k1abc"}},out),"export");QFile f(out);check(f.open(QIODevice::ReadOnly)&&f.readAll().contains("<CALL:5>K1ABC"),"export-contents");f.close();
 b.pruneDecodeHistoryOlderThan(1);check(b.queryDecodeStats().value("total").toInt()==2 && b.queryDecodeStats().value("session_count").toInt()==1,"prune");check(b.queryDecodeHistory({{"fromUtc",now-5000}}).size()==2,"date-filter");
}
