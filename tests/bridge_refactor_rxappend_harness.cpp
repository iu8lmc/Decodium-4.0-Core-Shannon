#include <QtCore>
struct MainThreadTraceScope {MainThreadTraceScope(const QString&,const QString&,int){}void addDetail(const QString&) {}};
QString decodeRxClearEntryKey(const QVariantMap& e){return e.value("clear").toString();}QString decodeMirrorEntryKey(const QVariantMap& e){return e.value("key").toString();}
class DecodiumBridge {public:bool m_sstvWorkspaceActive=false,legacy=false,mirror=true;QString m_mode="FT8";QVariantList m_rxDecodeList;QSet<QString>m_clearedRxDecodeKeys,m_rxDecodeMirrorKeys;int rotor=0,emits=0;
 bool usingLegacyBackendForTx()const{return legacy;}bool shouldMirrorToRxPane(const QVariantMap&)const{return mirror;}void rebuildRxDecodeMirrorIndex(){for(auto e:m_rxDecodeList)m_rxDecodeMirrorKeys.insert(decodeMirrorEntryKey(e.toMap()));}void trimDecodeListsIfNeeded(){}void emitRxDecodeListChangedThrottled(bool){++emits;}void rotorDecodeHeard(const QString&,int,bool,const QString&){++rotor;}void appendRxDecodeEntry(const QVariantMap&);};
#define DECODIUM_HAS_SSTV 1
#define emit
#include "bridge_refactor_under_test.inc"
int main(){DecodiumBridge b;QTextStream o(stdout);QVariantMap e{{"key","a"},{"clear","x"},{"message","CQ K1ABC FN42"},{"snr","-12"},{"isTx",false}};b.appendRxDecodeEntry(e);b.appendRxDecodeEntry(e);o<<"dedup|"<<b.m_rxDecodeList.size()<<'|'<<b.rotor<<'|'<<b.emits<<'|'<<b.m_rxDecodeList[0].toMap().value("forceRxPane").toBool()<<'\n';b.legacy=true;e["key"]="b";b.appendRxDecodeEntry(e);e["isTx"]=true;b.appendRxDecodeEntry(e);o<<"legacy|"<<b.m_rxDecodeList.size()<<'|'<<b.rotor<<'|'<<b.emits<<'\n';b.m_sstvWorkspaceActive=true;e["key"]="c";b.appendRxDecodeEntry(e);b.m_sstvWorkspaceActive=false;b.m_clearedRxDecodeKeys.insert("x");b.appendRxDecodeEntry(e);o<<"guards|"<<b.m_rxDecodeList.size()<<'\n';}
