#include <QtCore>
#include <cstdio>
#include "Sequencer/MessageTokenRules.hpp"
using namespace decodium::seq;
#include "bridge_refactor_telemetry.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(auto const&m:QStringList{"A1B2C3D4E5", "1234ABCD", "K1ABC", "<1234ABCD>", "123456789", "ABCDEF12", "CQ K1ABC FN42", "< >", "> <", "x", "", "A1B2;C3D4", "1234ABCDEF123456789"})for(int snr:{-30,10}){printf("%s|%d|%d|%d|%d\n",m.toUtf8().constData(),snr,decodeListModel_isTelemetryHexToken(m),decodeListModel_isTelemetryOnlyMessage(m),isValidDecode(m,snr,"?",0.0f));}}
