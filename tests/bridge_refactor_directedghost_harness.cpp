#include <QtCore>
#include <cstdio>
#include "Radio.hpp"
#include "Sequencer/MessageTokenRules.hpp"
#include "DecodeValidationRules.h"
using namespace decodium::seq;using namespace decodium::decode_validation;
namespace decodium {QVector<quint32> ft2MessageCallHashes(QString const&){return {1};}}
static void bridgeLog(QString const&){}static bool grid2deg(QString const&,double&lon,double&lat){lon=0;lat=0;return true;}static double distanceKm=0;static double azdist(double,double,double,double,double&az){az=0;return distanceKm;}
struct DxccEntity {QString name="TEST";double lat=0,lon=0;bool valid=true;bool isValid()const{return valid;}};
struct Lookup{bool loaded=true,valid=true;bool isLoaded()const{return loaded;}DxccEntity lookup(QString const&)const{DxccEntity e;e.valid=valid;return e;}};struct HashCache{bool found=false;bool containsHash28(quint32,qint64)const{return found;}};
class DecodiumBridge{public:
 QString m_callsign="IU8LMC",m_dxCall,m_autoCqLockedCall,m_resumeTargetCall,m_pendingAutoSeqPartnerBase,m_lateAutoLogCall,m_lastTransmittedMessage,m_pendingAutoSeqMessage,m_mode="FT2";bool m_lateAutoLogValid=false;QDateTime m_lateAutoLogExpires;Lookup lookup,*m_dxccLookup=nullptr;bool m_ft2ApHashCache=false;HashCache m_hashedCallsignCache;mutable QSet<QString> m_loggedDirectedGhostMsgs;mutable QHash<QString,QPair<QString,qint64>> m_directedWeakPeerSeen;
 QString inferredPartnerForAutolog()const{return {};}QString buildCurrentTxMessage()const{return {};}QString extractDecodedGrid(QString const&m)const{for(auto const&t:m.split(' '))if(isGridTokenStrict(t))return t;return {};}
 bool directedDecodePeerHasInvalidDxcc(QString const&,QString const&,QString const&,bool=true)const;bool shouldSuppressDirectedGhostDecode(QStringList const&,QString const&)const;bool shouldSuppressDirectedGhostDecode(QVariantMap const&,QString const&)const;
};
#include "bridge_refactor_directedghost.inc"
static QStringList row(QString t,QString snr,QString msg){return {t,snr,"0.1","1500",msg,"","","1500"};}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;printf("empty=%d other=%d\n",b.shouldSuppressDirectedGhostDecode(QStringList{},"test"),b.shouldSuppressDirectedGhostDecode(row("120000","-10","CQ K1ABC FN42"),"test"));
 printf("weak-first=%d\n",b.shouldSuppressDirectedGhostDecode(row("120000","-20","IU8LMC K1ABC -10"),"test"));printf("weak-repeat=%d\n",b.shouldSuppressDirectedGhostDecode(row("120015","-20","IU8LMC K1ABC -10"),"test"));b.m_ft2ApHashCache=true;b.m_hashedCallsignCache.found=true;printf("weak-cached=%d\n",b.shouldSuppressDirectedGhostDecode(row("120025","-20","IU8LMC 9H1SR -10"),"test"));b.m_ft2ApHashCache=false;b.m_dxCall="K1ABC";printf("active=%d\n",b.shouldSuppressDirectedGhostDecode(row("120030","-30","IU8LMC K1ABC -10"),"test"));b.m_dxCall.clear();b.m_lateAutoLogValid=true;b.m_lateAutoLogCall="K1ABC";b.m_lateAutoLogExpires=QDateTime::currentDateTimeUtc().addSecs(60);printf("late=%d\n",b.shouldSuppressDirectedGhostDecode(row("120045","-30","IU8LMC K1ABC 73"),"test"));b.m_lateAutoLogValid=false;
 b.m_dxccLookup=&b.lookup;b.lookup.valid=false;printf("invalid-dxcc=%d\n",b.directedDecodePeerHasInvalidDxcc("K1ABC","IU8LMC K1ABC -10","test",false));b.m_dxCall="K1ABC";printf("active-dxcc=%d\n",b.directedDecodePeerHasInvalidDxcc("K1ABC","IU8LMC K1ABC -10","test",false));b.m_dxCall.clear();b.lookup.valid=true;distanceKm=7000;printf("grid-mismatch=%d\n",b.shouldSuppressDirectedGhostDecode(row("120100","-20","IU8LMC K1ABC FN42"),"test"));distanceKm=0;printf("trusted-grid=%d\n",b.shouldSuppressDirectedGhostDecode(row("120115","-20","IU8LMC K1ABC FN42"),"test"));QVariantMap e;e["isTx"]=true;e["message"]="IU8LMC ZH7RTG -10";printf("tx=%d\n",b.shouldSuppressDirectedGhostDecode(e,"test"));}
