#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QVector>
#include <QString>
#include <iostream>
struct CwKeyEvent {int deltaMs;bool down;};
namespace decodium::cw {QVector<CwKeyEvent> timelineFor(const QString&,int){return {{20,true},{20,false}};}}
static void bridgeLog(const QString&){}
class DecodiumBridge:public QObject {
public:
 bool enabled=true,m_cwRemoteKeying=false,m_transmitting=false,m_tuning=false,m_cwToneTestPending=false,playResult=true;int plays=0;QString status;
 bool cwMonitorEnabled() const{return enabled;}QString cwMonitorDevice()const{return {};};int cwMonitorVolume()const{return 50;}
 bool playCwMonitor(const QVector<CwKeyEvent>& events,int hz){++plays;return playResult&&events.size()==2&&hz==700;}
 void statusMessage(const QString&s){status=s;}void testCwMonitor();
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;b.enabled=false;b.testCwMonitor();std::cout<<"disabled|"<<b.plays<<"|"<<b.status.contains("skipped")<<'\n';b.enabled=true;b.m_transmitting=true;b.testCwMonitor();std::cout<<"busy|"<<b.plays<<"|"<<b.status.contains("transmitting")<<'\n';b.m_transmitting=false;b.m_cwToneTestPending=true;b.testCwMonitor();std::cout<<"pending|"<<b.plays<<"|"<<b.status.contains("already running")<<'\n';b.m_cwToneTestPending=false;b.playResult=false;b.testCwMonitor();std::cout<<"failure|"<<b.plays<<"|"<<b.m_cwToneTestPending<<'\n';b.playResult=true;b.testCwMonitor();std::cout<<"queued|"<<b.plays<<"|"<<b.m_cwToneTestPending<<"|"<<b.status.contains("queued")<<'\n';QEventLoop loop;QTimer::singleShot(350,&loop,&QEventLoop::quit);loop.exec();std::cout<<"released|"<<b.m_cwToneTestPending<<'\n';}
