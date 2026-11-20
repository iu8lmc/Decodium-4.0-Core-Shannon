#include <QtCore>
#include <cstdio>
#include "Radio.hpp"
#include "Sequencer/MessageTokenRules.hpp"
#include "DecodeValidationRules.h"
using namespace decodium::seq;using namespace decodium::decode_validation;
static void bridgeLog(QString const&){}
static bool isStreamingKeyboardMode(QString const& mode){return mode.compare("RTTY",Qt::CaseInsensitive)==0 || mode.compare("JTTY",Qt::CaseInsensitive)==0;}
static bool grid2deg(QString const&,double&lon,double&lat){lon=0;lat=0;return true;}
static double azdist(double,double,double,double,double& az){az=0;return 0;}
struct DxccEntity {QString name="Test";double lat=0,lon=0;bool isValid()const{return true;}};
struct Lookup {bool isLoaded()const{return true;}DxccEntity lookup(QString const&)const{return {};}};
class DecodiumBridge {public:QString m_callsign="IU8LMC",m_dxCall,m_grid="JN70",m_mode="FT8";Lookup lookup,*m_dxccLookup=&lookup;QString extractDecodedGrid(QString const& m)const{for(auto const&t:m.split(' '))if(isGridTokenStrict(t))return t;return {};};bool looksLikeGhostDecode(QVariantMap const&)const;};
#include "bridge_refactor_ghost.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;auto row=[](QString m,QString db,QString time,QString mode="FT8"){QVariantMap e;e["message"]=m;e["displayMessage"]=m;e["db"]=db;e["time"]=time;e["mode"]=mode;return e;};QList<QVariantMap> cases{row("CQ K1ABC FN42","-10","120000"),row("CQ K1ABC FN42","-26","120015"),row("CQ 9H1SR JM75","-26","120030"),row("CQ 9H1SR JM75","-26","120045"),row("CQ ZH7RTG FN42","-10","120100"),row("CQ K1ABC R FN42","-10","120115"),row("random text","-25","120130","RTTY"),row("A1B2C3D4E5","-18","120145","FT2"),row("CQ IU8LMC JN70","-15","120200")};QVariantMap tx=row("CQ ZH7RTG FN42","-30","120215");tx["isTx"]=true;cases<<tx;for(auto const&e:cases)printf("%s|%s|%d\n",e.value("message").toString().toUtf8().constData(),e.value("time").toString().toUtf8().constData(),b.looksLikeGhostDecode(e));}
