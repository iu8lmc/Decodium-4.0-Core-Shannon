#include <QString>
#include <QHash>
#include <iostream>
#include "Radio.hpp"
#include "Sequencer/MessageTokenRules.hpp"
using decodium::seq::messageContainsCallToken;
void bridgeLog(QString const&){}
class DecodiumBridge {public:
 int m_pendingAutoSeqTxAfterActiveTx=0,m_currentTx=2;QString m_pendingAutoSeqPartnerBase,m_pendingAutoSeqMessage,m_pendingAutoSeqMode,m_mode="FT8",m_dxCall="K1ABC";bool m_autoCqRepeat=false,m_asyncTxEnabled=false,m_ft2PartnerMemoryEnabled=true,m_logAfterOwn73=false,m_ft2DeferredLogPending=true;QHash<QString,int> m_partnerMemory;int lockUpdates=0;bool advance=true;
 QString txMessage(int)const{return "K1ABC IU8LMC RR73";}bool advanceQsoState(int n){if(!advance)return false;m_currentTx=n;return true;}void updateAutoCqPartnerLock(){++lockUpdates;}
 void clearPendingAutoSeqTx(QString const&);void forgetPartnerMemoryForCall(QString const&,QString const&);bool shouldPreserveDeferredAutoSeqTxForRearm(QString*)const;bool applyDeferredAutoSeqTxForRearm(QString const&);};
#include "method.inc"
void check(bool x){if(!x)abort();}
int main(){DecodiumBridge b;QString why;b.m_pendingAutoSeqTxAfterActiveTx=3;b.m_pendingAutoSeqPartnerBase="K1ABC";b.m_pendingAutoSeqMode="FT8";b.m_pendingAutoSeqMessage="K1ABC IU8LMC R-10";check(b.shouldPreserveDeferredAutoSeqTxForRearm(&why));b.m_pendingAutoSeqMode="FT4";check(!b.shouldPreserveDeferredAutoSeqTxForRearm(&why)&&why.contains("mode changed"));b.m_pendingAutoSeqMode="FT8";b.m_pendingAutoSeqPartnerBase="W1AW";check(!b.shouldPreserveDeferredAutoSeqTxForRearm(&why)&&why.contains("partner changed"));b.m_pendingAutoSeqPartnerBase="K1ABC";b.m_pendingAutoSeqMessage="W1AW IU8LMC RR73";check(!b.shouldPreserveDeferredAutoSeqTxForRearm(&why)&&why.contains("does not mention"));b.m_pendingAutoSeqMessage="K1ABC IU8LMC 73";b.m_pendingAutoSeqTxAfterActiveTx=5;check(b.applyDeferredAutoSeqTxForRearm("test")&&b.m_currentTx==5&&b.m_logAfterOwn73&&!b.m_ft2DeferredLogPending&&b.lockUpdates==1&&b.m_pendingAutoSeqTxAfterActiveTx==0);
 b.m_pendingAutoSeqTxAfterActiveTx=2;b.m_mode="FT2";b.m_autoCqRepeat=true;b.m_asyncTxEnabled=true;b.m_ft2PartnerMemoryEnabled=false;check(b.shouldPreserveDeferredAutoSeqTxForRearm(&why));b.clearPendingAutoSeqTx("test");check(b.m_pendingAutoSeqTxAfterActiveTx==0&&b.m_pendingAutoSeqMessage.isEmpty());b.m_partnerMemory.insert("K1ABC",1);b.m_partnerMemory.insert("W1AW",2);b.forgetPartnerMemoryForCall("K1ABC/P","test");check(!b.m_partnerMemory.contains("K1ABC")&&b.m_partnerMemory.contains("W1AW"));std::cout<<"mode, partner, payload, deferred 73, FT2 CQ, clearing and memory passed\n";}
