#include <QCoreApplication>
#include <QDateTime>
#include <QVariantMap>
#include <QtMath>
#include <cmath>
#include <cassert>
#include <iostream>
QString logPromptDateTimeText(QDateTime const& d){return d.toUTC().toString("yyyy-MM-dd HH:mm:ss");}
namespace decodium::seq {QString signalReportFromMessage(QString const& s){return s.startsWith("REPORT ")?s.mid(7):QString();}}
class DecodiumBridge {public:
 bool m_promptLogSnapshotValid=false,m_promptLogCommentOverrideValid=false,m_pendingAutoLogValid=false,m_lateAutoLogValid=false;
 QString m_promptLogCall,m_promptLogGrid,m_promptLogRptSent,m_promptLogRptRcvd,m_logModeOverride,m_promptLogMode,m_promptLogComment;
 QDateTime m_promptLogOn,m_promptLogOff;double m_promptLogDialFreq=0;
 QString m_dxCall,m_dxGrid,m_reportSent,m_reportReceived,m_mode="FT8",m_grid="JM75",m_lastTransmittedMessage;
 QString m_pendingAutoLogCall,m_pendingAutoLogGrid,m_pendingAutoLogRptSent,m_pendingAutoLogRptRcvd;double m_pendingAutoLogDialFreq=0;QDateTime m_pendingAutoLogOn,m_pendingAutoLogOff;
 QString m_lateAutoLogCall,m_lateAutoLogGrid,m_lateAutoLogRptSent,m_lateAutoLogRptRcvd;double m_lateAutoLogDialFreq=0;QDateTime m_lateAutoLogOn,m_lateAutoLogOff,m_lateAutoLogExpires;double m_frequency=14074000;int m_specialOperationActivity=0;QVariantMap settings;
 QVariant getSetting(QString const& k,QVariant const& d)const{return settings.value(k,d);}double calcDistance(QString const&,QString const&)const{return 100;}
 QDateTime effectiveQsoLogTimeOnUtc()const{return QDateTime::fromString("2024-05-06T10:34:56Z",Qt::ISODate);}QString buildCurrentTxMessage()const{return "REPORT -07";}
 QVariantMap pendingLogQsoPreview()const;QString defaultLogCommentForQso(QString const&,QString const&,QString const&,QString const&)const;
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;
 b.m_dxCall=" K1ABC ";b.m_dxGrid=" FN42 ";b.m_reportReceived=" -12 ";auto live=b.pendingLogQsoPreview();assert(live["call"]=="K1ABC"&&live["sent"]=="-07"&&live["rcvd"]=="-12"&&live["mode"]=="FT8");
 b.m_pendingAutoLogValid=true;b.m_pendingAutoLogCall="K2DEF";b.m_pendingAutoLogDialFreq=7074000;b.m_pendingAutoLogOff=QDateTime::fromString("2024-05-06T10:30:00Z",Qt::ISODate);auto pending=b.pendingLogQsoPreview();assert(pending["call"]=="K2DEF"&&pending["freq"].toDouble()==7074000&&pending["timeOff"]==pending["timeOn"]);
 b.m_promptLogSnapshotValid=true;b.m_promptLogCall="K3ABC";b.m_promptLogMode="FT4";b.m_logModeOverride="CW";b.m_promptLogOn=b.effectiveQsoLogTimeOnUtc();b.m_promptLogCommentOverrideValid=true;b.m_promptLogComment="custom";auto snapshot=b.pendingLogQsoPreview();assert(snapshot["call"]=="K3ABC"&&snapshot["mode"]=="CW"&&snapshot["comment"]=="custom"&&snapshot["timeOff"]==snapshot["timeOn"]);
 b.settings["dBReportsToComments"]=true;b.settings["Miles"]=true;assert(b.defaultLogCommentForQso(" ft8 ","-05","-10","FN42")=="FT8 Sent: -05 Rcvd: -10 Distance: 62 miles");b.settings["dBReportsToComments"]=false;b.settings["SpecOpInComments"]=true;b.m_specialOperationActivity=3;assert(b.defaultLogCommentForQso("FT8","","","")=="ARRL Field Day");b.settings["Individual_Contest_Name"]=true;b.settings["Contest_Name"]=" Custom ";assert(b.defaultLogCommentForQso("FT8","","","")=="Custom Contest");b.settings["SpecOpInComments"]=false;b.settings["LogComments"]=" hello ";assert(b.defaultLogCommentForQso("FT8","","","")=="hello");
 std::cout<<"QSO live/pending/snapshot preview, reports, dates and comment policies: OK\n";}
