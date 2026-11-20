#include <QCoreApplication>
#include <QVector>
#include <QList>
#include <QByteArray>
#include <QString>
#include <iostream>
struct CwKeyEvent{int deltaMs;bool down;};
namespace decodium::cw {struct CwSidetone {QByteArray device;double volume=-1;QList<int>deltas;QList<bool>downs;int frequency=0;bool result=true;explicit CwSidetone(QObject*){}void failed(const QString&){}void setDeviceId(QByteArray v){device=v;}void setVolume(double v){volume=v;}bool enqueue(QList<int>d,QList<bool>k,int hz){deltas=d;downs=k;frequency=hz;return result;}};}
class DecodiumBridge:public QObject {public:bool enabled=false;int connections=0;decodium::cw::CwSidetone*m_cwSidetone=nullptr;bool cwMonitorEnabled()const{return enabled;}QString cwMonitorDevice()const{return QString::fromLatin1(QByteArray("output").toBase64());}int cwMonitorVolume()const{return 42;}void errorMessage(const QString&){}template<class A,class B,class C,class D>void connect(A,B,C,D){++connections;}bool playCwMonitor(const QVector<CwKeyEvent>&,int);};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;QVector<CwKeyEvent> events{{30,true},{90,false}};bool x=b.playCwMonitor(events,720);std::cout<<"disabled|"<<x<<"|"<<(b.m_cwSidetone==nullptr)<<'\n';b.enabled=true;x=b.playCwMonitor(events,720);auto* t=b.m_cwSidetone;std::cout<<"enabled|"<<x<<"|"<<b.connections<<"|"<<(t->device=="output")<<"|"<<(t->volume==0.42)<<"|"<<(t->deltas==QList<int>{30,90})<<"|"<<(t->downs==QList<bool>{true,false})<<"|"<<t->frequency<<'\n';t->result=false;x=b.playCwMonitor(events,600);std::cout<<"failed|"<<x<<"|"<<b.connections<<"|"<<t->frequency<<'\n';delete t;}
