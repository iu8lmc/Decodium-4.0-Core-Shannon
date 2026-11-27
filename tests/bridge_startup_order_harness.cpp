#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QObject>
#include <iostream>
static void bridgeLog(const QString&){}
class DecodiumBridge : public QObject {
 Q_OBJECT
public:
 bool m_useLegacyTxBackend=true,m_shuttingDown=false,m_mainQmlReady=false,m_startupServicesStarted=false,ok=true;int m_mode=0,starts=0,fallback=0;
 bool ensureLegacyBackendAvailable(){++starts;return ok;}
 void ensureDecodeWorkerForMode(int){++fallback;}
 void schedule();
signals:
 void mainQmlReadyForNativeWindowing();
};
void DecodiumBridge::schedule(){
#include "bridge_startup_under_test.inc"
}
static void wait(int ms){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge slow;slow.schedule();wait(70);std::cout<<"before-ready|"<<slow.starts<<'\n';slow.m_mainQmlReady=true;emit slow.mainQmlReadyForNativeWindowing();wait(40);std::cout<<"after-ready|"<<slow.starts<<'\n';DecodiumBridge normal;normal.m_mainQmlReady=true;normal.schedule();wait(70);std::cout<<"ready|"<<normal.starts<<'\n';DecodiumBridge failed;failed.ok=false;failed.m_mainQmlReady=true;failed.schedule();wait(70);std::cout<<"fallback|"<<failed.starts<<"|"<<failed.m_useLegacyTxBackend<<"|"<<failed.fallback<<'\n';DecodiumBridge stop;stop.m_shuttingDown=true;stop.schedule();wait(70);std::cout<<"shutdown|"<<stop.starts<<'\n';}
#include "moc_test.cpp"
