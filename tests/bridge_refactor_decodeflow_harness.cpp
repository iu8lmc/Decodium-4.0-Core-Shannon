#include <QCoreApplication>
#include <QDateTime>
#include <QSettings>
#include <QSet>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariantMap>
#include <utility>
#include <iostream>
static void bridgeLog(const QString&){}static QString decodeMirrorEntryKey(const QVariantMap&e){return e.value("message").toString();}static QString decodeRxClearEntryKey(const QVariantMap&e){return e.value("message").toString();}static void normalizeDecodeEntriesForDisplay(QVariantList&,int,const QString&){}static void coalesceRxPaneTxRows(QVariantList&,const QString&){}
struct MainThreadTraceScope{MainThreadTraceScope(const QString&,const QString&,qint64){}};
class DecodiumBridge:public QObject{public:
 static constexpr int kDecodeListCap=192,kDecodeListRetainRows=128,kRxDecodeListCap=192,kRxDecodeListRetainRxRows=128,kRxDecodeListRetainTxRows=32;
 bool m_smoothDecodeFlow=true,m_decodeShowPeriodSeparator=false;int m_pendingDecodeReleaseChunkSize=2,m_decodeReleaseChunkSize=2,committed=0,persisted=0,notifications=0,modelRebuilds=0;QString m_mode="FT8";QVariantList m_decodeList,m_rxDecodeList;QVector<QVariantMap>m_pendingDecodeReleaseQueue;QSet<QString>m_legacyPrunedBandMirrorKeys,m_clearedRxDecodeKeys,m_legacyClearedRxMirrorKeys;QTimer*m_decodeReleaseTimer=nullptr;QObject*m_rxDecodeModel=nullptr;
 void smoothDecodeFlowChanged(){++notifications;}void rememberDecodeDedupEntry(const QVariantMap&){}void noteDecodeCommitted(){++committed;}bool persistDecodeHistoryEntry(const QVariantMap&){++persisted;return true;}void emitDecodeListChangedThrottled(){++notifications;}void emitRxDecodeListChangedThrottled(bool){++notifications;}void rebuildRxDecodeMirrorIndex(){}void rebuildRxDecodeModel(){++modelRebuilds;}void rebuildBandActivityModel(){++modelRebuilds;}void saveSettingsAsync(){}void invalidateLegacyDecodeModelDeltaFastPath(){}
 void setSmoothDecodeFlow(bool);void trimDecodeListsIfNeeded();void drainDecodeReleaseQueue();void setDecodeShowPeriodSeparator(bool);
};
#include "bridge_refactor_under_test.inc"
static QVariantMap row(int n,bool tx=false){return {{"message",QString::number(n)},{"isTx",tx}};}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir t;if(!t.isValid())return 1;QCoreApplication::setOrganizationName("FlowTest");QCoreApplication::setApplicationName("FlowTest");QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,t.path());DecodiumBridge b;QTimer timer;timer.start(60000);b.m_decodeReleaseTimer=&timer;
 for(int i=0;i<192;++i)b.m_decodeList.append(row(i));for(int i=0;i<192;++i)b.m_rxDecodeList.append(row(i));for(int i=0;i<40;++i)b.m_rxDecodeList.append(row(1000+i,true));QObject model;b.m_rxDecodeModel=&model;b.trimDecodeListsIfNeeded();if(b.m_decodeList.size()!=128||b.m_decodeList.first().toMap().value("message")!="64"||b.m_legacyPrunedBandMirrorKeys.size()!=64||b.m_rxDecodeList.size()!=160||b.m_clearedRxDecodeKeys.size()!=64||b.modelRebuilds!=1)return 2;
 b.m_decodeList.clear();b.m_pendingDecodeReleaseQueue={row(1),row(2),row(3)};b.drainDecodeReleaseQueue();if(b.m_decodeList.size()!=2||b.m_pendingDecodeReleaseQueue.size()!=1||b.persisted!=2||!timer.isActive())return 3;b.drainDecodeReleaseQueue();if(b.m_decodeList.size()!=3||!b.m_pendingDecodeReleaseQueue.isEmpty()||timer.isActive())return 4;
 timer.start(60000);b.m_pendingDecodeReleaseQueue={row(4),row(5)};b.setSmoothDecodeFlow(false);if(b.m_decodeList.size()!=5||b.persisted!=5||timer.isActive())return 5;b.setDecodeShowPeriodSeparator(true);if(!b.m_decodeShowPeriodSeparator||b.modelRebuilds!=3)return 6;
 std::cout<<"decode caps and retained RX/TX rows; chunk drain, persistence and smooth-flow flush: OK\n";
}
