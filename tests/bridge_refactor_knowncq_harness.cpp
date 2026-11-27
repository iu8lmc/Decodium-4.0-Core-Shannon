#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTimeZone>
#include <algorithm>
#include <utility>
#include <cassert>
#include <iostream>
QStringList known,hashes;
extern "C" void ftx_ft8_stage4_seed_known_cq_c(char const* call,char const* grid,float freq,float dt,int nutc){known<<QString::fromLatin1(call,12).trimmed()+"|"+QString::fromLatin1(grid,4).trimmed()+"|"+QString::number(freq)+"|"+QString::number(dt)+"|"+QString::number(nutc);}
extern "C" void ftx_ft8_stage4_seed_hash_call_c(char const* call){hashes<<QString::fromLatin1(call,13).trimmed();}
void bridgeLog(QString const&){}
class DecodiumBridge {public:QString path,m_legacyAllTxtConsumedPath; qint64 m_legacyAllTxtConsumedSize=0; QString legacyAllTxtPath() const{return path;} void moveLegacyAllTxtCursorToEnd();void seedFt8KnownCqCacheFromAllTxt();};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTemporaryDir tmp;assert(tmp.isValid());DecodiumBridge b;b.path=tmp.filePath("missing");b.moveLegacyAllTxtCursorToEnd();assert(b.m_legacyAllTxtConsumedSize==-1);b.seedFt8KnownCqCacheFromAllTxt();assert(known.empty()&&hashes.empty());
 auto line=[](QDateTime t,QString msg){return t.toUTC().toString("yyMMdd_hhmmss")+" 14.074 Rx FT8 -10 0.2 1500 "+msg+"\n";};
 QString text=line(QDateTime::currentDateTimeUtc().addSecs(-30),"CQ K1ABC FN42")+line(QDateTime::currentDateTimeUtc().addSecs(-15),"CQ K1ABC FN42")+line(QDateTime::currentDateTimeUtc().addSecs(-14400),"CQ K2DEF FN31")+line(QDateTime::currentDateTimeUtc().addSecs(-10),"CQ NOPE RR73")+"malformed garbage\n";
 b.path=tmp.filePath("ALL.TXT");QFile f(b.path);assert(f.open(QIODevice::WriteOnly));f.write(text.toUtf8());f.close();b.moveLegacyAllTxtCursorToEnd();assert(b.m_legacyAllTxtConsumedPath==QFileInfo(b.path).absoluteFilePath()&&b.m_legacyAllTxtConsumedSize==f.size());
 qputenv("DECODIUM_FT8_SEED_KNOWN_CQ_REPLAY","1");b.seedFt8KnownCqCacheFromAllTxt();assert(known.empty()&&hashes.contains("K1ABC")&&hashes.contains("K2DEF"));
 QString jtdx=tmp.filePath("JTDX-ALL.TXT");assert(QFile::copy(b.path,jtdx));b.path=jtdx;hashes.clear();b.seedFt8KnownCqCacheFromAllTxt();assert(known.size()==2&&known[0].startsWith("K1ABC|FN42|1500|0.2|")&&known[0]==known[1]);assert(!hashes.contains("NOPE")&&hashes.contains("K1ABC"));
 qunsetenv("DECODIUM_FT8_SEED_KNOWN_CQ_REPLAY");std::cout<<"ALL.TXT cursor, missing file, hash calls, source gate, age and CQ replay: OK\n";}
