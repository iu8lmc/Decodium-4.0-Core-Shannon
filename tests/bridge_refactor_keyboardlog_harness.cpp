#include <QtCore>
void bridgeLog(const QString&){}
class DecodiumBridge {public:QString m_logModeOverride,m_promptLogComment,call,grid,sent,received,lastMode;bool m_promptLogCommentOverrideValid=false,m_qsoLogged=true;int logged=0;
 void setDxCall(const QString& v){call=v;}void setDxGrid(const QString& v){grid=v;}void setReportSent(const QString& v){sent=v;}void setReportReceived(const QString& v){received=v;}void logQso(){++logged;lastMode=m_logModeOverride;}
 bool registraQsoRtty(const QString&,const QString&,const QString&,const QString&,const QString&,const QString&);bool registraQsoCw(const QString&,const QString&,const QString&,const QString&,const QString&,const QString&);};
#include "bridge_refactor_under_test.inc"
int main(){DecodiumBridge b;QTextStream o(stdout);bool missing=b.registraQsoCw(" ","","","","","");o<<"empty|"<<missing<<'|'<<b.m_logModeOverride.isEmpty()<<'|'<<b.logged<<'\n';bool a=b.registraQsoRtty(" k1abc ","",""," Name "," Town "," fn42 ");o<<"rtty|"<<a<<'|'<<b.call<<'|'<<b.grid<<'|'<<b.sent<<'|'<<b.received<<'|'<<b.m_promptLogComment<<'|'<<b.m_promptLogCommentOverrideValid<<'|'<<b.m_qsoLogged<<'|'<<b.logged<<'\n';b.m_qsoLogged=true;bool c=b.registraQsoCw("k2xyz","579","589","","","");o<<"cw|"<<c<<'|'<<b.call<<'|'<<b.sent<<'|'<<b.received<<'|'<<b.lastMode<<'|'<<b.m_qsoLogged<<'|'<<b.logged<<'\n';}
