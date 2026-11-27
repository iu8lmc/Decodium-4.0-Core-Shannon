#include <QCoreApplication>
#include <QDateTime>
#include <QTimer>
#include <iostream>
static void bridgeLog(const QString&) {}
#define connect(...) ((void)0)
#define disconnect(...) ((void)0)
class DecodiumBridge:public QObject {public:
 QString m_targetCallSign,m_dxCall,m_targetCallSavedDxCall;int m_txPeriod=0,m_targetCallSavedTxPeriod=0,m_targetCallRetryCount=0,m_targetCallPeriod=0,m_targetCallMaxRetries=1,m_targetCallTimeoutS=10,m_targetCallPauseS=0,m_armedReArmCount=0,m_currentTx=1,m_qsoProgress=0;quint64 m_targetCallSessionId=0;QDateTime m_targetCallStartedUtc,m_targetCallLastTxUtc,m_targetCallLastSeenUtc;
 bool m_autoCallEnabled=false,m_targetCallActive=false,m_alt12Enabled=false,m_targetCallSavedAlt12=false,m_targetCallSeenSinceLastTx=false,m_targetCallWasTransmitting=false,m_transmitting=false,m_autoSeq=false,m_armedWatchEnabled=false,m_targetCallArmedWaiting=false,m_armedReArm=false,m_qsoLogged=false,m_txEnabled=false,held=false,requested=false;
 void targetCallActiveChanged(){}void targetCallRetryCountChanged(){}void targetCallArmedWaitingChanged(){}void statusMessage(const QString&){}
 void setAlt12Enabled(bool v){m_alt12Enabled=v;}void setTxPeriod(int v){m_txPeriod=v;}void setDxCall(const QString&v){m_dxCall=v;}void setAutoSeq(bool v){m_autoSeq=v;}void setTxEnabled(bool v){m_txEnabled=v;}void armExplicitManualTxRequest(int,const QString&){requested=true;}bool txWatchdogLimitConfigured()const{return false;}void engageManualTxHold(const QString&,bool){held=true;}
 void startTargetCall();void stopTargetCall();void beginTargetCallCalling();void endTargetCallNoQso(const QString&);
};
#include "bridge_refactor_targetcall_lifecycle.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto check=[](bool ok,const char*name){if(!ok){std::cerr<<name<<" failed\n";std::exit(1);}std::cout<<name<<'\n';};DecodiumBridge b;b.startTargetCall();check(!b.m_targetCallActive,"empty");b.m_targetCallSign="K1ABC";b.m_autoCallEnabled=true;b.startTargetCall();check(!b.m_targetCallActive,"autocall-blocked");b.m_autoCallEnabled=false;b.m_dxCall="OLD";b.m_txPeriod=0;b.m_armedWatchEnabled=true;b.m_armedReArm=true;b.startTargetCall();check(b.m_targetCallActive&&b.m_targetCallArmedWaiting&&!b.m_txEnabled&&b.m_dxCall=="K1ABC"&&b.m_txPeriod==1,"armed-start");b.beginTargetCallCalling();check(!b.m_targetCallArmedWaiting&&b.m_txEnabled&&b.requested,"simulated-calling");b.endTargetCallNoQso("test");check(b.m_targetCallArmedWaiting&&!b.m_txEnabled&&b.m_armedReArmCount==1,"rearm");b.beginTargetCallCalling();b.endTargetCallNoQso("test");b.beginTargetCallCalling();b.endTargetCallNoQso("test");b.beginTargetCallCalling();b.endTargetCallNoQso("test");check(b.held&&!b.m_targetCallActive&&b.m_dxCall=="OLD"&&b.m_txPeriod==0,"rearm-cap-stop");DecodiumBridge c;c.m_targetCallSign="K1ABC";c.m_targetCallPeriod=2;c.startTargetCall();check(c.m_targetCallActive&&c.m_txEnabled&&c.m_alt12Enabled,"simulated-immediate");c.stopTargetCall();check(!c.m_targetCallActive&&!c.m_txEnabled&&!c.m_alt12Enabled,"stop");}
