#include <QCoreApplication>
#include <QDateTime>
#include <QSet>
#include <QTextStream>
#include <QVariantMap>
#include <algorithm>
namespace decodium { namespace decode_ui { struct WorkedFilterOptions {}; }}
static void bridgeLog(const QString&){}
static QString decodeRxClearEntryKey(const QVariantMap&e){return e.value("key").toString();}
static void coalesceRxPaneTxRows(QVariantList&,const QString&){}
static qint64 decodeDisplaySortMsecs(const QVariantMap&e,const QDateTime&){return e.value("timestamp").toLongLong();}
class DecodiumBridge {public:bool m_decodeShowPeriodSeparator=true,m_decodeNewestFirst=false,showTx=true;QString m_mode="FT8";QSet<QString> m_clearedRxDecodeKeys;decodium::decode_ui::WorkedFilterOptions workedFilterOptions()const{return {};}
 bool shouldDisplayEntryForBandActivity(const QVariantMap&e,const decodium::decode_ui::WorkedFilterOptions&)const{return !e.value("hidden").toBool();}
 bool shouldMirrorToRxPane(const QVariantMap&e,bool)const{return e.value("current").toBool();}
 QVariant getSetting(const QString&,bool)const{return showTx;}
 bool entryBelongsToCurrentQso(const QVariantMap&,bool=false)const;void injectPeriodSeparators(QVariantList&)const;QVariantList filterEntriesForBandActivity(const QVariantList&)const;QVariantList filterEntriesForRxDecode(const QVariantList&)const;};
#include "bridge_refactor_under_test.inc"
static QString show(const QVariantList&rows){QStringList r;for(auto v:rows){auto m=v.toMap();r<< (m.value("isSeparator").toBool()?QStringLiteral("SEP:")+m.value("time").toString():m.value("key").toString());}return r.join(",");}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);DecodiumBridge b;QVariantList rows{QVariantMap{{"key","A"},{"time","120000"},{"timestamp",1000},{"current",true}},QVariantMap{{"key","B"},{"time","120015"},{"timestamp",16000},{"current",true}},QVariantMap{{"key","TX"},{"time","120015"},{"timestamp",17000},{"isTx",true},{"current",true}},QVariantMap{{"key","H"},{"hidden",true}},QVariantMap{{"key","C"},{"time","120030"},{"timestamp",31000},{"forceRxPane",true}}};
 o<<"band|"<<show(b.filterEntriesForBandActivity(rows))<<'\n';b.m_decodeNewestFirst=true;o<<"reverse|"<<show(b.filterEntriesForBandActivity(rows))<<'\n';b.m_decodeNewestFirst=false;b.showTx=false;b.m_clearedRxDecodeKeys.insert("B");o<<"rx|"<<show(b.filterEntriesForRxDecode(rows))<<'\n';b.m_decodeShowPeriodSeparator=false;o<<"nosep|"<<show(b.filterEntriesForBandActivity(rows))<<'\n';b.m_decodeShowPeriodSeparator=true;QVariantList noTime{QVariantMap{{"key","X"},{"timestamp",1000}},QVariantMap{{"key","Y"},{"timestamp",3000}}};b.injectPeriodSeparators(noTime);o<<"timestamp|"<<show(noTime)<<'\n';o<<"empty|"<<b.entryBelongsToCurrentQso({},false)<<'\n';}
