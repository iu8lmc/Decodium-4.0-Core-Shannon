#include <QtCore>
#include <algorithm>
#include "AdifDocumentInternal.h"
#include "DecodiumBridgeQsoCacheInternal.h"
using namespace decodium::adif_document;
struct GeoDistanceInfo{int km=0;};GeoDistanceInfo geo_distance(const QString& a,const QString& b,double){return {a=="JM75"&&b=="FN42"?7000:0};}
#define DIAG_INFO(x) do{}while(false)
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir d;QString p=d.filePath("test.adi");QFile f(p);if(!f.open(QIODevice::WriteOnly))return 1;f.write("TEST\n<EOH>\n<CALL:5>K1ABC<GRIDSQUARE:4>FN42<QSO_DATE:8>20261106<TIME_ON:6>120000<BAND:3>20M<MODE:3>FT8<EOR>\n<CALL:5>K2XYZ<QSO_DATE:8>20261107<TIME_ON:6>120000<BAND:3>40M<MODE:3>FT4<EOR>\n");f.close();auto snap=buildQsoLogSnapshot(p,"JM75");QTextStream o(stdout);o<<"rows|"<<snap.rows.size()<<'|'<<snap.rows[0].toMap().value("call").toString()<<'|'<<snap.rows[1].toMap().value("call").toString()<<'\n';o<<"stats|"<<snap.stats.value("totalQsos").toInt()<<'|'<<snap.stats.value("uniqueCalls").toInt()<<'|'<<snap.stats.value("maxDistance").toInt()<<'|'<<snap.stats.value("farthestCall").toString()<<'\n';o<<"export|"<<(snap.rows[0].toMap().value("exportFingerprint").toString().size()==64)<<'|'<<(snap.rows[1].toMap().value("exportLogPath").toString()==p)<<'\n';auto missing=buildQsoLogSnapshot(d.filePath("missing"),"JM75");o<<"missing|"<<missing.rows.size()<<'|'<<missing.stats.value("totalQsos").toInt()<<'\n';}
