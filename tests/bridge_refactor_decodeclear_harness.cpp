#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariantMap>
#include <utility>
#include <iostream>
static void bridgeLog(const QString&){}
static QString decodeMirrorEntryKey(const QVariantMap& e){return e.value("message").toString();}
static QString decodeRxClearEntryKey(const QVariantMap& e){return e.value("message").toString();}
struct Legacy {int band=0,rx=0;void clearBandActivity(){++band;}void clearRxFrequency(){++rx;}};
struct Active {int n=0;void clear(){++n;}};
struct Model {QVariantList data;void setEntries(const QVariantList&v){data=v;}};
class DecodiumBridge:public QObject {public:
 bool legacy=false,m_ft2LinkTxActive=false;Legacy* m_legacyBackend=nullptr;Active* m_activeStations=nullptr;Model* m_rxDecodeModel=nullptr;QTimer* m_decodeReleaseTimer=nullptr;QString path,m_legacyAllTxtConsumedPath,m_legacyAllTxtRevisionKey="old";qint64 m_legacyAllTxtConsumedSize=0;int m_legacyBandActivityRevision=0,m_legacyRxFrequencyRevision=0,decodeSignals=0,rxSignals=0,resets=0,world=0,remote=0;
 QSet<QString> m_legacyPrunedBandMirrorKeys,m_clearedRxDecodeKeys,m_nativeDecodeSecondaryPendingKeys,m_legacyClearedRxMirrorKeys,m_rxDecodeMirrorKeys;
 QVariantList m_decodeList,m_rxDecodeList,m_pendingDecodeReleaseQueue,m_nativeDecodeSecondaryQueue;
 void resetWorldMapDisplayFromCurrentDecodes(){++world;} bool usingLegacyBackendForTx()const{return legacy;}QString legacyAllTxtPath()const{return path;}void resetNativeDecodeDedupIndex(){++resets;}void clearRemoteActivityCache(bool){++remote;}void decodeListChanged(){++decodeSignals;}void rxDecodeListChanged(){++rxSignals;}bool entryBelongsToCurrentQso(const QVariantMap& e)const{return e.value("current").toBool();}
 void clearDecodeList();void clearRxDecodes();
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir tmp;if(!tmp.isValid())return 1;QFile f(tmp.path()+"/ALL.TXT");if(!f.open(QIODevice::WriteOnly)||f.write("abc",3)!=3)return 2;f.close();
 DecodiumBridge b;Legacy legacy;Active active;Model model;QTimer timer;timer.start(60000);b.legacy=true;b.path=f.fileName();b.m_legacyBackend=&legacy;b.m_activeStations=&active;b.m_rxDecodeModel=&model;b.m_decodeReleaseTimer=&timer;
 QVariantMap current{{"message","current"},{"current",true}},other{{"message","other"},{"current",false}};b.m_rxDecodeList={current};b.m_decodeList={other};b.clearRxDecodes();
 if(legacy.rx!=1||b.m_legacyRxFrequencyRevision!=-1||b.m_legacyAllTxtConsumedSize!=3||!b.m_clearedRxDecodeKeys.contains("current")||!b.m_legacyClearedRxMirrorKeys.contains("current")||!b.m_rxDecodeList.isEmpty()||b.rxSignals!=1)return 3;
 b.m_pendingDecodeReleaseQueue={1};b.m_nativeDecodeSecondaryQueue={1};b.clearDecodeList();if(legacy.band!=1||!b.m_decodeList.isEmpty()||!b.m_pendingDecodeReleaseQueue.isEmpty()||!b.m_nativeDecodeSecondaryQueue.isEmpty()||timer.isActive()||active.n!=1||b.decodeSignals!=1||b.resets!=1||b.remote!=1)return 4;
 b.m_ft2LinkTxActive=true;b.clearDecodeList();if(legacy.band!=1)return 5;
 b.legacy=false;b.m_rxDecodeList={other};b.clearRxDecodes();if(legacy.rx!=1||b.m_clearedRxDecodeKeys.contains("other"))return 6;
 std::cout<<"legacy cursor and delegated clears; current-QSO keys; timer, queues and model reset: OK\n";
}
