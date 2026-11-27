#include <QtCore>
#include <cstdio>
static QString decodeDedupKey(const QString& a,const QString& b,const QString& c){return a+"|"+b+"|"+c.trimmed().toUpper();}
static QString decodeDedupKeyFt2Async(const QString& a,const QString& b,const QString& c,qint64 t){return decodeDedupKey(a,b,c)+"|"+QString::number(t);}
static QString decodeMirrorEntryKey(const QVariantMap& e){return e.value("message").toString();}
class DecodiumBridge { public:
 QSet<QString> m_nativeDecodeDedupKeys,m_ft2AsyncDecodeDedupKeys,m_rxDecodeMirrorKeys;
 QQueue<QString> m_nativeDecodeDedupOrder,m_ft2AsyncDecodeDedupOrder;
 quint64 m_nativeDecodeDedupSessionId=0,m_ft2AsyncDecodeDedupSessionId=0,m_decodeSessionId=3;
 QVariantList m_decodeList,m_rxDecodeList;QString m_mode="FT8";
 void resetNativeDecodeDedupIndex();void rememberNativeDecodeDedupKey(const QString&);
 void resetFt2AsyncDecodeDedupIndex();void rememberFt2AsyncDecodeDedupKey(const QString&);
 void rememberDecodeDedupEntry(const QVariantMap&);void ensureNativeDecodeDedupIndex();
 void rebuildRxDecodeMirrorIndex();
};
#include "bridge_refactor_decode_dedup.inc"
static void check(bool ok,const char* label){if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}puts(label);}
int main(int argc,char** argv){QCoreApplication app(argc,argv);DecodiumBridge b;
 b.rememberNativeDecodeDedupKey("");check(b.m_nativeDecodeDedupKeys.isEmpty(),"ignore-empty");
 for(int i=0;i<770;++i)b.rememberNativeDecodeDedupKey(QString::number(i));check(b.m_nativeDecodeDedupKeys.size()==768&&!b.m_nativeDecodeDedupKeys.contains("0")&&b.m_nativeDecodeDedupKeys.contains("769"),"native-capacity");
 b.rememberNativeDecodeDedupKey("769");check(b.m_nativeDecodeDedupOrder.size()==768,"native-duplicate");
 for(int i=0;i<770;++i)b.rememberFt2AsyncDecodeDedupKey(QString::number(i));check(b.m_ft2AsyncDecodeDedupKeys.size()==768&&!b.m_ft2AsyncDecodeDedupKeys.contains("0"),"ft2-capacity");
 b.resetNativeDecodeDedupIndex();b.resetFt2AsyncDecodeDedupIndex();check(b.m_nativeDecodeDedupSessionId==3&&b.m_ft2AsyncDecodeDedupSessionId==3&&b.m_nativeDecodeDedupKeys.isEmpty(),"reset-session");
 QVariantMap e{{"time","120000"},{"freq","1200"},{"message","cq 9h1sr"},{"mode","FT2"},{"timestamp",123},{"decodeSessionId",3}};
 b.rememberDecodeDedupEntry(e);check(b.m_nativeDecodeDedupKeys.size()==1&&b.m_ft2AsyncDecodeDedupKeys.size()==1,"ft2-entry");b.rememberDecodeDedupEntry(e);check(b.m_nativeDecodeDedupKeys.size()==1&&b.m_ft2AsyncDecodeDedupKeys.size()==1,"entry-duplicate");
 e["isTx"]=true;b.rememberDecodeDedupEntry(e);e["isTx"]=false;e["decodeSessionId"]=2;b.rememberDecodeDedupEntry(e);e["decodeSessionId"]=3;e["message"]=" ";b.rememberDecodeDedupEntry(e);check(b.m_nativeDecodeDedupKeys.size()==1,"skip-tx-old-empty");
 b.m_decodeSessionId=4;e["decodeSessionId"]=4;e["message"]="new";e["mode"]="FT8";b.rememberDecodeDedupEntry(e);check(b.m_nativeDecodeDedupSessionId==4&&b.m_nativeDecodeDedupKeys.size()==1&&b.m_ft2AsyncDecodeDedupSessionId==3,"session-reset-non-ft2");
 b.m_decodeList={e,QVariantMap{{"decodeSessionId",3},{"message","old"}},QVariantMap{{"decodeSessionId",4},{"message","tx"},{"isTx",true}}};b.m_nativeDecodeDedupSessionId=0;b.ensureNativeDecodeDedupIndex();check(b.m_nativeDecodeDedupKeys.size()==1&&b.m_nativeDecodeDedupSessionId==4,"rebuild-index");
 b.m_rxDecodeList={QVariantMap{{"message","ONE"}},QVariantMap{{"message","ONE"}},QVariantMap{{"message","TWO"}},QVariantMap{{"message",""}}};b.rebuildRxDecodeMirrorIndex();check(b.m_rxDecodeMirrorKeys.size()==2,"mirror-rebuild");
}
