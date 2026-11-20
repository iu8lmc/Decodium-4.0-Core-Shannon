#include <QtCore>
#include <cstdio>
#include "DecodeMessageNormalization.h"
#include "Decoder/decodedtext.h"
extern "C" bool stdmsg_(char const*, fortran_charlen_t){return false;}
#include "DecodeUtcTime.h"
#include "LegacyDecodeWindow.hpp"
#include "Sequencer/MessageTokenRules.hpp"
using namespace decodium::decode_message;using namespace decodium::seq;
static void bridgeLog(QString const&){}
struct MainThreadTraceScope {MainThreadTraceScope(QString,QString,int){}void addDetail(QString){}};
struct DecodeUserFilterConfig{bool block=false;};
static bool shouldAcceptDecodeEntryByUserFilters(QVariantMap const&e,DecodeUserFilterConfig const& f,bool,QString* reason){if(f.block&&e.value("message").toString().contains("K1ABC")){if(reason)*reason="blocked";return false;}return true;}
static QString decodeMirrorEntryKey(QVariantMap const&e){return e.value("time").toString()+"|"+e.value("freq").toString()+"|"+e.value("message").toString();}
static QString decodeMirrorSemanticKey(QVariantMap const&e){return e.value("freq").toString()+"|"+e.value("message").toString();}
class DecodiumBridge {public:QString m_mode="FT8";bool m_hideGhostDecodes=false;mutable int enriched=0;DecodeUserFilterConfig config;
 DecodeUserFilterConfig readDecodeUserFilterConfig()const{return config;}QStringList localDecodeCallCandidates()const{return {"IU8LMC"};}bool isTimeSyncDecodeMode(QString const&)const{return true;}int periodMsForMode(QString const&)const{return 15000;}
 QString extractDecodedCallsign(QString const&m,bool)const{return m.contains("K1ABC")?"K1ABC":"9H1SR";}void enrichDecodeEntry(QVariantMap&e)const{++enriched;e["marker"]="enriched";}bool shouldAcceptDecodedMessage(QString const&m,QString*)const{return !m.contains("INVALID");}bool looksLikeGhostDecode(QVariantMap const&e)const{return e.value("message").toString().contains("GHOST");}
 QVariantList mirrorLegacyDecodeLines(QStringList const&,bool,QVariantList const&)const;QVariantList mirrorLegacyDecodeLines(QStringList const&,bool,QVariantList const&,DecodeUserFilterConfig const&)const;
};
#include "bridge_refactor_legacymirror.inc"
static void show(char const* tag,QVariantList const& v,int enriched){printf("%s|count=%d|enriched=%d|%s\n",tag,int(v.size()),enriched,QJsonDocument::fromVariant(v).toJson(QJsonDocument::Compact).constData());}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;QStringList rows{"1200 -10 0.1 1500 ~ CQ K1ABC FN42", "120015 -08 0.2 1600 ~ CQ 9H1SR JM75", "120030 Tx 1500 ~ K1ABC IU8LMC 73", "1200 -10  0.1 1500~   CQ W1AW FN31", "120045 -20 0.2 1500 ~ INVALID", "------"};auto first=b.mirrorLegacyDecodeLines(rows,false,{});show("first",first,b.enriched);for(auto const& value:first){auto e=value.toMap();if(e.value("message").toString()=="CQ W1AW FN31")printf("fallback=%s|%s|%s|%s\n",e.value("time").toString().toUtf8().constData(),e.value("db").toString().toUtf8().constData(),e.value("dt").toString().toUtf8().constData(),e.value("freq").toString().toUtf8().constData());}auto again=b.mirrorLegacyDecodeLines(rows,true,first);show("reused",again,b.enriched);b.config.block=true;auto filtered=b.mirrorLegacyDecodeLines(rows,false,first);show("filtered",filtered,b.enriched);b.config.block=false;b.m_hideGhostDecodes=true;auto ghost=b.mirrorLegacyDecodeLines({"120100 -15 0.1 1700 ~ GHOST"},false,{});show("ghost",ghost,b.enriched);QStringList many;for(int i=0;i<390;i++)many<<QString("1200 -10 0.1 1500 ~ CQ K1ABC FN42");auto limited=b.mirrorLegacyDecodeLines(many,false,{});printf("window=%d\n",int(limited.size()));}
