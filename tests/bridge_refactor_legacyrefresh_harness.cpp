#include <QCoreApplication>
#include <QEventLoop>
#include <QObject>
#include <QString>
#include <QTextStream>
#include <QTimer>
static bool fallback=false;static bool useLegacyRigControlFallback(void*,const QString&){return fallback;}static void bridgeLog(const QString&){}
class DecodiumBridge:public QObject{public:void*m_legacyBackend=nullptr;QString m_catBackend;bool available=true,rx=true,m_syncingLegacyBackendState=false,m_syncingLegacyBackendDecodeList=false,m_legacyStateRefreshBurstQueued=false;QTimer*m_legacyStateTimer=nullptr;int states=0,dialogs=0;bool legacyBackendAvailable()const{return available;}bool usingLegacyBackendForRx()const{return rx;}void syncLegacyBackendDialogState(){++dialogs;}void syncLegacyBackendState(){++states;}void syncLegacyBackendTxState();void scheduleLegacyStateRefreshBurst();};
#include "bridge_refactor_under_test.inc"
static void wait(int ms){QEventLoop e;QTimer::singleShot(ms,&e,&QEventLoop::quit);e.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);DecodiumBridge b;b.rx=false;b.syncLegacyBackendTxState();b.scheduleLegacyStateRefreshBurst();o<<"blocked|"<<b.dialogs<<'|'<<b.states<<'\n';fallback=true;b.syncLegacyBackendTxState();o<<"fallback|"<<b.dialogs<<'\n';fallback=false;b.rx=true;b.available=false;b.scheduleLegacyStateRefreshBurst();o<<"unavailable|"<<b.states<<'\n';b.available=true;QTimer stateTimer;stateTimer.setInterval(5000);b.m_legacyStateTimer=&stateTimer;b.scheduleLegacyStateRefreshBurst();o<<"started|"<<b.states<<'|'<<stateTimer.isActive()<<'\n';wait(420);o<<"burst|"<<b.states<<'\n';b.m_syncingLegacyBackendState=true;b.scheduleLegacyStateRefreshBurst();b.scheduleLegacyStateRefreshBurst();o<<"queued|"<<b.m_legacyStateRefreshBurstQueued<<'|'<<b.states<<'\n';b.m_syncingLegacyBackendState=false;wait(420);o<<"resumed|"<<b.m_legacyStateRefreshBurstQueued<<'|'<<b.states<<'\n';}
