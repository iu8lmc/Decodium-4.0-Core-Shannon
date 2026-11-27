#include <QCoreApplication>
#include <QString>
#include <iostream>
static void bridgeLog(const QString&){}
struct Cat {int sends=0,stops=0,on=0,off=0;QString text;int speed=0;bool sendRigMorse(const QString&t,int w){++sends;text=t;speed=w;return true;}void stopRigMorse(){++stops;}void setRigPtt(bool value){value?++on:++off;}};
class DecodiumBridge:public QObject {public:Cat*m_hamlibCat=nullptr;bool available=false,m_startupTxGuard=false,m_decoPortUseRemote=false;QString m_mode="CW";bool cwRigKeyerAvailable()const{return available&&m_hamlibCat;}bool cwSendRigMorse(const QString&,int);void cwStopRigMorse();void cwRigPtt(bool);};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;Cat cat;std::cout<<"missing|"<<b.cwSendRigMorse("CQ",20)<<'\n';b.m_hamlibCat=&cat;b.available=true;std::cout<<"morse|"<<b.cwSendRigMorse("CQ",20)<<"|"<<cat.sends<<"|"<<(cat.text=="CQ")<<"|"<<cat.speed<<'\n';b.cwStopRigMorse();std::cout<<"stop|"<<cat.stops<<'\n';b.m_startupTxGuard=true;b.cwRigPtt(true);b.m_startupTxGuard=false;b.m_mode="FT8";b.cwRigPtt(true);b.m_mode="CW";b.m_decoPortUseRemote=true;b.cwRigPtt(true);std::cout<<"guards|"<<cat.on<<'\n';b.cwRigPtt(false);b.m_decoPortUseRemote=false;b.cwRigPtt(true);std::cout<<"ptt|"<<cat.on<<"|"<<cat.off<<'\n';}
