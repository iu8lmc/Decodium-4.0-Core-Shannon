#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QHash>
#include <QObject>
#include <QTextStream>
#include <QThread>
#include <QTimer>
constexpr int kDecodeUiRefreshBand=1,kDecodeUiRefreshRx=2,kDecodeUiRefreshFullSpectrum=4;
class DecodiumBridge:public QObject {public:int m_decodeUiPendingFlags=0;quint64 m_decodeUiSnapshotGeneration=0;qint64 m_decodeUiBurstUntilMs=0;QTimer*m_decodeUiRefreshTimer=nullptr;bool m_decodeUiSnapshotInFlight=false,stalled=false;QHash<QString,bool>m_decodeUiGhostCache,m_decodeUiRxMembershipCache;QString quality="High";int fired=0;QString uiQuality()const{return quality;}bool isUiStallActive(int,int)const{return stalled;}void startDecodeUiSnapshotRefresh(){++fired;}
 void rebuildBandActivityModel();void rebuildRxDecodeModel();bool decodeUiBurstActive()const;int decodeUiRowsPerCycle()const;void invalidateDecodeUiPredicateCaches();void scheduleDecodeUiRefresh(int,int=60);};
#include "bridge_refactor_under_test.inc"
static void wait(int ms){QEventLoop e;QTimer::singleShot(ms,&e,&QEventLoop::quit);e.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);DecodiumBridge b;b.scheduleDecodeUiRefresh(0,0);o<<"zero|"<<b.m_decodeUiSnapshotGeneration<<'|'<<(b.m_decodeUiRefreshTimer==nullptr)<<'\n';b.rebuildBandActivityModel();b.rebuildRxDecodeModel();o<<"queued|"<<b.m_decodeUiPendingFlags<<'|'<<b.m_decodeUiSnapshotGeneration<<'|'<<b.decodeUiBurstActive()<<'|'<<b.m_decodeUiRefreshTimer->isActive()<<'\n';wait(100);o<<"coalesced|"<<b.fired<<'\n';b.m_decodeUiSnapshotInFlight=true;b.scheduleDecodeUiRefresh(2,0);wait(20);o<<"inflight|"<<b.fired<<'|'<<b.m_decodeUiPendingFlags<<'\n';b.m_decodeUiGhostCache["a"]=true;b.m_decodeUiRxMembershipCache["b"]=true;b.invalidateDecodeUiPredicateCaches();o<<"cleared|"<<b.m_decodeUiGhostCache.size()<<'|'<<b.m_decodeUiRxMembershipCache.size()<<'\n';b.quality="Low";o<<"low|"<<(b.decodeUiRowsPerCycle()<=20)<<'\n';b.stalled=true;o<<"stall|"<<(b.decodeUiRowsPerCycle()<=16)<<'\n';}
