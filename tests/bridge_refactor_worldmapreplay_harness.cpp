#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QSet>
#include <QTextStream>
#include <QTimer>
#include <QVariantMap>
#include <utility>
static void bridgeLog(const QString&){}class MainThreadTraceScope{public:MainThreadTraceScope(QString,QString){}};
class DecodiumBridge:public QObject{public:QVariantList m_decodeList,m_rxDecodeList,m_worldMapReplayEntries;bool m_transmitting=false,m_tuning=false,m_worldMapReplayActive=false,m_worldMapFullReplayDeferred=false,m_worldMapDeferredLogActive=false,enabled=true,ready=true,deferred=false;quint64 m_worldMapReplayGeneration=0;int m_worldMapReplayIndex=0,resets=0,paths=0,scheduled=0;QStringList replayed;bool worldMapFeedEnabled()const{return enabled;}bool worldMapConsumerReady()const{return ready;}bool visualFeedsDeferredForTx()const{return deferred;}void scheduleDeferredWorldMapFeedFlush(int=600){++scheduled;}void worldMapResetRequested(){++resets;}QString worldMapFeedEntryKey(const QVariantMap&e){return e.value("key").toString();}void replayWorldMapEntry(const QVariantMap&e,bool,int age){replayed<<e.value("key").toString()+QString::number(age);}void emitCurrentWorldMapQsoPath(){++paths;}void replayWorldMapFeed();void flushWorldMapReplayChunk(quint64);};
#include "bridge_refactor_under_test.inc"
static void wait(int ms){QEventLoop e;QTimer::singleShot(ms,&e,&QEventLoop::quit);e.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);DecodiumBridge b;b.m_decodeList={QVariantMap{{"key","A"}},QVariantMap{{"key","B"}}};b.m_rxDecodeList={QVariantMap{{"key","B"}},QVariantMap{{"key","C"}}};b.ready=false;b.replayWorldMapFeed();o<<"notready|"<<b.m_worldMapFullReplayDeferred<<'|'<<b.resets<<'\n';b.ready=true;b.deferred=true;b.replayWorldMapFeed();o<<"deferred|"<<b.scheduled<<'|'<<b.m_worldMapFullReplayDeferred<<'\n';b.deferred=false;b.replayWorldMapFeed();auto gen=b.m_worldMapReplayGeneration;b.flushWorldMapReplayChunk(gen-1);o<<"queued|"<<b.m_worldMapReplayEntries.size()<<'|'<<b.m_worldMapReplayIndex<<'|'<<b.resets<<'\n';wait(30);o<<"complete|"<<b.replayed.join(",")<<'|'<<b.paths<<'|'<<b.m_worldMapReplayActive<<'\n';b.replayWorldMapFeed();b.deferred=true;wait(30);o<<"interrupted|"<<b.m_worldMapReplayActive<<'|'<<b.m_worldMapFullReplayDeferred<<'|'<<b.scheduled<<'\n';}
