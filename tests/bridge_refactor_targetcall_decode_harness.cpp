#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>
#include <iostream>
static QString normalizedBaseCall(QString s){return s.trimmed().toUpper();}
static QString normalizedUsableCallToken(const QString&s){return s.trimmed().toUpper();}
static void bridgeLog(const QString&){}
class DecodiumBridge:public QObject {public:
 bool m_targetCallActive=true,m_targetCallArmedWaiting=true,m_targetCallSeenSinceLastTx=false,m_transmitting=false,m_targetCallWasTransmitting=false,m_tuning=false,m_txEnabled=true,ghost=false,accepted=true,called=false,ended=false,watchdog=false,requested=false;QString m_targetCallSign="K1ABC",m_dxCall="K1ABC";int m_targetCallRetryCount=0,m_targetCallMaxRetries=3,m_qsoProgress=0,m_currentTx=1,m_targetCallTimeoutS=60,m_targetCallPauseS=0;quint64 m_targetCallSessionId=1;QDateTime m_targetCallLastSeenUtc,m_targetCallLastTxUtc,m_targetCallStartedUtc=QDateTime::currentDateTimeUtc();
 QStringList parseFt8Row(const QString&r)const{return r.split('|');}bool shouldAcceptDecodedMessage(const QString&)const{return accepted;}bool shouldSuppressDirectedGhostDecode(const QStringList&,const QString&)const{return ghost;}void targetCallRetryCountChanged(){}void processDecodeDoubleClick(const QString&,const QString&,const QString&,int){called=true;}void beginTargetCallCalling(){m_targetCallArmedWaiting=false;}void endTargetCallNoQso(const QString&){ended=true;m_targetCallActive=false;}bool txWatchdogLimitConfigured()const{return watchdog;}void setTxEnabled(bool b){m_txEnabled=b;}void armExplicitManualTxRequest(int,const QString&){requested=true;}
 bool targetCallRowsContainTarget(const QStringList&,QString*)const;void observeTargetCallDecodeRows(const QStringList&,const QString&);bool tryArmedWatchSeesTarget(const QStringList&);void onTargetCallTransmittingChanged();void tickTargetCallOnTx();
};
#include "bridge_refactor_targetcall_decode.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);auto check=[](bool ok,const char*name){if(!ok){std::cerr<<name<<" failed\n";std::exit(1);}std::cout<<name<<'\n';};DecodiumBridge b;QString hit;QStringList rows={"1200|-10|0.2|1300|CQ DX K1ABC FN42"};check(b.targetCallRowsContainTarget(rows,&hit)&&hit.contains("K1ABC"),"cq-target");check(!b.targetCallRowsContainTarget({"1200|-10|0.2|1300|K1ABC IU8LMC RRR"},nullptr),"recipient-not-sender");b.ghost=true;check(!b.targetCallRowsContainTarget(rows,nullptr),"ghost-suppressed");b.ghost=false;check(b.tryArmedWatchSeesTarget(rows)&&b.called&&!b.m_targetCallArmedWaiting,"armed-trigger-simulated");b.m_targetCallRetryCount=2;b.observeTargetCallDecodeRows(rows,"test");check(b.m_targetCallSeenSinceLastTx&&b.m_targetCallRetryCount==0,"heard-resets");b.tickTargetCallOnTx();check(b.m_targetCallRetryCount==0&&!b.m_targetCallSeenSinceLastTx,"heard-tick");b.m_targetCallWasTransmitting=true;b.m_transmitting=false;b.onTargetCallTransmittingChanged();check(b.m_targetCallRetryCount==1,"tx-edge");b.m_targetCallRetryCount=2;b.tickTargetCallOnTx();check(b.ended&&!b.m_targetCallActive,"retry-limit");DecodiumBridge c;c.m_targetCallArmedWaiting=false;c.m_targetCallPauseS=1;c.tickTargetCallOnTx();check(!c.m_txEnabled,"pause");QElapsedTimer t;t.start();while(t.elapsed()<1400){app.processEvents();QThread::msleep(5);}check(c.m_txEnabled&&c.requested,"resume-simulated");}
