#include <QCoreApplication>
#include <QStringList>
#include <QString>
#include <iostream>
namespace Radio{QString base_callsign(QString s){return s.trimmed().toUpper();}}
static void bridgeLog(const QString&){}
class DecodiumBridge:public QObject {public:
 QString m_dxCall="K1ABC",m_dxGrid="FN42",m_lastTransmittedMessage="old",m_activeTxMessage="old",m_pendingAutoSeqPartnerBase="old",m_pendingAutoSeqMessage="old",m_pendingAutoSeqMode="FT8",m_reportSent="-05",m_reportReceived="-07",m_tx1="1",m_tx2="2",m_tx3="3",m_tx4="4",m_tx5="5",m_tx6="CQ",m_autoCqLockedCall="K1ABC",m_autoSeqRogerReportBase="K1ABC";
 bool m_reportReceivedReal=true,m_logAfterOwn73=true,m_ft2DeferredLogPending=true,m_quickPeerSignaled=true,m_multiAnswerMode=false,m_autoCqRepeat=false,m_transmitting=false,m_tuning=false,m_txEnabled=true,restart=false;
 int m_activeTxNumber=2,m_pendingAutoSeqTxAfterActiveTx=3,m_nTx73=2,m_lastNtx=3,m_lastCqPidx=3,m_txRetryCount=2,m_txWatchdogTicks=2,m_autoCQPeriodsMissed=2,m_qsoProgress=2,current=0,closed=0,removed=0,invalidated=0;
 QStringList m_callerQueue;
 void clearAutoCqPartnerLock(){m_autoCqLockedCall.clear();}void reportSentChanged(){}void reportReceivedChanged(){}void qsoProgressChanged(){}void setDxCall(const QString&s){m_dxCall=s;}void setDxGrid(const QString&s){m_dxGrid=s;}void setTx1(const QString&s){m_tx1=s;}void setTx2(const QString&s){m_tx2=s;}void setTx3(const QString&s){m_tx3=s;}void setTx4(const QString&s){m_tx4=s;}void setTx5(const QString&s){m_tx5=s;}void setCurrentTx(int v){current=v;}
 void markWorldMapQsoClosed(const QString&,const QString&){++closed;}bool autoCqCanRestartTx6(){return restart;}void invalidateDeferredTxRequests(const QString&){++invalidated;}void removeCallerFromQueue(const QString&){++removed;}void clearDeferredManualSyncTx(const QString&){}void clearPendingAutoSeqTx(const QString&){}void setTxEnabled(bool x){m_txEnabled=x;}
 void clearCompletedQsoTxFields(const QString&,const QString&);void resetCompletedQsoLatchesForCq(const QString&);void clearTxArmedAfterCompletedQso(const QString&,const QString&);
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge a;a.clearTxArmedAfterCompletedQso("K1ABC","test");std::cout<<"complete|"<<a.closed<<"|"<<a.invalidated<<"|"<<a.m_txEnabled<<"|"<<a.m_dxCall.isEmpty()<<"|"<<a.current<<"|"<<a.m_qsoProgress<<'\n';DecodiumBridge b;b.m_multiAnswerMode=true;b.m_callerQueue<<"K2XYZ";b.clearTxArmedAfterCompletedQso("K1ABC","test");std::cout<<"queue|"<<b.m_txEnabled<<"|"<<b.removed<<"|"<<b.invalidated<<"|"<<!b.m_dxCall.isEmpty()<<'\n';DecodiumBridge c;c.m_dxCall="K2XYZ";c.m_transmitting=true;c.clearTxArmedAfterCompletedQso("K1ABC","test");std::cout<<"other-active|"<<c.m_txEnabled<<"|"<<c.m_dxCall.toStdString()<<"|"<<c.invalidated<<'\n';DecodiumBridge d;d.m_dxCall="K2XYZ";d.clearCompletedQsoTxFields("K1ABC","test");std::cout<<"other-fields|"<<d.m_dxCall.toStdString()<<"|"<<d.m_tx1.toStdString()<<'\n';DecodiumBridge e;e.m_dxCall.clear();e.resetCompletedQsoLatchesForCq("test");std::cout<<"cq-reset|"<<e.m_logAfterOwn73<<"|"<<e.m_pendingAutoSeqTxAfterActiveTx<<"|"<<e.m_lastNtx<<"|"<<e.m_activeTxMessage.isEmpty()<<'\n';}
