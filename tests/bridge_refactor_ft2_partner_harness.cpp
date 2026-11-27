#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QRegularExpression>
#include <iostream>
#include "DecodeValidationRules.h"
#include "Sequencer/MessageTokenRules.hpp"
#include "Radio.hpp"
using namespace decodium::seq;using namespace decodium::decode_validation;
void bridgeLog(QString const&){}
class DecodiumBridge {public:QString m_autoCqLockedCall,m_autoCqLockedGrid,m_ft2AutoCqAwaitingPartnerBase,m_ft2AutoCqAwaitingPartnerDecodeIdentity,m_mode="FT2",m_dxCall="K1ABC",m_callsign="IU8LMC",m_resumeTargetCall,m_pendingAutoSeqPartnerBase,m_lastTransmittedMessage,m_pendingAutoSeqMessage,m_lastAutoSeqDecodeIdentity="id";int m_autoCqLockedNtx=6,m_autoCqLockedProgress=1,m_ft2AutoCqAwaitingPartnerTx=0,m_pendingAutoSeqTxAfterActiveTx=0,m_currentTx=2,m_qsoProgress=3,m_lastNtx=-1,m_nTx73=0;qint64 m_ft2AutoCqAwaitingPartnerSinceMs=0;bool m_asyncTxEnabled=true,m_autoCqRepeat=true,m_ft2DeferredLogPending=false,m_logAfterOwn73=false;
 QString inferredPartnerForAutolog()const{return {};}QString buildCurrentTxMessage()const{return {};}
 void clearAutoCqPartnerLock();bool ft2AutoCqAwaitingPartnerDecode()const;bool isDirectedToLocalHashFromActivePartner(QString const&,QString*)const;bool isDirectedActivePartnerSignoffDecode(QStringList const&)const;bool signoffDecodeWatchActive()const;void armFt2AutoCqAwaitingPartnerDecode(int,QString const&);void clearFt2AutoCqAwaitingPartnerDecode(QString const&);};
#include "method.inc"
void check(bool x){if(!x)abort();}
int main(){DecodiumBridge b;check(!b.ft2AutoCqAwaitingPartnerDecode());b.armFt2AutoCqAwaitingPartnerDecode(2,"test");check(b.ft2AutoCqAwaitingPartnerDecode());b.m_dxCall="W1AW";check(!b.ft2AutoCqAwaitingPartnerDecode());b.m_dxCall="K1ABC";check(b.isDirectedActivePartnerSignoffDecode({"1200","-10","0","1500","IU8LMC K1ABC RR73"}));check(!b.isDirectedActivePartnerSignoffDecode({"1200","-10","0","1500","IU8LMC W1AW RR73"}));QString peer;check(!b.isDirectedToLocalHashFromActivePartner("<...> K1ABC RR73",&peer));b.m_currentTx=5;b.m_lastNtx=5;b.m_lastTransmittedMessage="K1ABC IU8LMC 73";check(b.signoffDecodeWatchActive());b.clearFt2AutoCqAwaitingPartnerDecode("test");check(!b.ft2AutoCqAwaitingPartnerDecode());b.m_autoCqLockedCall="K1ABC";b.clearAutoCqPartnerLock();check(b.m_autoCqLockedCall.isEmpty());std::cout<<"partner lock, directed signoff, unknown hash and signoff watch passed\n";}
