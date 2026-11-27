#include <QtCore>
#include <algorithm>
#include <cmath>
#include <utility>
#include <cstdio>
#include <chrono>
static QTimeZone decodiumUtcTimeZone(){return QTimeZone(QByteArrayLiteral("UTC"));}
class DecodiumBridge { public:
 QVariantMap worldClockSnapshot(const QString&) const;
 QVariantList worldClockCityOptions(const QString&,int) const;
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv); DecodiumBridge b;
 const QStringList queries {"", "rome", "RoMa", "Londra", "america", "new york", "tokio", "UTC", "zzzz_unlikely", "São", "l"};
 if (argc > 1) {
  qint64 total=0;
  auto start=std::chrono::steady_clock::now();
  for (int i=0;i<100;i++) total+=b.worldClockCityOptions(queries[1+i%10],40).size();
  auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
  std::printf("%lld %lld\n",static_cast<long long>(ms),static_cast<long long>(total));
 } else {
  QJsonArray results;
  for (const auto &query : queries) for(int limit : {0,1,3,40,200}) results.append(QJsonValue::fromVariant(b.worldClockCityOptions(query,limit)));
  std::puts(QJsonDocument(results).toJson(QJsonDocument::Compact).constData());
 }
}
