#include <QtCore>
#include <cstdio>
#include "Sequencer/MessageTokenRules.hpp"
using decodium::seq::normalizedUsableCallToken;using decodium::seq::isPlausibleDecodedCallsignToken;
#include "bridge_refactor_tokenclass.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(auto const&t:QStringList{"FN42","fn42aa","RR73","JN70","AA00ZZ","XYZ","K1ABC","9H1SR/P","<...>","1234ABCD","A1B2C3D4E5",""," AA00 ","CQ","1234567890123456789"})printf("%s|%d|%d|%d\n",t.toUtf8().constData(),isGridToken(t),isTelemetryHexToken(t),looksLikeCallsignToken(t));}
