#include <QtCore>
#include <cstdio>
#include "DecodeTokenClassifier.h"
#include "Sequencer/MessageTokenRules.hpp"
using namespace decodium::seq;using decodium::decode_tokens::isGridToken;using decodium::decode_tokens::looksLikeCallsignToken;
#include "bridge_refactor_unresolved.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);printf("placeholder_modifier=%d placeholder_recognized=%d\n",isDirectedCqModifierToken("..."),isPlaceholderCallToken("..."));QStringList msgs{"<...> FN42","CQ <...> FN42","CQ K1ABC FN42","IU8LMC <...> -10","<...> IU8LMC R-10","<...> FN42 37","<...>","", "  K1ABC IU8LMC 73  "};for(auto const&m:msgs)for(auto const& mode:QStringList{"FT8","FST4W","FST4W-120","OTHER"})for(bool tx:{false,true}){QString grid;bool u=messageIsUnresolvedGridOnlyDecode(m,&grid);printf("%s|%s|%d|%d|%s|%d|%d|%s\n",m.toUtf8().constData(),mode.toUtf8().constData(),tx,u,grid.toUtf8().constData(),messageHasUnresolvedPeerPlaceholder(m,"IU8LMC"),isFst4wUnresolvedHashDecode(mode,m),displayMessageWithCallsignsActorFirst(m,tx,"IU8LMC").toUtf8().constData());}}
