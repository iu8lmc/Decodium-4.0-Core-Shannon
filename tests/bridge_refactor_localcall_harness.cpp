#include <QtCore>
#include <cstdio>
#include "Sequencer/MessageTokenRules.hpp"
using decodium::seq::normalizedUsableCallToken;
using decodium::seq::normalizedMessageTokens;
using decodium::seq::isDirectedCqModifierToken;
struct Legacy { QString call; QString callsign() const {return call;} };
class DecodiumBridge {public:
 QString m_callsign,m_lastTransmittedMessage,m_pendingAutoSeqMessage,current;
 Legacy* m_legacyBackend=nullptr;
 QString buildCurrentTxMessage()const{return current;}
 QStringList localDecodeCallCandidates()const;
};
#include "bridge_refactor_localcall.inc"
int main(int argc,char**argv) {
 QCoreApplication app(argc,argv);DecodiumBridge b;Legacy l;
 for(QString const& own:QStringList{"IU8LMC", "", "9h1sr/p"})
  for(QString const& legacy:QStringList{"", "IU8LMC", "K1ABC"})
   for(QString const& payload:QStringList{"CQ 9H1SR JM75", "CQ DX K1ABC FN42", "K1ABC IU8LMC -10", "CQ TEST 9H1SR JM75", "", "RR73"})
    for(int kind=0;kind<3;kind++) {
     b.m_callsign=own;l.call=legacy;b.m_legacyBackend=legacy.isEmpty()?nullptr:&l;b.m_lastTransmittedMessage.clear();b.m_pendingAutoSeqMessage.clear();b.current.clear();
     if(kind==0)b.m_lastTransmittedMessage=payload;else if(kind==1)b.m_pendingAutoSeqMessage=payload;else b.current=payload;
     printf("%s\n",b.localDecodeCallCandidates().join('|').toUtf8().constData());
    }
}
