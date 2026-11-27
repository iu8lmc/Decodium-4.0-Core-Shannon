#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <QVariantMap>
#include <cmath>
#include <utility>
#include <iostream>
class DecodeListModel {public:QVariantList rows;int count()const{return rows.size();}QVariantMap entry(int i)const{return rows.at(i).toMap();}void appendEntriesBudgeted(const QVariantList& v,bool prepend,int){if(prepend)rows=v+rows;else rows+=v;}};
class DecodiumBridge:public QObject{public:
 DecodeListModel *m_bandActivityModel=nullptr,*m_fullSpectrumModel=nullptr,*m_rxDecodeModel=nullptr;bool m_decodeUiSnapshotInFlight=false,m_fullSpectrumSnapshotPending=false,m_decodeNewestFirst=false,m_decodeShowPeriodSeparator=true,m_skipNextLegacyBandModelSnapshot=false,m_skipNextLegacyRxModelSnapshot=false,burst=false;int m_decodeUiPendingFlags=0;qint64 m_decodeUiBurstUntilMs=0;QVariantList m_pendingLegacyFullSpectrumDelta;QTimer*m_legacyFullSpectrumDeltaTimer=nullptr;
 QVariantList filterEntriesForBandActivity(const QVariantList&v)const{return v;}QVariantList filterEntriesForRxDecode(const QVariantList&v)const{return v;}int decodeUiRowsPerCycle()const{return 24;}bool decodeUiBurstActive()const{return burst;}
 void invalidateLegacyDecodeModelDeltaFastPath();bool applyLegacyBandModelDelta(const QVariantList&);bool applyLegacyRxModelDelta(const QVariantList&);void queueLegacyFullSpectrumModelDelta(const QVariantList&);void applyLegacyFullSpectrumModelDelta();
};
#include "bridge_refactor_under_test.inc"
static QVariantMap row(QString time,QString text){return {{"time",time},{"message",text}};}
static void pump(int ms){QElapsedTimer t;t.start();while(t.elapsed()<ms){QCoreApplication::processEvents();QThread::msleep(2);}}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;DecodeListModel band,full,rx;b.m_bandActivityModel=&band;b.m_fullSpectrumModel=&full;b.m_rxDecodeModel=&rx;band.rows={row("120000","old")};full.rows=band.rows;rx.rows=band.rows;
 b.m_decodeUiSnapshotInFlight=true;if(b.applyLegacyBandModelDelta({row("120015","new")}))return 1;b.m_decodeUiSnapshotInFlight=false;
 if(!b.applyLegacyBandModelDelta({row("120015","new")})||band.count()!=3||!band.entry(1).value("isSeparator").toBool()||!b.m_skipNextLegacyBandModelSnapshot)return 2;
 if(!b.applyLegacyRxModelDelta({row("120015","rx")})||rx.count()!=3||!rx.entry(1).value("isSeparator").toBool())return 3;
 pump(180);if(full.count()!=3||full.entry(2).value("message")!="new"||!b.m_pendingLegacyFullSpectrumDelta.isEmpty())return 4;
 b.queueLegacyFullSpectrumModelDelta({row("120030","pending")});b.invalidateLegacyDecodeModelDeltaFastPath();if(b.m_legacyFullSpectrumDeltaTimer->isActive()||!b.m_pendingLegacyFullSpectrumDelta.isEmpty()||b.m_skipNextLegacyBandModelSnapshot||b.m_skipNextLegacyRxModelSnapshot)return 5;
 b.m_decodeNewestFirst=true;band.rows={row("120030","old")};full.rows=band.rows;if(!b.applyLegacyBandModelDelta({row("120015","previous")}))return 6;pump(180);if(band.entry(0).value("message")!="previous"||!band.entry(1).value("isSeparator").toBool()||full.entry(0).value("message")!="previous")return 7;
 std::cout<<"legacy model delta ordering, period separators, snapshot guard, timer and invalidation: OK\n";
}
