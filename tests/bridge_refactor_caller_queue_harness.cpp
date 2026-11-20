#include <QtCore>
#include <cstdio>
#include "Radio.hpp"
#include "Sequencer/MessageTokenRules.hpp"
using decodium::seq::normalizedBaseCall;
static void bridgeLog(const QString&){}
static void decodiumBridgeCallerQueueLog(const QString&){}
class DecodiumBridge { public:
 bool m_manualTxHold=false,m_callerQueueManualOrder=false,m_logAfterOwn73=false,m_ft2DeferredLogPending=false,m_quickPeerSignaled=false,m_qsoLogged=false, allowAdvance=true,restart=true,tx=false;
 QStringList m_callerQueue; QSet<QString> m_callerQueuePinned; QHash<QString,qint64> m_callerQueueArrivalMs;
 QString m_mode="FT8",m_reportSent,m_autoSeqRogerReportBase,m_lastTransmittedMessage,m_grid="JM75ab",dxCall,dxGrid;
 double m_frequency=7074000; int m_rxFrequency=900,m_nTx73=4,m_txRetryCount=3,m_lastNtx=2,m_lastCqPidx=2,m_txWatchdogTicks=1,m_autoCQPeriodsMissed=2,notifications=0,ranked=0,removed=0,lastState=0;
 qint64 m_asyncLastTxEndMs=1; QDateTime m_qsoStartedOn,m_qsoFirstReplyOn;
 void callerQueueChanged(){++notifications;} void removeActiveMamCallerFromQueue(const QString&){++removed;} void rankCallerQueue(const QString&){++ranked;}
 bool isRecentAutoCqDuplicate(const QString& c,double,const QString&){return c=="OLD";}
 void clearPendingAutoLogSnapshot(){} void clearAutoCqPartnerLock(){} void clearPendingAutoSeqTx(const QString&){}
 void setReportSent(const QString& s){m_reportSent=s;} void setReportReceivedPlaceholder(const QString&){}
 void setDxCall(const QString& s){dxCall=s;} void setDxGrid(const QString& s){dxGrid=s;}
 void setRxFrequency(int f){m_rxFrequency=f;} void genStdMsgs(const QString&,const QString&){}
 bool advanceQsoState(int s){lastState=s;return allowAdvance;} void updateAutoCqPartnerLock(){}
 void setTxEnabled(bool v){tx=v;} bool autoCqCanRestartTx6()const{return restart;}
 void enqueueCallerInternal(const QString& c){m_callerQueue<<c;} void removeCallerFromQueue(const QString& c){m_callerQueue.removeAll(c);}
 void processNextInQueue(); void enqueueStation(const QString&); void dequeueStation(const QString&); void moveCallerQueueItem(int,int); void clearCallerQueue();
};
#include "bridge_refactor_caller_queue.inc"
static void check(bool ok,const char* n){if(!ok){fprintf(stderr,"FAIL %s\n",n);exit(1);}puts(n);}
int main(){
 DecodiumBridge b; b.m_callerQueue<<"K1ABC 1000 -10";b.m_manualTxHold=true;b.processNextInQueue();check(b.m_callerQueue.size()==1&&b.removed==0,"hold-keeps-queue");
 b.m_manualTxHold=false;b.m_callerQueue.prepend("OLD 900 -5");b.m_callerQueue.append("N0CALL 800 5");b.processNextInQueue();
 check(b.dxCall=="K1ABC"&&b.m_rxFrequency==1000&&b.m_reportSent=="-10"&&b.lastState==2&&b.tx&&b.m_callerQueue.size()==1,"skip-duplicate-and-start-next");
 check(b.m_nTx73==0&&b.m_txRetryCount==0&&b.m_lastNtx==-1&&b.m_qsoStartedOn.isValid(),"reset-qso-state");
 b.tx=false;b.m_callerQueue.clear();b.processNextInQueue();check(b.lastState==6&&b.tx,"empty-restarts-cq");
 b.tx=false;b.restart=false;b.lastState=0;b.processNextInQueue();check(!b.tx&&b.lastState==0,"empty-no-restart");
 b.enqueueStation(" k2abc ");check(b.m_callerQueuePinned.contains("K2ABC")&&b.ranked>0,"manual-enqueue-pins");
 b.enqueueStation("W1XYZ");b.enqueueStation("N2XYZ");b.moveCallerQueueItem(0,99);check(b.m_callerQueue.last()==" k2abc "&&b.m_callerQueueManualOrder,"move-clamps-and-pins-order");
 int notifications=b.notifications;b.moveCallerQueueItem(-1,0);b.moveCallerQueueItem(2,2);check(b.notifications==notifications,"invalid-and-noop-move");
 b.dequeueStation("W1XYZ");check(!b.m_callerQueue.contains("W1XYZ"),"dequeue");
 b.m_callerQueueArrivalMs.insert("K2ABC",1);b.clearCallerQueue();check(b.m_callerQueue.isEmpty()&&b.m_callerQueuePinned.isEmpty()&&b.m_callerQueueArrivalMs.isEmpty()&&!b.m_callerQueueManualOrder,"clear-resets-metadata");
 b.clearCallerQueue();check(b.m_callerQueue.isEmpty(),"clear-empty");
 DecodiumBridge fail;fail.allowAdvance=false;fail.m_callerQueue<<"K9ABC 1000 90";fail.processNextInQueue();check(fail.m_reportSent=="49"&&fail.lastState==2&&!fail.tx,"clamp-report-and-abort-transition");
 DecodiumBridge portable;portable.enqueueStation("EA8/K1ABC");check(portable.m_callerQueuePinned.contains("K1ABC"),"portable-pin-base-call");
 portable.processNextInQueue();check(portable.dxCall=="K1ABC"&&portable.tx,"portable-selection-base-call");
}
