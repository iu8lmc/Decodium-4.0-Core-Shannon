#include <QtCore>
#include <cstdio>
#include "DecodeValidationRules.h"
#include "SpecialOperationBounds.h"
#include "Radio.hpp"
#include "Sequencer/MessageTokenRules.hpp"
using namespace decodium::seq;using namespace decodium::decode_validation;
class DecodiumBridge {public:
 int m_specialOperationActivity=0,m_contestType=0,m_currentTx=0,m_qsoProgress=0;bool m_txEnabled=false,m_transmitting=false,m_tuning=false,contestSetting=false;QString m_mode="FT8",m_callsign="IU8LMC",m_dxCall,m_autoCqLockedCall,m_lastTransmittedMessage,m_pendingAutoSeqMessage;QDateTime m_lastTxActivityUtc;
 QVariant getSetting(QString,QVariant)const{return contestSetting;}bool isDirectedToLocalHashFromActivePartner(QString const&)const{return false;}QString inferredPartnerForAutolog()const{return {};}
 bool contestDecodeExchangesEnabled()const;bool shouldAcceptDecodedMessage(QString const&,QString*,bool)const;
};
#include "bridge_refactor_semantic.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QStringList msgs{"", "CQ K1ABC FN42", "CQ IU8LMC JN70", "CQ DX 9H1SR JM75", "CQ 0Z4SYH FN42", "CQ X9 FN42", "CQ KS58OQ94M FN42", "1234ABCD", "IU8LMC K1ABC -10", "IU8LMC K1ABC R FN42", "IU8LMC <...> -10", "<IU8LMC> II9MESC", "ZH7RTG K1ABC R-10", "K1ABC 9H1SR 73"};for(auto const& mode:QStringList{"FT8","FT2"})for(bool contest:{false,true})for(bool active:{false,true})for(bool allow:{false,true}){DecodiumBridge b;b.m_mode=mode;b.contestSetting=contest;if(active){b.m_dxCall="K1ABC";b.m_currentTx=2;b.m_qsoProgress=2;}for(auto const&m:msgs){QString reason;bool ok=b.shouldAcceptDecodedMessage(m,&reason,allow);printf("%s|%d|%d|%d|%s|%d|%s\n",mode.toUtf8().constData(),contest,active,allow,m.toUtf8().constData(),ok,reason.toUtf8().constData());}}}
