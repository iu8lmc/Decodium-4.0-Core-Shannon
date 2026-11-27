#include "AdifDocumentInternal.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QSet>
#include <QTextStream>
using namespace decodium::adif_document;
void rebuildWorkedCallsFromDocument(QSet<QString>& calls, QList<ParsedAdifRecord> const& records) { calls.clear(); for (const auto& r:records) calls.insert(r.fields.value("CALL")); }
QString lastLog;
void bridgeLog(const QString& s) { lastLog=s; }
struct Map { QString path; void reloadFromAdif(const QString& p){path=p;} };
class DecodiumBridge {
public:
 QString path; QSet<QString> m_workedCalls; int m_qsoCountCache=-1, rebuilt=-1, invalid=0,warm=0, notifications=0; Map* m_mapIntelligenceService=nullptr;
 QString ensureAdifLogPath(){return path;} void rebuildWorkedSetsFromAdifRecords(const QList<ParsedAdifRecord>& r){rebuilt=r.size();} void invalidateQsoSearchCache(){++invalid;} void warmLogCacheAsync(){++warm;} void qsoCountChanged(){++notifications;} void workedCountChanged(){++notifications;} void qsoLogCacheChanged(){++notifications;} void activeLogbookChanged(){++notifications;}
 void reloadActiveLogbookState(const QString&);
};
#define emit
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir dir;DecodiumBridge b;Map m;b.m_mapIntelligenceService=&m;b.path=dir.filePath("test.adi");QFile file(b.path);if(!file.open(QIODevice::WriteOnly))return 1;file.write("ADIF\n<EOH>\n<CALL:5>K1ABC<EOR>\n<CALL:5>K2XYZ<EOR>\n");file.close();b.reloadActiveLogbookState("switch");QTextStream o(stdout);o<<"loaded|"<<b.m_qsoCountCache<<'|'<<b.m_workedCalls.contains("K1ABC")<<'|'<<b.m_workedCalls.contains("K2XYZ")<<'|'<<b.rebuilt<<'|'<<b.invalid<<'|'<<b.warm<<'|'<<b.notifications<<'|'<<(m.path==b.path)<<'|'<<lastLog.contains("reason=switch")<<'\n';b.path=dir.filePath("missing.adi");b.m_mapIntelligenceService=nullptr;b.reloadActiveLogbookState("missing");o<<"missing|"<<b.m_qsoCountCache<<'|'<<b.m_workedCalls.size()<<'|'<<b.rebuilt<<'|'<<b.notifications<<'\n';}
