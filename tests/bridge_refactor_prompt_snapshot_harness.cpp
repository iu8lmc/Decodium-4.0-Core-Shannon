#include <QCoreApplication>
#include <QDateTime>
#include <QVariantMap>
#include <QTextStream>
class DecodiumBridge {
public:
 bool m_promptLogSnapshotValid=false,m_pendingAutoLogValid=false,m_lateAutoLogValid=false,m_promptLogCommentOverrideValid=true;
 QString m_promptLogCall,m_promptLogGrid,m_promptLogRptSent,m_promptLogRptRcvd,m_promptLogMode,m_promptLogComment="comment",m_mode="FT8";
 double m_promptLogDialFreq=0,m_frequency=14074000;
 QDateTime m_promptLogOn,m_promptLogOff,m_pendingAutoLogOn,m_pendingAutoLogOff,m_lateAutoLogOn,m_lateAutoLogOff, fallback;
 QDateTime effectiveQsoLogTimeOnUtc(){return fallback;}
 void capturePromptLogSnapshot(const QVariantMap&); void clearPromptLogSnapshot();
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv); QTextStream out(stdout);const auto t=QDateTime::fromString("2020-01-01T00:00:00Z",Qt::ISODate); QVariantMap p{{"call"," K1ABC "},{"grid"," FN42 "},{"sent"," -12 "},{"rcvd"," -08 "},{"mode"," FT4 "},{"freq",7074000}};
 for(int i=0;i<4;++i){DecodiumBridge b;b.fallback=t;b.m_pendingAutoLogValid=i==0;b.m_pendingAutoLogOn=t;b.m_pendingAutoLogOff=t.addSecs(5);b.m_lateAutoLogValid=i==1;b.m_lateAutoLogOn=t.addSecs(10);b.m_lateAutoLogOff=t.addSecs(5);b.capturePromptLogSnapshot(p);out<<b.m_promptLogSnapshotValid<<'|'<<b.m_promptLogCall<<'|'<<b.m_promptLogGrid<<'|'<<b.m_promptLogRptSent<<'|'<<b.m_promptLogRptRcvd<<'|'<<b.m_promptLogMode<<'|'<<b.m_promptLogDialFreq<<'|'<<b.m_promptLogOn.toString(Qt::ISODate)<<'|'<<(b.m_promptLogOff>=b.m_promptLogOn)<<'\n';b.clearPromptLogSnapshot();out<<b.m_promptLogSnapshotValid<<'|'<<b.m_promptLogCall.isEmpty()<<'|'<<b.m_promptLogComment.isEmpty()<<'|'<<b.m_promptLogCommentOverrideValid<<'|'<<b.m_promptLogOn.isValid()<<'|'<<b.m_promptLogDialFreq<<'\n';}
}
