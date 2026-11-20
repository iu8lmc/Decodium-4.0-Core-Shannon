#include <QCoreApplication>
#include <QDateTime>
#include <QMap>
#include <QSet>
#include <QVariantMap>
#include <iostream>
struct ParsedAdifRecord{QMap<QString,QString>fields;};
static QString freqHzToBandToken(quint64 f){return f>10000000?"20m":"40m";}
static QString normalizedBaseCall(QString c){return c.trimmed().toUpper().section('/',0,0);}
static QString normalizedAdifDateToken(QString d){return d.trimmed();}
static QString normalizeAdifModeForDisplay(const QMap<QString,QString>&f){return f.value("MODE").toUpper();}
struct DxccEntity{QString name="Testland",continent="EU";int cqZone=15,ituZone=28;bool isValid()const{return true;}};struct DxccLookup{bool isLoaded()const{return true;}DxccEntity lookup(const QString&)const{return {};}};
struct Link{int port=0;bool enabled=false;QString call;void setIdentity(const QString&,const QString&c){call=c;}void setPort(quint16 p){port=p;}void setEnabled(bool b){enabled=b;}};
class DecodiumBridge:public QObject{public:
 struct WorkedSets {QSet<QString>dxccEver,dxccByBand,continentEver,continentByBand,cqZoneByBand,ituZoneByBand,gridEver,gridByBand,callByBand,callByBandMode,callToday,callTodayByBand,callYesterday,callYesterdayByBand,callEver;QSet<int>cqZoneEver,ituZoneEver;void clear(){*this={};}}m_worked;
 struct DecoLogRow{QString call,grid,band,mode,date;};QList<DecoLogRow>m_decoLogRows;QSet<QString>m_workedCalls;DxccLookup*m_dxccLookup=nullptr;Link*m_decoLogLink=nullptr;bool m_decoLogLinkEnabled=false,m_decoLogConnected=false;int m_decoLogLinkPort=2237,events=0;QString m_callsign="9h1sr",m_decoLogStatus;QVariantList m_decodeList,m_rxDecodeList;
 QString version()const{return "1.0.737";}void decoLogStateChanged(){++events;}void decoLogLinkChanged(){++events;}void invalidateDecodeUiPredicateCaches(){}void rebuildBandActivityModel(){}void rebuildRxDecodeModel(){}void enrichDecodeEntry(QVariantMap&e,bool){e["refreshed"]=true;}
 void appendWorkedQso(const QString&,const QString&,quint64,const QString&,const QString&);void appendWorkedQsoBand(const QString&,const QString&,const QString&,const QString&,const QString&);void applyDecoLogRows();void configureDecoLogLink();void setDecoLogLinkEnabled(bool);void setDecoLogLinkPort(int);void refreshWorkedBeforeDecodeEntriesForCall(const QString&);void rebuildWorkedSetsFromAdifRecords(const QList<ParsedAdifRecord>&);
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;DxccLookup dx;Link link;b.m_dxccLookup=&dx;b.m_decoLogLink=&link;QString today=QDateTime::currentDateTimeUtc().date().toString("yyyyMMdd"),yesterday=QDateTime::currentDateTimeUtc().date().addDays(-1).toString("yyyyMMdd");
 b.appendWorkedQso("k1abc/p","FN42xx",14074000,"ft8",today);if(!b.m_worked.callToday.contains("K1ABC")||!b.m_worked.callByBandMode.contains("20m|FT8|K1ABC/P")||!b.m_worked.dxccEver.contains("Testland")||!b.m_worked.gridByBand.contains("20m|FN42"))return 1;
 b.appendWorkedQsoBand("G1ABC","IO91","40m","FT4",yesterday);if(!b.m_worked.callYesterdayByBand.contains("40m|G1ABC"))return 2;
 ParsedAdifRecord r;r.fields={{"CALL","W1ABC/P"},{"FREQ","14.074"},{"MODE","ft8"},{"GRIDSQUARE","FN31"},{"QSO_DATE",today}};b.m_decoLogRows.append({"IU8LMC","JM75","20m","FT8",today});b.rebuildWorkedSetsFromAdifRecords({r});if(!b.m_worked.callToday.contains("W1ABC")||!b.m_worked.callByBandMode.contains("20m|FT8|W1ABC/P")||!b.m_workedCalls.contains("IU8LMC"))return 3;
 b.m_decodeList={QVariantMap{{"dxCallsign","W1ABC/P"}},QVariantMap{{"fromCall","G1ABC"}}};b.refreshWorkedBeforeDecodeEntriesForCall("W1ABC");if(!b.m_decodeList[0].toMap().value("refreshed").toBool()||b.m_decodeList[1].toMap().value("refreshed").toBool())return 4;
 b.setDecoLogLinkPort(999999);b.setDecoLogLinkEnabled(true);if(link.port!=65535||!link.enabled||link.call!="9H1SR")return 5;b.setDecoLogLinkEnabled(false);if(link.enabled)return 6;
 std::cout<<"worked base calls, date/band/mode/grid/DXCC, ADIF rebuild, targeted refresh and log-link settings: OK\n";
}
