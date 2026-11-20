#include <QCoreApplication>
#include <QString>
#include <QVector>
#include <iostream>
static void bridgeLog(const QString&){}
struct CwKeyEvent{int deltaMs;bool down;};namespace decodium::cw{QVector<CwKeyEvent>timelineFor(const QString&,int){return {{20,true},{20,false}};}}
struct RemoteRadioLink{bool supported=true,result=true;int sends=0,ptton=0,pttoff=0;bool supportsCwKey()const{return supported;}bool sendCwKey(const QVector<CwKeyEvent>&,int){++sends;return result;}void setPtt(bool on,int){on?++ptton:++pttoff;}int txAudioLeadMs()const{return 150;}};
struct Tone{int clears=0;void clear(){++clears;}};
class DecodiumBridge:public QObject {public:
 bool m_transmitting=false,m_tuning=false,m_decoPortUseRemote=false,m_rttyTxActive=false,m_cwRemoteKeying=false,m_cwTxActive=false,allowed=true,sstv=false;int m_cwSidetoneHz=700,plays=0,stops=0,audio=0;Tone*m_cwSidetone=nullptr;RemoteRadioLink*remote=nullptr;
 bool cwCanTransmit(){return allowed;}bool sstvTxActive()const{return sstv;}RemoteRadioLink*remoteLinkIfAny(){return remote;}void sendCwAudio(const QString&,int,int){++audio;m_transmitting=true;}bool playCwMonitor(const QVector<CwKeyEvent>&,int){++plays;return true;}void stopTx(){++stops;}
 bool cwSendAudio(const QString&,int);bool cwRemoteKeySupported();bool cwRemoteSendKey(const QVector<CwKeyEvent>&,int);void cwRemotePtt(bool);int cwRemoteLeadMs();void cwAbortAudio();
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;RemoteRadioLink remote;Tone tone;b.remote=&remote;b.m_cwSidetone=&tone;QVector<CwKeyEvent>ev{{20,true},{20,false}};std::cout<<"local|"<<b.cwRemoteKeySupported()<<"|"<<b.cwRemoteSendKey(ev,700)<<'\n';b.m_decoPortUseRemote=true;std::cout<<"supported|"<<b.cwRemoteKeySupported()<<"|"<<b.cwRemoteLeadMs()<<'\n';b.m_tuning=true;std::cout<<"busy|"<<b.cwRemoteKeySupported()<<'\n';b.m_tuning=false;std::cout<<"send|"<<b.cwRemoteSendKey(ev,700)<<"|"<<remote.sends<<"|"<<b.plays<<'\n';remote.result=false;std::cout<<"failed|"<<b.cwRemoteSendKey(ev,700)<<"|"<<b.plays<<'\n';b.cwRemotePtt(true);b.cwRemotePtt(false);std::cout<<"ptt|"<<remote.ptton<<"|"<<remote.pttoff<<"|"<<tone.clears<<"|"<<b.m_cwRemoteKeying<<'\n';b.allowed=false;std::cout<<"blocked-audio|"<<b.cwSendAudio("CQ",20)<<"|"<<b.audio<<'\n';b.allowed=true;std::cout<<"audio|"<<b.cwSendAudio("CQ",20)<<"|"<<b.audio<<"|"<<b.plays<<'\n';b.m_cwTxActive=true;b.cwAbortAudio();std::cout<<"abort|"<<b.stops<<"|"<<tone.clears<<'\n';}
