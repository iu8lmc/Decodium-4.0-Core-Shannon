#include <QtCore>
#include <cstdio>
#include "DecodeUserFilterConfig.h"
#include "DecodeMessageNormalization.h"
#include "Sequencer/MessageTokenRules.hpp"
using decodium::decode_message::canonicalDecodeMessage;
using decodium::seq::normalizedUsableCallToken;using decodium::seq::normalizedBaseCall;using decodium::seq::normalizedMessageTokens;using decodium::seq::isDirectedCqModifierToken;
bool decodiumBridgeLooksLikeCallsignToken(QString const&);
#define looksLikeCallsignToken decodiumBridgeLooksLikeCallsignToken
#include "bridge_refactor_userfilters.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QList<QVariantMap> entries;
 for(QString message:QStringList{"CQ K1ABC FN42","CQ DX 9H1SR JM75","IU8LMC K1ABC -10","CQ TEST 9H1SR JM75","", "CQ EA1ABC IN53"}) {QVariantMap e;e["message"]=message;e["fromCall"]=message.contains("K1ABC")?"K1ABC":"9H1SR";e["dxCountry"]="Malta";e["dxContinent"]="EU";entries<<e;}QVariantMap tx;tx["message"]="CQ K1ABC FN42";tx["db"]="TX";entries<<tx;
 for(int config=0;config<7;++config){DecodeUserFilterConfig f;f.blacklistEnabled=(config==1||config==4);f.blacklistTerms={"K1ABC"};f.whitelistEnabled=(config==2);f.whitelistTerms={"9H1"};f.alwaysPassEnabled=(config==4);f.passTerms={"K1ABC"};f.callingOnly=(config==5);if(config==5){f.blacklistEnabled=true;f.blacklistTerms={"9H1"};}if(config==3)f.territoryTerms={"EU"};f.waitPounceOnly=(config==6);if(config==6){f.blacklistEnabled=true;f.blacklistTerms={"K1ABC"};}
  for(auto const&e:entries)for(bool scope:{false,true}){QString reason;bool accepted=shouldAcceptDecodeEntryByUserFilters(e,f,scope,&reason);printf("%d|%d|%s|%d|%s\n",config,scope,e.value("message").toString().toUtf8().constData(),accepted,reason.toUtf8().constData());}}
}
