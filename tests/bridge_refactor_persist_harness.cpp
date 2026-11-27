#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QVariantMap>
#include <iostream>
#define DECODIUM_HAS_SSTV 1
namespace decodium {namespace telemetry {struct StationTelemetryFields{QString grid4;int tempC=0,windSpeedKmh=0,windDirDeg16=0,sky=0,powerWatts=0,radioModelId=0,antennaTypeId=0;};constexpr int kTempUnknown=-999;bool decodeStationTelemetryHex(const QString&,StationTelemetryFields&){return false;}QString windDirIndex16ToLabel(int){return {};}QStringList stationSkyConditionNames(){return {};}QStringList stationRadioModelNames(){return {};}QStringList stationAntennaTypeNames(){return {};}}}
static bool isStreamingKeyboardMode(const QString&m){return m=="RTTY"||m=="JTTY";}static QString decodeMirrorEntryKey(const QVariantMap&e){return e.value("message").toString();}static bool messageIsUnresolvedGridOnlyDecode(const QString&m,QString*){return m.contains("<...>");}static bool isFst4wUnresolvedHashDecode(const QString&,const QString&){return false;}
class FakeWorker:public QObject{Q_OBJECT public:QList<QVariantMap>received;public slots:void enqueueDecode(const QVariantMap&m){received.append(m);}};
struct Band{QString currentBandLambda()const{return "20m";}};
class DecodiumBridge:public QObject{public:
 bool m_sstvWorkspaceActive=false;QString m_mode="FT8",m_lastLoggedQsoCall; qint64 m_lastLoggedQsoTimestampMs=0,m_currentSessionId=1;double m_frequency=14074000;FakeWorker*m_persistenceWorker=nullptr;Band*m_bandManager=nullptr;QSet<QString>m_decodeHistoryPersistedKeys;QVariantList m_decodeList;int committed=0;
 QVariant getSetting(const QString&,bool b)const{return b;}void stationTelemetryDecoded(const QVariantMap&){}void rememberDecodeDedupEntry(const QVariantMap&){}void trimDecodeListsIfNeeded(){}void noteDecodeCommitted(){++committed;}
 void appendDecodeMapToList(const QVariantMap&);bool persistDecodeHistoryEntry(const QVariantMap&);void enqueuePersistDecode(const QVariantMap&);
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;FakeWorker worker;Band band;b.m_persistenceWorker=&worker;b.m_bandManager=&band;
 QVariantMap rx{{"mode","FT8"},{"message","CQ K1ABC FN42"},{"fromCall","K1ABC"}};b.appendDecodeMapToList(rx);b.appendDecodeMapToList(rx);QCoreApplication::processEvents();if(b.m_decodeList.size()!=2||worker.received.size()!=1||worker.received[0].value("dialFreqHz").toLongLong()!=14074000||worker.received[0].value("bandLabel").toString()!="20m")return 1;
 QVariantMap tx=rx;tx["message"]="TX";tx["isTx"]=true;if(b.persistDecodeHistoryEntry(tx))return 2;QVariantMap partial=rx;partial["message"]="CQ <...> FN42";partial["dxCallsign"]="K1ABC";if(!b.persistDecodeHistoryEntry(partial))return 3;QCoreApplication::processEvents();if(worker.received.size()!=2||!worker.received[1].value("unresolvedHash").toBool()||!worker.received[1].value("dxCallsign").toString().isEmpty())return 4;
 b.m_sstvWorkspaceActive=true;b.appendDecodeMapToList(rx);if(b.m_decodeList.size()!=2)return 5;b.m_sstvWorkspaceActive=false;b.m_mode="RTTY";b.appendDecodeMapToList(rx);if(b.m_decodeList.size()!=2)return 6;
 std::cout<<"decode append, SSTV/keyboard gates, dedup, TX exclusion and async redaction: OK\n";
}
#include "persist_harness.moc"
