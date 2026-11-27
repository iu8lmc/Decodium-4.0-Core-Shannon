#include "AdifDocumentInternal.h"
#include <QCoreApplication>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>
#include <algorithm>
using namespace decodium::adif_document;
static void bridgeLog(const QString&){}static void rebuildWorkedCallsFromDocument(QSet<QString>&out,const QList<ParsedAdifRecord>&r){out.clear();for(auto x:r)out.insert(x.fields.value("CALL"));}namespace Radio{QString base_callsign(QString s){return s.section('/',0,0);}}class MapService{public:int reloads=0;void reloadFromAdif(const QString&){++reloads;}};
class DecodiumBridge{public:bool m_shuttingDown=false,m_qsoLogged=false;QString m_dxCall="K1ABC",m_promptLogCall,m_pendingAutoLogCall,m_lateAutoLogCall,m_mode="FT8",error,path;int m_qsoCountCache=0,events=0;QSet<QString>m_workedCalls;MapService*m_mapIntelligenceService=nullptr;QString ensureAdifLogPath(){return path;}void rebuildWorkedSetsFromAdifRecords(const QList<ParsedAdifRecord>&){}void invalidateQsoSearchCache(){}void warmLogCacheAsync(){}void qsoCountChanged(){++events;}void workedCountChanged(){++events;}void qsoLogCacheChanged(){++events;}void errorMessage(QString s){error=s;}void mirrorLegacyLoggedAdif(const QByteArray&);};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);QTemporaryDir temp;if(!temp.isValid())return 2;DecodiumBridge b;MapService m;b.path=temp.filePath("isolated.adi");b.m_mapIntelligenceService=&m;QByteArray input="<CALL:5>K1ABC <MODE:3>FT8 <QSO_DATE:8>20261107 <TIME_ON:6>120000 <BAND:3>20m <EOR>\n<CALL:5>K2ABC <MODE:3>FT4 <QSO_DATE:8>20261107 <TIME_ON:6>120100 <BAND:3>20m <EOR>\n";b.mirrorLegacyLoggedAdif(input);auto doc=loadAdifDocument(b.path);o<<"import|"<<doc.records.size()<<'|'<<doc.records.value(0).fields.value("CALL")<<'|'<<doc.records.value(1).fields.value("MODE")<<'|'<<b.m_qsoLogged<<'|'<<b.events<<'|'<<m.reloads<<'\n';b.mirrorLegacyLoggedAdif(input);o<<"duplicate|"<<loadAdifDocument(b.path).records.size()<<'|'<<b.events<<'\n';b.path=temp.path();b.mirrorLegacyLoggedAdif("<CALL:5>K3ABC <MODE:3>FT8 <EOR>");o<<"failure|"<<!b.error.isEmpty()<<'|'<<b.events<<'\n';}
