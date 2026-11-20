#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QString>
#include <iostream>
struct QAudioDevice{bool null=true;bool isNull()const{return null;}};
struct RemoteRadioLink{bool linked=true,tx=true;QString reason;bool isLinked()const{return linked;}bool canTransmit()const{return tx;}QString txBlockedReason()const{return reason;}};
class DecodiumBridge:public QObject {public:bool m_ssbTxActive=false,m_transmitting=false,m_tuning=false,m_rttyTxActive=false,m_decoPortUseRemote=false,sstv=false,keyer=false,remoteKey=true,found=false;QString m_mode="CW";RemoteRadioLink*link=nullptr;bool sstvTxActive()const{return sstv;}bool cwRigKeyerAvailable()const{return keyer;}bool cwRemoteKeySupported()const{return remoteKey;}RemoteRadioLink*remoteLinkIfAny(){return link;}QAudioDevice resolveTxOutputDevice(bool*f){*f=found;return {!found};}bool cwCanTransmit();};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir tmp;QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,tmp.path());DecodiumBridge b;std::cout<<"local-cw|"<<b.cwCanTransmit();b.keyer=true;std::cout<<"|"<<b.cwCanTransmit()<<'\n';b.m_transmitting=true;std::cout<<"busy|"<<b.cwCanTransmit()<<'\n';b.m_transmitting=false;b.m_mode="FT8";std::cout<<"audio|"<<b.cwCanTransmit();b.found=true;std::cout<<"|"<<b.cwCanTransmit()<<'\n';b.m_mode="CW";b.m_decoPortUseRemote=true;RemoteRadioLink r;b.link=&r;std::cout<<"remote|"<<b.cwCanTransmit();r.reason="busy";std::cout<<"|"<<b.cwCanTransmit();r.reason.clear();r.linked=false;std::cout<<"|"<<b.cwCanTransmit()<<'\n';r.linked=true;QSettings settings(QSettings::IniFormat,QSettings::UserScope,"Decodium","Decodium");settings.setValue("CW/remoteKey",false);settings.sync();std::cout<<"remote-disabled|"<<b.cwCanTransmit()<<'\n';}
