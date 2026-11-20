#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QStringList>
#include <QTextStream>
#include <QTimer>
#include <QVariant>
class Doppler{public:bool active=true;int drops=0;bool applied(){return active;}void disconnected(){++drops;}};class Cat{public:bool online=true;QStringList calls;bool connected(){return online;}void setRigPtt(bool v){calls<<QString("ptt:%1").arg(v);}void setRigFrequency(double v){calls<<QString("freq:%1").arg(v);}void setRigTxFrequency(int v){calls<<QString("tx:%1").arg(v);}};class Worker:public QObject{public:int stops=0;void beginShutdown(){++stops;}};
class DecodiumBridge:public QObject{public:bool m_shuttingDown=false,m_asyncDecodePending=true;Doppler*m_q65Doppler=nullptr;QString m_catBackend="hamlib";Cat*m_hamlibCat=nullptr;double m_frequency=100;QTimer*m_asyncDecodeTimer=nullptr;Worker*m_ft8Worker=nullptr,*m_ft2Worker=nullptr,*m_ft4Worker=nullptr,*m_q65Worker=nullptr,*m_mskWorker=nullptr,*m_wsprWorker=nullptr,*m_legacyJtWorker=nullptr,*m_fst4Worker=nullptr;QObject*m_activeStations=nullptr;double applyFrequencyCalibration(double v){return v+1;}bool shouldIgnoreDecodeCallbacks()const;void beginDecodeCallbackShutdown();};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);DecodiumBridge b;Doppler d;Cat c;Worker w8,w2,w4;QTimer timer;QObject stations;b.m_q65Doppler=&d;b.m_hamlibCat=&c;b.m_ft8Worker=&w8;b.m_ft2Worker=&w2;b.m_ft4Worker=&w4;b.m_asyncDecodeTimer=&timer;b.m_activeStations=&stations;timer.start(10000);o<<"initial|"<<b.shouldIgnoreDecodeCallbacks()<<'\n';app.setProperty("decodiumShuttingDown",true);o<<"appguard|"<<b.shouldIgnoreDecodeCallbacks()<<'\n';app.setProperty("decodiumShuttingDown",false);b.beginDecodeCallbackShutdown();o<<"shutdown|"<<b.shouldIgnoreDecodeCallbacks()<<'|'<<b.m_asyncDecodePending<<'|'<<timer.isActive()<<'|'<<w8.stops<<w2.stops<<w4.stops<<'|'<<c.calls.join(",")<<'|'<<d.drops<<'|'<<(b.m_activeStations==nullptr)<<'\n';b.beginDecodeCallbackShutdown();o<<"repeat|"<<c.calls.size()<<'|'<<d.drops<<'\n';}
