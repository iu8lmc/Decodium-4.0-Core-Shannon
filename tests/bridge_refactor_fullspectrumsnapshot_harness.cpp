#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QObject>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <QVariantList>
#include <utility>
class DecodeListModel{public:struct PreparedSnapshot{QVariantList entries;QStringList keys;};int count=0,budget=0;PreparedSnapshot target;void setEntriesBudgeted(PreparedSnapshot p,int b){++count;budget=b;target=std::move(p);}};
class DecodiumBridge:public QObject {public:bool m_fullSpectrumSnapshotPending=false;QTimer*m_fullSpectrumRefreshTimer=nullptr;qint64 m_decodeUiBurstUntilMs=0;DecodeListModel*m_fullSpectrumModel=nullptr;QVariantList m_pendingFullSpectrumModelEntries;QStringList m_pendingFullSpectrumModelKeys;bool burst=false;bool decodeUiBurstActive()const{return burst;}int decodeUiRowsPerCycle()const{return 30;}void scheduleFullSpectrumModelRefresh();void applyPendingFullSpectrumSnapshot();};
#include "bridge_refactor_under_test.inc"
static void wait(int ms){QEventLoop e;QTimer::singleShot(ms,&e,&QEventLoop::quit);e.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);DecodiumBridge b;DecodeListModel model;b.m_fullSpectrumModel=&model;b.scheduleFullSpectrumModelRefresh();o<<"empty|"<<(b.m_fullSpectrumRefreshTimer==nullptr)<<'\n';b.m_fullSpectrumSnapshotPending=true;b.m_pendingFullSpectrumModelEntries={"rowA","rowB"};b.m_pendingFullSpectrumModelKeys={"A","B"};b.burst=true;b.scheduleFullSpectrumModelRefresh();o<<"scheduled|"<<b.m_fullSpectrumRefreshTimer->isActive()<<'|'<<(b.m_fullSpectrumRefreshTimer->interval()>=80)<<'\n';b.applyPendingFullSpectrumSnapshot();o<<"defer|"<<model.count<<'|'<<b.m_fullSpectrumSnapshotPending<<'\n';b.burst=false;wait(120);o<<"applied|"<<model.count<<'|'<<model.budget<<'|'<<model.target.entries.size()<<'|'<<model.target.keys.join(",")<<'|'<<b.m_fullSpectrumSnapshotPending<<'|'<<b.m_pendingFullSpectrumModelEntries.size()<<'\n';b.m_fullSpectrumSnapshotPending=true;b.m_fullSpectrumModel=nullptr;b.applyPendingFullSpectrumSnapshot();o<<"missing|"<<b.m_fullSpectrumSnapshotPending<<'|'<<model.count<<'\n';}
