#include <QtCore>
#include <cstdio>
struct Encoding{bool ok;};
namespace decodium::txmsg {
#define ENCODER(n) Encoding n(const QString& m,bool=true){return {m=="GOOD"};}
 ENCODER(encodeFt2) ENCODER(encodeFt4) ENCODER(encodeFt8) ENCODER(encodeJt65) ENCODER(encodeJt4) ENCODER(encodeJt9) ENCODER(encodeWspr) ENCODER(encodeMsk144) ENCODER(encodeQ65)
 Encoding encodeFst4WithHint(const QString&m,bool,bool){return {m=="GOOD"};}
#undef ENCODER
}
static QString customCqPrefixFromMessage(const QString&m,const QString&){return m.startsWith("CQ ")?"CQ":"";}
static QString standardFtxCqMessage(const QString&call,const QString&grid,const QString&){return "CQ "+call+" "+grid;}
static bool bridgeGeneratedTxWaveMode(const QString&m){return m!="OTHER";}
struct Legacy{int n=0;QString text;void setTxMessage(int i,const QString&v){n=i;text=v;}};
class DecodiumBridge {public:
 QString m_tx1,m_tx2,m_tx3,m_tx4,m_tx5,m_tx6,m_customCqPrefix,m_customCqMessage,m_callsign="9H1SR",m_grid="JM75AA",m_mode="FT8";bool m_skipCustomCqPrefixRecovery=false,legacyEnabled=true;Legacy l,*m_legacyBackend=&l;int signalCount=0,invalidations=0,precomputes=0,regenerated=0;
 bool usingLegacyBackendForTx()const{return legacyEnabled;}void invalidateTxAudioCache(){++invalidations;}void scheduleTxAudioPrecompute(){++precomputes;}void regenerateTxMessages(){++regenerated;}
#define EMIT_COUNTER(n) void n(){++signalCount;}
 EMIT_COUNTER(tx1Changed) EMIT_COUNTER(tx2Changed) EMIT_COUNTER(tx3Changed) EMIT_COUNTER(tx4Changed) EMIT_COUNTER(tx5Changed) EMIT_COUNTER(tx6Changed) EMIT_COUNTER(txMessagesChanged) EMIT_COUNTER(currentTxMessageChanged)
#undef EMIT_COUNTER
#define PAIR(n) QString tx##n()const;void setTx##n(const QString&);
 PAIR(1) PAIR(2) PAIR(3) PAIR(4) PAIR(5) PAIR(6)
#undef PAIR
 QString txMessage(int)const;void setTxMessage(int,const QString&);QString validateTxMessage(const QString&)const;void resetStandardTxMessages();
};
#include "bridge_refactor_tx_messages.inc"
static void check(bool ok,const char* label){if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}puts(label);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;
 for(int i=1;i<=5;++i){b.setTxMessage(i," hello ");check(b.txMessage(i)=="HELLO"&&b.l.n==i&&b.l.text=="HELLO","set-and-legacy");}check(b.signalCount==15&&b.invalidations==5&&b.precomputes==5,"notifications");
 b.setTxMessage(1,"hello");check(b.signalCount==15&&b.invalidations==5,"unchanged");b.setTxMessage(-2,"low");check(b.txMessage(-1)=="LOW","low-bound");
 b.setTxMessage(99," cq 9h1sr jm75 ");check(b.txMessage(99)=="CQ 9H1SR JM75"&&b.m_customCqPrefix=="CQ"&&b.m_customCqMessage.isEmpty(),"standard-cq");
 b.setTxMessage(6,"CQ TEST 9H1SR");check(b.m_customCqMessage=="CQ TEST 9H1SR","custom-cq");b.resetStandardTxMessages();check(b.m_customCqPrefix.isEmpty()&&b.m_customCqMessage.isEmpty()&&b.m_skipCustomCqPrefixRecovery&&b.regenerated==1,"reset");
 check(!b.validateTxMessage(" ").isEmpty(),"empty-rejected");for(const QString& mode:{"FT2","FT4","FT8","JT65","JT4","JT9","WSPR","MSK144","Q65","FST4","FST4W"}){b.m_mode=mode;check(b.validateTxMessage(" good ").isEmpty()&&!b.validateTxMessage("bad").isEmpty(),"validation-mode");}b.m_mode="OTHER";check(b.validateTxMessage("bad").isEmpty(),"unhandled-mode");
 b.legacyEnabled=false;int previous=b.l.n;b.setTxMessage(2,"another");check(b.l.n==previous&&b.tx2()=="ANOTHER","no-legacy");
}
