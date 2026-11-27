#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVariantMap>
#include <limits>
#include <utility>
#include <iostream>
#include "DecodeMessageNormalization.h"
using decodium::decode_message::canonicalDecodeMessage;
struct FakePaths{enum StandardLocation{GenericDataLocation,AppDataLocation,AppLocalDataLocation};static QString root;static QString writableLocation(StandardLocation l){return root+((l==GenericDataLocation)?"/generic":(l==AppDataLocation)?"/app":"/local");}};QString FakePaths::root;
#define QStandardPaths FakePaths
struct Legacy{QString path;QString allTxtPath()const{return path;}};
static QDateTime approxUtcDateTimeForDisplayToken(const QString&){return QDateTime::fromString("2026-11-07T12:00:00Z",Qt::ISODate);}
class DecodiumBridge:public QObject{public:Legacy*m_legacyBackend=nullptr;bool legacy=false;QString m_mode="FT8";double m_frequency=14074000;bool usingLegacyBackendForTx()const{return legacy;}QString legacyAllTxtPath()const;void appendLegacyAllTxtDecodeLine(const QVariantMap&)const;};
#include "bridge_refactor_under_test.inc"
static void write(const QString& p,const QByteArray&d){QDir().mkpath(QFileInfo(p).absolutePath());QFile f(p);if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate))std::abort();f.write(d);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir t;if(!t.isValid())return 1;FakePaths::root=t.path();DecodiumBridge b;Legacy backend;b.m_legacyBackend=&backend;
 QString embedded=t.path()+"/embedded/ALL.TXT";backend.path=embedded;if(b.legacyAllTxtPath()!=embedded)return 2;QVariantMap e{{"time","120000"},{"db","-10"},{"dt","0.3"},{"freq","1500"},{"message"," CQ K1ABC FN42 "}};b.appendLegacyAllTxtDecodeLine(e);QFile f(embedded);if(!f.open(QIODevice::ReadOnly))return 3;auto out=f.readAll();if(!out.contains("CQ K1ABC FN42")||!out.contains("FT8")||!out.contains("14.074"))return 4;f.close();b.legacy=true;b.appendLegacyAllTxtDecodeLine(e);e["isTx"]=true;b.legacy=false;b.appendLegacyAllTxtDecodeLine(e);if(QFileInfo(embedded).size()!=out.size())return 5;
 backend.path.clear();QString generic=t.path()+"/generic/ft2/ALL.TXT",local=t.path()+"/app/ALL.TXT";write(generic,QByteArray(200,'x'));write(local,QByteArray(20,'x'));if(b.legacyAllTxtPath()!=local)return 6;
 QFile::remove(local);if(b.legacyAllTxtPath()!=generic)return 7;QFile::remove(generic);if(b.legacyAllTxtPath()!=t.path()+"/app/ALL.TXT")return 8;
 std::cout<<"embedded/fallback paths, local preference, formatted append, TX and legacy write guards: OK\n";
}
