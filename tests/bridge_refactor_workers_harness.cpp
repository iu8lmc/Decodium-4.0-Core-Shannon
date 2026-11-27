#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>
#include <iostream>
class FakeWorker : public QObject {
 Q_OBJECT
public: void setDecodeEnabled(bool) {}
signals: void decodeReady(); void asyncDecodeReady(); void coherentCount();
};
namespace decodium {
 namespace ft8 {using FT8DecodeWorker=FakeWorker;} namespace ft2 {using FT2DecodeWorker=FakeWorker;}
 namespace ft4 {using FT4DecodeWorker=FakeWorker;} namespace q65 {using Q65DecodeWorker=FakeWorker;}
 namespace msk144 {using MSK144DecodeWorker=FakeWorker;} namespace wspr {using WSPRDecodeWorker=FakeWorker;}
 namespace legacyjt {using LegacyJtDecodeWorker=FakeWorker;} namespace fst4 {using FST4DecodeWorker=FakeWorker;}
}
static QString canonicalApplicationDecodeMode(QString s){return s.trimmed().toUpper();}
static void bridgeLog(const QString&){}
class DecodiumBridge : public QObject {
public:
 bool m_shuttingDown=false;
 QThread *m_workerThread=nullptr,*m_workerThreadFt2=nullptr,*m_workerThreadFt4=nullptr,*m_workerThreadQ65=nullptr,*m_workerThreadMsk=nullptr,*m_workerThreadWspr=nullptr,*m_workerThreadLegacyJt=nullptr,*m_workerThreadFst4=nullptr;
 FakeWorker *m_ft8Worker=nullptr,*m_ft2Worker=nullptr,*m_ft4Worker=nullptr,*m_q65Worker=nullptr,*m_mskWorker=nullptr,*m_wsprWorker=nullptr,*m_legacyJtWorker=nullptr,*m_fst4Worker=nullptr;
 void onFt8DecodeReady(){} void onFt2DecodeReady(){} void onFt2AsyncDecodeReady(){} void onFt4DecodeReady(){} void onQ65DecodeReady(){} void notifyCoherentAveraging(){} void onMsk144DecodeReady(){} void onWsprDecodeReady(){} void onLegacyJtDecodeReady(){} void onFst4DecodeReady(){}
 void ensureDecodeWorkerForMode(const QString&);
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;
 for(const QString& mode:QStringList{"FT8","FT2","FT4","Q65-60A","MSK144","WSPR","JT65","FST4-120"})b.ensureDecodeWorkerForMode(mode);
 if(!b.m_ft8Worker||!b.m_ft2Worker||!b.m_ft4Worker||!b.m_q65Worker||!b.m_mskWorker||!b.m_wsprWorker||!b.m_legacyJtWorker||!b.m_fst4Worker)return 1;
 auto threads=b.findChildren<QThread*>();if(threads.size()!=8)return 2;
 b.ensureDecodeWorkerForMode("JT9");b.ensureDecodeWorkerForMode("FT8");b.ensureDecodeWorkerForMode("UNKNOWN");if(b.findChildren<QThread*>().size()!=8)return 3;
 for(auto* t:threads){t->quit();if(!t->wait(5000))return 4;}
 DecodiumBridge stopped;stopped.m_shuttingDown=true;stopped.ensureDecodeWorkerForMode("FT8");if(stopped.m_ft8Worker)return 5;
 std::cout<<"eight worker modes, shared JT worker, duplicate guard and shutdown: OK\n";
}
#include "workers_harness.moc"
