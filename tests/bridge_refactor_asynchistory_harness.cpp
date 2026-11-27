#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPointer>
#include <QRunnable>
#include <QSet>
#include <QTemporaryDir>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <memory>
#include <iostream>
struct ParsedAdifRecord {QString call;};struct ParsedAdifDocument {QList<ParsedAdifRecord> records;};
static ParsedAdifDocument loadAdifDocument(const QString& path){QFile f(path);ParsedAdifDocument d;if(f.open(QIODevice::ReadOnly))for(auto line:f.readAll().split('\n'))if(!line.trimmed().isEmpty())d.records.append({QString::fromUtf8(line.trimmed())});return d;}
static void rebuildWorkedCallsFromDocument(QSet<QString>& out,const QList<ParsedAdifRecord>& records){out.clear();for(auto r:records)out.insert(r.call);}
class DxccLookup {bool loaded=false;public:bool loadCtyDat(const QString& path){QFile f(path);loaded=f.open(QIODevice::ReadOnly)&&f.readAll()=="valid";return loaded;}bool isLoaded()const{return loaded;}int entityCount()const{return loaded?1:0;}};
struct Intelligence {int notifications=0;void notifyDxccDataChanged(){++notifications;}};
static void bridgeLog(const QString&){}
class DecodiumBridge:public QObject {public:
 bool m_shuttingDown=false,m_dxccLookupLoading=false,m_dxccLookupLoadAttempted=false,m_workedHistoryLoading=false,m_workedHistoryLoaded=false;
 quint64 m_dxccLookupLoadSerial=0;DxccLookup* m_dxccLookup=nullptr;Intelligence* m_callsignIntelligence=nullptr;QStringList candidates;QString path;QSet<QString>m_workedCalls;int m_qsoCountCache=0,refresh=0,countSignals=0,workedSignals=0;
 struct {QSet<QString> dxccEver,cqZoneEver,gridEver;} m_worked;
 QStringList ctyDatSearchPaths()const{return candidates;}QString ensureAdifLogPath()const{return path;}QString effectiveAdifLogPath()const{return path;}
 void refreshDecodeListDxcc(){++refresh;}void localDataStateChanged(){}void rebuildWorkedSetsFromAdifRecords(const QList<ParsedAdifRecord>&r){for(auto x:r)m_worked.dxccEver.insert(x.call);}void invalidateQsoSearchCache(){}void qsoCountChanged(){++countSignals;}void workedCountChanged(){++workedSignals;}
 void loadDxccLookupAsync();void loadWorkedBeforeHistoryAsync();
};
#include "bridge_refactor_under_test.inc"
static void write(const QString&path,const QByteArray&data){QFile f(path);if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate))std::abort();f.write(data);}
static void pump(int ms=500){QElapsedTimer t;t.start();while(t.elapsed()<ms){QThreadPool::globalInstance()->waitForDone();QCoreApplication::processEvents();QThread::msleep(5);}}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir t;if(!t.isValid())return 1;QString c=t.path()+"/cty",a=t.path()+"/a.adi",b=t.path()+"/b.adi";write(c,"valid");write(a,"K1ABC\n9H1SR\n");write(b,"G1ABC\n");
 DxccLookup lookup;Intelligence intel;DecodiumBridge bridge;bridge.m_dxccLookup=&lookup;bridge.m_callsignIntelligence=&intel;bridge.candidates={c};bridge.path=a;bridge.loadWorkedBeforeHistoryAsync();pump();
 if(!lookup.isLoaded()||intel.notifications!=1||!bridge.m_workedHistoryLoaded||bridge.m_qsoCountCache!=2||bridge.m_workedCalls.size()!=2||bridge.countSignals!=1)return 2;
 DxccLookup lookup2;DecodiumBridge changed;changed.m_dxccLookup=&lookup2;changed.m_dxccLookupLoadAttempted=true;changed.path=a;changed.loadWorkedBeforeHistoryAsync();QThreadPool::globalInstance()->waitForDone();changed.path=b;pump();if(!changed.m_workedHistoryLoaded||changed.m_qsoCountCache!=1||!changed.m_workedCalls.contains("G1ABC"))return 3;
 DxccLookup lookup3;DecodiumBridge modified;modified.m_dxccLookup=&lookup3;modified.m_dxccLookupLoadAttempted=true;modified.path=b;modified.loadWorkedBeforeHistoryAsync();QThreadPool::globalInstance()->waitForDone();write(b,"G1ABC\nG2ABC\n");pump();if(!modified.m_workedHistoryLoaded||modified.m_qsoCountCache!=2)return 4;
 DxccLookup lookup4;DecodiumBridge stale;stale.m_dxccLookup=&lookup4;stale.candidates={c};stale.path=a;stale.loadDxccLookupAsync();QThreadPool::globalInstance()->waitForDone();++stale.m_dxccLookupLoadSerial;pump(50);if(lookup4.isLoaded())return 5;
 std::cout<<"async DXCC then history; profile/file-change retry; stale result rejection: OK\n";
}
