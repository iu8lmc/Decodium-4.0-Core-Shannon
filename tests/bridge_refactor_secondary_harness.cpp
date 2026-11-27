#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QQueue>
#include <QSet>
#include <QThread>
#include <QTimer>
#include <QVariantMap>
#include <cmath>
#include <utility>
#include <iostream>
#include "NativeDecodeSecondaryWork.h"
static QString decodeMirrorEntryKey(const QVariantMap&e){return e.value("message").toString();}static int secondsFromUtcDisplayToken(QString){return -1;}static void bridgeLog(const QString&){}
struct Active{int count=0;void addStation(QString,int,int,QString,QString){++count;}};struct Sync{int count=0;void reportDecodeDt(double,int){++count;}};
class DecodiumBridge:public QObject{public:
 struct LegacyDecodeSecondaryWork{QVariantMap entry;QString source,key;};using NativeDecodeSecondaryWork=decodium::NativeDecodeSecondaryWork;
 QQueue<LegacyDecodeSecondaryWork>m_legacyDecodeSecondaryQueue;QQueue<NativeDecodeSecondaryWork>m_nativeDecodeSecondaryQueue;QSet<QString>m_legacyDecodeSecondaryPendingKeys,m_nativeDecodeSecondaryPendingKeys;
 bool m_legacyDecodeSecondaryDrainScheduled=false,m_nativeDecodeSecondaryDrainScheduled=false;QString m_mode="FT8";quint64 m_decodeSessionId=3;Active*m_activeStations=nullptr;Sync*m_decoSyncTime=nullptr;QStringList world,remote,psk,persisted,udp,alerts;
 int periodMsForMode(const QString&)const{return 15000;}void queueWorldMapEntryForReplay(const QVariantMap&e,bool,int){world<<e.value("message").toString();}void publishRemoteActivityEntries(const QVariantList&v){remote<<v.first().toMap().value("message").toString();}void maybeQueuePskReporterSpot(const QVariantMap&,const QString&m,bool,const QString&,const QString&,const QString&){psk<<m;}bool persistDecodeHistoryEntry(const QVariantMap&e){persisted<<e.value("message").toString();return true;}void udpSendDecode(bool,const QString&r,quint64){udp<<r;}void maybePlayDecodeAlert(bool,bool,const QString&m){alerts<<m;}
 void queueLegacyDecodeSecondaryWork(const QVariantList&,const QString&);void scheduleLegacyDecodeSecondaryDrain(int=0);void drainLegacyDecodeSecondaryWork();void queueNativeDecodeSecondaryWork(NativeDecodeSecondaryWork);void scheduleNativeDecodeSecondaryDrain(int=0);void drainNativeDecodeSecondaryWork();
};
#include "bridge_refactor_under_test.inc"
static void pump(){for(int i=0;i<40;++i){QCoreApplication::processEvents();QThread::msleep(2);}}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;Active active;Sync sync;b.m_activeStations=&active;b.m_decoSyncTime=&sync;QVariantList legacy;
 for(int i=0;i<12;++i)legacy.append(QVariantMap{{"message",QString("L%1").arg(i)},{"mode","FT8"}});legacy.append(legacy.first());legacy.append(QVariantMap{{"message","WRONG"},{"mode","FT4"}});
 b.queueLegacyDecodeSecondaryWork(legacy,"test");pump();if(!b.m_legacyDecodeSecondaryQueue.isEmpty()||b.world.size()!=12||b.remote.size()!=12||b.psk.size()!=12||b.persisted.size()!=12||b.world.first()!="L0"||b.world.last()!="L11")return 1;
 b.world.clear();b.psk.clear();decodium::NativeDecodeSecondaryWork w;w.entry={{"message","CQ K1ABC"},{"mode","FT8"},{"isCQ",true},{"fromCall","K1ABC"},{"freq","1500"},{"db","-10"},{"dt","0.3"}};w.rawRow="raw";w.serial=42;w.publishPsk=w.updateActiveStation=w.updateWorldMap=w.sendUdp=w.playAlert=w.reportDecodeTiming=true;
 b.queueNativeDecodeSecondaryWork(w);b.queueNativeDecodeSecondaryWork(w);auto bad=w;bad.key="other";bad.entry["mode"]="FT4";b.queueNativeDecodeSecondaryWork(bad);pump();if(!b.m_nativeDecodeSecondaryQueue.isEmpty()||b.world.size()!=1||b.psk.size()!=1||active.count!=1||sync.count!=1||b.udp.size()!=1||b.alerts.size()!=1)return 2;
 std::cout<<"legacy/native queue order, dedup, mode filter, timer drain and side-effect routing: OK\n";
}
