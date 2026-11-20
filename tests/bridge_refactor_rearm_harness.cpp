#include <QString>
#include <QDateTime>
#include <iostream>
#define emit
void bridgeLog(QString const&){}
class DecodiumBridge {public:
 QString m_mode="FT2",m_dxCall="K1ABC",m_dxGrid="FN42",m_pendingAutoSeqMessage="msg",m_lastTransmittedMessage="old",m_autoSeqRogerReportBase="K1ABC",m_activeTxMessage="old",m_tx6="CQ",m_pendingAutoSeqPartnerBase="K1ABC",m_pendingAutoSeqMode="FT2";bool m_asyncTxEnabled=true,m_logAfterOwn73=true,m_ft2DeferredLogPending=true,m_quickPeerSignaled=true,m_qsoLogged=true,m_ft2AsyncSmartTxPending=true,preserve=false;
 int m_qsoProgress=3,m_pendingAutoSeqTxAfterActiveTx=4,m_lastNtx=3,m_txRetryCount=2,m_nTx73=2,m_currentTx=3,m_lastCqPidx=1,m_txWatchdogTicks=2,m_autoCQPeriodsMissed=2,m_activeTxNumber=3,m_ft2AsyncAudioQuietRuns=1,m_ft2AsyncSmartTxRetries=1;quint64 m_ft2AsyncSmartTxSerial=2;qint64 m_asyncLastTxEndMs=2,m_ft2AsyncLastDecodeMs=2,m_ft2AsyncFirstDecodeMs=2,m_ft2AsyncSmartTxDeadlineMs=2;QDateTime m_qsoStartedOn=QDateTime::currentDateTimeUtc(),m_qsoFirstReplyOn=QDateTime::currentDateTimeUtc();int clears=0;
 void clearDeferredManualSyncTx(QString const&){++clears;}void clearPendingAutoSeqTx(QString const&){m_pendingAutoSeqTxAfterActiveTx=0;m_pendingAutoSeqMessage.clear();++clears;}void clearPendingAutoLogSnapshot(){++clears;}void clearLateAutoLogSnapshot(){++clears;}void clearAutoCqPartnerLock(){++clears;}void setDxCall(QString s){m_dxCall=s;}void setDxGrid(QString s){m_dxGrid=s;}void qsoProgressChanged(){}void setCurrentTx(int x){m_currentTx=x;}bool applyDeferredAutoSeqTxForRearm(QString const&){return preserve;}
 void clearFt2AsyncAbortQsoState(QString const&);void resetManualTxRearmState(QString const&);};
#include "method.inc"
void check(bool x){if(!x)abort();}
int main(){DecodiumBridge a;a.m_mode="FT8";a.clearFt2AsyncAbortQsoState("test");check(a.m_dxCall=="K1ABC"&&a.clears==0);a.m_mode="FT2";a.clearFt2AsyncAbortQsoState("test");check(a.m_dxCall.isEmpty()&&a.m_dxGrid.isEmpty()&&a.m_qsoProgress==0&&a.m_currentTx==6&&a.clears==5&&!a.m_ft2AsyncSmartTxPending&&a.m_ft2AsyncSmartTxSerial==3);
 DecodiumBridge b;b.resetManualTxRearmState("test");check(b.m_txRetryCount==0&&b.m_pendingAutoSeqTxAfterActiveTx==0&&!b.m_logAfterOwn73&&!b.m_ft2DeferredLogPending&&b.m_autoSeqRogerReportBase.isEmpty());DecodiumBridge c;c.preserve=true;c.m_currentTx=5;c.resetManualTxRearmState("test");check(c.m_logAfterOwn73&&c.m_ft2DeferredLogPending&&c.m_autoSeqRogerReportBase=="K1ABC");std::cout<<"FT2 abort, idle-mode guard, stale rearm and preserved 73 passed\n";}
