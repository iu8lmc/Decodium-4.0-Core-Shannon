#include <QtCore>
#include <cstdio>
#include "Radio.hpp"
#include "Sequencer/MessageTokenRules.hpp"
using namespace decodium::seq;
#include "bridge_refactor_validation.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(auto const&t:QStringList{"K1ABC","9H1SR/P","IU8LMC","TEST1","II9MESC","KS58OQ94M","0Z4SYH","ZH7RTG","1A0C","VZ1ABC","<...>","FN42","RR73","R-10","+03","R03","-7","R FN42","001","DX","", "73","TU"})printf("%s|%d%d%d%d%d%d%d%d%d%d%d|%d\n",t.toUtf8().constData(),isPlainSignalReportToken(t),isRogerSignalReportToken(t),isQsoSignoffToken(t),isDirectedDecodeTerminalToken(t),looksLikeMalformedRGrid(t),hasPortableOperatingDesignator(t),isPlausibleDecodedCallsign(t),isSyntheticLabCallsign(t),isPlausibleFt2CqCallsign(t),isCqRoutingModifierToken(t),hasHighConfidenceGhostPrefix(t),digitRunCount(t));
 for(auto const&p:QList<QStringList>{{},{"73"},{"-10"},{"R-10","TU"},{"R","FN42"},{"bad"},{"1","2","3"}})for(bool contest:{false,true}){QString reason;bool a=isDirectedDecodePayloadValid(p,contest,&reason);bool b=isResolvablePlaceholderDirectedPayload(p,contest);printf("payload=%s|%d|%d|%d|%s\n",p.join(' ').toUtf8().constData(),contest,a,b,reason.toUtf8().constData());}
 for(auto const&t:QList<QStringList>{{"CQ","K1ABC","FN42"},{"CQ","K1ABC","BAD"},{"CQ","K1ABC","?"}}){QString reason;printf("trailing=%s|%d|%s\n",t.join(' ').toUtf8().constData(),isValidFt2CqTrailingPayload(t,1,&reason),reason.toUtf8().constData());}printf("peer=%d,%d,%d\n",directedPeerLooksStructurallyGhost("ZH7RTG"),directedPeerLooksStructurallyGhost("K1ABC"),directedPeerLooksStructurallyGhost("<...>"));}
