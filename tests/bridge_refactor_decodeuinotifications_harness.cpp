#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QObject>
#include <QScopeGuard>
#include <QTextStream>
#include <QTimer>
#include <QVariantList>
class MainThreadTraceScope{public:MainThreadTraceScope(QString,QString,int=0){}};
class DecodiumBridge:public QObject{public:qint64 m_decodeUiBurstUntilMs=0;QVariantList m_decodeList,m_rxDecodeList;QTimer*m_decodeListEmitTimer=nullptr,*m_rxDecodeListEmitTimer=nullptr;bool m_decodeListEmitPending=false,m_rxDecodeListEmitPending=false,m_rxDecodeListNormalizePending=false,requeue=false;int band=0,rx=0;static int starts,ends;static void noteDecodeModelEmitStart(){++starts;}static void noteDecodeModelEmitEnd(){++ends;}void decodeListChanged(){++band;if(requeue){requeue=false;emitDecodeListChangedThrottled();}}void rxDecodeListChanged(){++rx;}void emitDecodeListChangedThrottled();void emitRxDecodeListChangedThrottled(bool);};
int DecodiumBridge::starts=0;int DecodiumBridge::ends=0;
#include "bridge_refactor_under_test.inc"
static void wait(int ms){QEventLoop e;QTimer::singleShot(ms,&e,&QEventLoop::quit);e.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);DecodiumBridge b;b.emitDecodeListChangedThrottled();b.emitDecodeListChangedThrottled();b.emitRxDecodeListChangedThrottled(false);b.emitRxDecodeListChangedThrottled(true);o<<"queued|"<<b.m_decodeListEmitTimer->interval()<<'|'<<b.m_rxDecodeListEmitTimer->interval()<<'|'<<b.m_rxDecodeListNormalizePending<<'|'<<(b.m_decodeUiBurstUntilMs>QDateTime::currentMSecsSinceEpoch())<<'\n';wait(350);o<<"rx|"<<b.band<<'|'<<b.rx<<'|'<<b.m_rxDecodeListNormalizePending<<'\n';wait(250);o<<"band|"<<b.band<<'|'<<b.rx<<'|'<<b.starts<<'|'<<b.ends<<'\n';b.requeue=true;b.emitDecodeListChangedThrottled();wait(1100);o<<"requeue|"<<b.band<<'|'<<b.starts<<'|'<<b.ends<<'\n';}
