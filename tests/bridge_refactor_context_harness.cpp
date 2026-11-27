#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>
#include <QTimer>
#include <QVariantMap>
#include <cmath>
#include <iostream>
static void bridgeLog(const QString&){} static QString decodeMirrorEntryKey(const QVariantMap&e){return e.value("message").toString();}
static QString freqHzToBandToken(quint64 hz){return hz<10000000?"40m":"20m";}
struct Legacy{int band=0,rx=0;QStringList bandActivityLines(){return {"legacy-band"};}QStringList rxFrequencyLines(){return {"legacy-rx"};}void clearBandActivity(){++band;}void clearRxFrequency(){++rx;}};
struct Model{int resets=0;void resetForContextChange(){++resets;}};struct Active{int clears=0;void clear(){++clears;}};
class DecodiumBridge:public QObject{public:
 QString path,m_legacyAllTxtConsumedPath,m_legacyAllTxtRevisionKey; qint64 m_legacyAllTxtConsumedSize=0; int m_legacyBandActivityRevision=0,m_legacyRxFrequencyRevision=0,m_decodeUiPendingFlags=0,notifications=0;quint64 m_decodeSessionId=0,m_decodeUiSnapshotGeneration=0;bool legacy=false,m_fullSpectrumSnapshotPending=true;
 QSet<QString>m_pendingFullSpectrumModelKeys,m_legacyModeChangeClearedDecodeKeys,m_legacyPrunedBandMirrorKeys,m_legacyClearedRxMirrorKeys,m_clearedRxDecodeKeys,m_rxDecodeMirrorKeys,m_nativeDecodeSecondaryPendingKeys;
 QVariantList m_pendingFullSpectrumModelEntries,m_pendingDecodeReleaseQueue,m_decodeList,m_rxDecodeList,m_nativeDecodeSecondaryQueue;
 QTimer *m_decodeUiRefreshTimer=nullptr,*m_fullSpectrumRefreshTimer=nullptr,*m_decodeReleaseTimer=nullptr;Legacy*m_legacyBackend=nullptr;Model*m_bandActivityModel=nullptr,*m_rxDecodeModel=nullptr,*m_fullSpectrumModel=nullptr;Active*m_activeStations=nullptr;
 QString legacyAllTxtPath()const{return path;}void invalidateLegacyDecodeModelDeltaFastPath(){}void invalidateDecodeUiPredicateCaches(){}void resetWorldMapDisplayFromCurrentDecodes(){}bool usingLegacyBackendForTx()const{return legacy;}bool legacyBackendAvailable()const{return legacy;}QVariantList mirrorLegacyDecodeLines(QStringList const& lines,bool,QVariantList const&){QVariantList out;for(auto x:lines)out.append(QVariantMap{{"message",x}});return out;}void resetNativeDecodeDedupIndex(){}void clearRemoteActivityCache(bool){}void decodeListChanged(){++notifications;}void rxDecodeListChanged(){++notifications;}
 void primeLegacyAllTxtCursor();void clearDecodeWindowsForModeChange(const QString&,const QString&);void clearDecodeWindowsForBandChange(double,double,const QString&);
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir t;if(!t.isValid())return 1;QFile f(t.path()+"/ALL.TXT");if(!f.open(QIODevice::WriteOnly)||f.write("abc",3)!=3)return 2;f.close();
 DecodiumBridge b;Legacy legacy;Model band,rx,full;Active active;QTimer timer;timer.start(60000);b.path=f.fileName();b.legacy=true;b.m_legacyBackend=&legacy;b.m_bandActivityModel=&band;b.m_rxDecodeModel=&rx;b.m_fullSpectrumModel=&full;b.m_activeStations=&active;b.m_decodeReleaseTimer=&timer;b.m_decodeList={QVariantMap{{"message","old-band"}}};b.m_rxDecodeList={QVariantMap{{"message","old-rx"}}};b.clearDecodeWindowsForModeChange("FT8","FT4");
 if(b.m_decodeSessionId!=1||b.m_decodeUiSnapshotGeneration!=1||b.m_legacyAllTxtConsumedSize!=3||legacy.band!=1||legacy.rx!=1||!b.m_legacyClearedRxMirrorKeys.contains("old-band")||!b.m_legacyClearedRxMirrorKeys.contains("legacy-rx")||!b.m_decodeList.isEmpty()||timer.isActive()||band.resets!=1||rx.resets!=1||full.resets!=1||active.clears!=1||b.notifications!=2)return 3;
 b.clearDecodeWindowsForBandChange(14074000,14075000,"small");if(b.m_decodeSessionId!=1)return 4;
 b.m_decodeList={QVariantMap{{"message","old2"}}};b.clearDecodeWindowsForBandChange(14074000,7074000,"band");if(b.m_decodeSessionId!=2||legacy.band!=2||!b.m_decodeList.isEmpty()||!b.m_legacyClearedRxMirrorKeys.contains("old2"))return 5;
 b.path=t.path()+"/missing";b.primeLegacyAllTxtCursor();if(b.m_legacyAllTxtConsumedSize!=-1||b.m_legacyAllTxtRevisionKey!=b.path)return 6;b.path.clear();b.primeLegacyAllTxtCursor();if(!b.m_legacyAllTxtConsumedPath.isEmpty()||!b.m_legacyAllTxtRevisionKey.isEmpty())return 7;
 b.legacy=false;b.m_decodeList={QVariantMap{{"message","native"}}};b.clearDecodeWindowsForModeChange("FT4","FT8");if(!b.m_decodeList.isEmpty()||!b.m_legacyClearedRxMirrorKeys.isEmpty()||legacy.band!=2)return 8;
 std::cout<<"mode and band reset; small QSY; legacy mirror and ALL.TXT cursor; native reset: OK\n";
}
