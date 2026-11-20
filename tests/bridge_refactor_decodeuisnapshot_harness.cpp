#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSet>
#include <QTextStream>
#include <QVariantList>
#include <QVector>
#include <algorithm>
#include <utility>
#include "DecodeUiFilterPolicy.h"
constexpr int kDecodeUiRefreshBand=1,kDecodeUiRefreshRx=2,kDecodeUiRefreshFullSpectrum=4;
class DecodeListModel{public:struct PreparedSnapshot{QVariantList entries;QStringList keys;};static PreparedSnapshot prepareSnapshot(QVariantList e){PreparedSnapshot p;p.entries=e;for(auto x:e)p.keys<<x.toMap().value("key").toString();return p;}};
static QString decodeMirrorEntryKey(const QVariantMap&e){return e.value("key").toString();}
static QString decodeRxClearEntryKey(const QVariantMap&e){return e.value("key").toString();}
static bool decodeListModel_isTelemetryOnlyMessage(const QString&s){return s=="TELEMETRY";}
static void normalizeDecodeEntriesForDisplay(QVariantList&,int,const QString&){}
static void coalesceRxPaneTxRows(QVariantList&,const QString&){}
#include "bridge_refactor_under_test.inc"
static QString show(const QVariantList& rows){QStringList r;for(auto x:rows){auto v=x.toMap();r<< (v.value("isSeparator").toBool()?"SEP":v.value("key").toString());}return r.join(",");}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);QVariantMap a{{"key","A"},{"time","120000"},{"timestamp",1000},{"message","CQ"}},b{{"key","B"},{"time","120015"},{"timestamp",16000},{"message","CQ"}},tx{{"key","TX"},{"time","120015"},{"isTx",true}},ghost{{"key","G"},{"time","120015"}},telemetry{{"key","T"},{"message","TELEMETRY"}};DecodeUiSnapshotJob j;j.flags=7;j.generation=19;j.mode="FT8";j.bandSource.entries={a,b,tx,ghost,telemetry};j.bandSource.ghostRejected={0,0,0,1,0};j.bandSource.rxMembership={1,1,1,1,1};j.rxSource.entries={a,b,tx};j.rxSource.ghostRejected={0,0,0};j.rxSource.rxMembership={1,1,1};j.includeBandSourceInRx=true;j.showTxMessagesInRx=false;j.clearedRxKeys.insert("B");auto r=prepareDecodeUiSnapshot(j);o<<"normal|"<<r.generation<<'|'<<r.flags<<'|'<<show(r.bandEntries.entries)<<'|'<<show(r.fullSpectrumEntries.entries)<<'|'<<show(r.rxEntries.entries)<<'\n';j.newestFirst=true;j.showPeriodSeparators=false;j.hideTelemetry=false;auto q=prepareDecodeUiSnapshot(j);o<<"reverse|"<<show(q.bandEntries.entries)<<'|'<<show(q.rxEntries.entries)<<'\n';o<<"key|"<<decodeUiPredicateCacheKey(a).startsWith("A|")<<'|'<<decodeUiEntryPassesBandFilters({},false,j)<<'\n';}
