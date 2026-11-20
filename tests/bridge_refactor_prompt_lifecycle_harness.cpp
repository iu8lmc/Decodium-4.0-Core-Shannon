#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QObject>
#include <QTimer>
#include <QTextStream>
#include <QVariantMap>
static void bridgeLog(const QString&) {}
class DecodiumBridge: public QObject {
public:
 bool m_qsoLogged=false,m_promptLogSnapshotValid=false,m_logPromptOpen=false,m_logPromptShownByQml=false,m_mainQmlReady=true,m_transmitting=false,m_tuning=false,m_pendingAutoLogValid=false,m_lateAutoLogValid=false,promptEnabled=true;
 QString m_promptLogCall,m_dxCall="CURRENT",m_pendingAutoLogCall,m_lateAutoLogCall,m_logModeOverride="RTTY",skipped,status; QVariantMap preview; int captured=0, dialogs=0,requests=0,commits=0,deferred=0,clears=0;
 QVariant getSetting(const QString&,bool)const{return promptEnabled;}
 QVariantMap pendingLogQsoPreview()const{return preview;}
 void capturePromptLogSnapshot(const QVariantMap&p){++captured;m_promptLogSnapshotValid=true;m_promptLogCall=p.value("call").toString();}
 void showLogQsoPromptDialog(){++dialogs;}
 void statusMessage(const QString&s){status=s;}
 void logQsoPromptRequested(){++requests;}
 void logQsoNow(){++commits;}
 void applyDeferredSpecialOperationActivityIfReady(){++deferred;}
 void clearNextLogClusterSpotOverride(){++clears;}
 void clearPromptLogSnapshot(){m_promptLogSnapshotValid=false;++clears;}
 void clearPendingAutoLogSnapshot(){m_pendingAutoLogValid=false;++clears;}
 void clearLateAutoLogSnapshot(){m_lateAutoLogValid=false;++clears;}
 void clearTxArmedAfterCompletedQso(const QString&s,const QString&){skipped=s;}
 bool promptToLogEnabled()const;void promptLogQso();void notifyLogPromptShown();void requestManualLogQso();void logQso();void confirmLogQso();void rejectPromptedLogQso();
};
#include "bridge_refactor_under_test.inc"
static void wait(int ms){QEventLoop e;QTimer::singleShot(ms,&e,&QEventLoop::quit);e.exec();}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTextStream o(stdout);
 {DecodiumBridge b;b.requestManualLogQso();o<<"empty|"<<b.status<<'|'<<b.requests<<'\n';}
 {DecodiumBridge b;b.m_transmitting=true;b.requestManualLogQso();o<<"tx|"<<b.status<<'|'<<b.requests<<'\n';}
 {DecodiumBridge b;b.preview["call"]=" K1ABC ";b.logQso();wait(10);b.notifyLogPromptShown();wait(520);b.rejectPromptedLogQso();o<<"reject|"<<b.requests<<'|'<<b.captured<<'|'<<b.dialogs<<'|'<<b.skipped<<'|'<<b.clears<<'|'<<b.m_qsoLogged<<'|'<<b.commits<<'\n';}
 {DecodiumBridge b;b.preview["call"]="K2ABC";b.logQso();wait(530);o<<"fallback|"<<b.requests<<'|'<<b.dialogs<<'\n';b.confirmLogQso();wait(10);o<<"accept|"<<b.commits<<'|'<<b.deferred<<'|'<<b.m_logPromptOpen<<'\n';}
 {DecodiumBridge b;b.m_mainQmlReady=false;b.preview["call"]="K3ABC";b.promptLogQso();wait(10);o<<"native|"<<b.dialogs<<'|'<<b.requests<<'\n';}
 {DecodiumBridge b;b.promptEnabled=false;b.logQso();o<<"direct|"<<b.commits<<'|'<<b.requests<<'\n';}
 {DecodiumBridge b;b.m_logPromptOpen=true;b.m_pendingAutoLogValid=true;b.m_pendingAutoLogCall=" K4ABC ";b.rejectPromptedLogQso();o<<"pending|"<<b.skipped<<'|'<<b.m_qsoLogged<<'\n';}
 {DecodiumBridge b;b.m_logPromptOpen=true;b.m_lateAutoLogValid=true;b.m_lateAutoLogCall=" K5ABC ";b.rejectPromptedLogQso();o<<"late|"<<b.skipped<<'|'<<b.m_qsoLogged<<'\n';}
 {DecodiumBridge b;b.m_qsoLogged=true;b.preview["call"]="K6ABC";b.promptLogQso();o<<"already|"<<b.requests<<'|'<<b.captured<<'\n';}
}
