#include <QtCore>
#include <cstdio>
namespace decodium::txmsg {
bool isStandardFtxCall(QString const& c) { return c == "K1ABC" || c == "9H1SR"; }
QString bracketHashCall(QString const& c) { return "<" + c + ">"; }
}
#include "bridge_refactor_ftx_helpers.inc"
int main(int argc, char **argv) {
 QCoreApplication app(argc, argv);
 const QStringList calls{"K1ABC", "9H1SR", "II9MESC", "", "  k1abc  "};
 const QStringList modes{"FT8", "ft4", "FT2", "JT65", ""};
 for (auto const& mode : modes) for (auto const& own : calls) for (auto const& dx : calls)
  for (int n = 0; n <= 6; ++n) for (bool rr : {false,true}) for (bool quick : {false,true}) {
   auto q = standardFtxQsoMessage(n,dx,own," jm75aa ","R-08",rr,quick,mode);
   printf("%s\n", q.toUtf8().constData());
  }
 for (auto const& mode : modes) for (auto const& own : calls) {
  auto cq=standardFtxCqMessage(own,"jm75aa",mode);
  for (auto const& p : QStringList{"CQ TEST", "CQ DX", "bad", "", "CQ  EU"}) {
   auto expanded=ftxCqMessageWithCustomPrefix(cq,p);
   printf("%s|%s|%s\n",cq.toUtf8().constData(),expanded.toUtf8().constData(),customCqPrefixFromMessage(expanded,own).toUtf8().constData());
  }
 }
}
