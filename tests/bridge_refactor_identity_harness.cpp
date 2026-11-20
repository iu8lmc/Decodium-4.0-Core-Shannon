#include <QtCore>
#include <cstdio>
#include "Sequencer/MessageTokenRules.hpp"
using decodium::seq::normalizeCallToken;
using decodium::seq::normalizedUsableCallToken;
using decodium::seq::isPlausibleDecodedCallsignToken;
using decodium::seq::normalizedBaseCall;
using decodium::seq::tokenMatchesCall;
class DecodiumBridge { public:
 QStringList locals;
 QStringList localDecodeCallCandidates() const { return locals; }
 QString extractDecodedCallsign(QString const&,bool) const;
 QString extractDecodedGrid(QString const&) const;
};
#include "bridge_refactor_identity.inc"
int main(int argc,char**argv) {
 QCoreApplication app(argc,argv); DecodiumBridge b;
 QStringList messages{"CQ K1ABC FN42", "cq dx 9h1sr jm75", "IU8LMC K1ABC -10", "K1ABC IU8LMC R-09", "CQ TEST II9MESC JN70", "RR73", "", "<...> FN42", "123ABC K1ABC FN42AA", "CQ 0123456789ABCDEF", "CQ K1ABC FN42 JM75aa"};
 for(auto const& locals : QList<QStringList>{{}, {"IU8LMC"}, {"K1ABC/P"}, {"IU8LMC", "9H1SR"}}) {
  b.locals=locals;
  for(auto const& m : messages) for(bool cq : {false,true})
   printf("%s|%s\n",b.extractDecodedCallsign(m,cq).toUtf8().constData(),b.extractDecodedGrid(m).toUtf8().constData());
 }
}
