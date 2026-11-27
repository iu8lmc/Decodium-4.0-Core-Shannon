#include <QCoreApplication>
#include <QDateTime>
#include <QString>
#include <cassert>
#include <iostream>
namespace Radio {QString base_callsign(QString s){return s.section('/',0,0).trimmed().toUpper();}}
namespace decodium::seq {QString signalReportFromMessage(QString s){auto parts=s.split(' ',Qt::SkipEmptyParts);return parts.isEmpty()?QString():parts.last();}QString normalizedBaseCall(QString s){return Radio::base_callsign(s);}bool messageContainsCallToken(QString s,QString call,QString){return s.contains(call,Qt::CaseInsensitive);}}
using decodium::seq::normalizedBaseCall;using decodium::seq::messageContainsCallToken;
QString inferPartnerFromDirectedMessage(QString const& msg,QString const&,QString const&){return msg.startsWith("TO ")?msg.mid(3):QString();}constexpr int kLateAutoLogGraceWindowSeconds=45;void bridgeLog(QString const&){}
struct Seq{QDateTime effectiveQsoLogTimeOnUtc()const{return QDateTime::fromString("2024-05-06T10:34:56Z",Qt::ISODate);}};
class DecodiumBridge{public:
 bool m_pendingAutoLogValid=false,m_lateAutoLogValid=false,m_reportReceivedReal=false;QString m_dxCall,m_autoCqLockedCall,m_callsign="IU8LMC",m_lastTransmittedMessage,m_dxGrid,m_autoCqLockedGrid,m_reportSent,m_reportReceived,m_lastReportTxMessage,m_mode="FT8";
 QString m_pendingAutoLogCall,m_pendingAutoLogGrid,m_pendingAutoLogRptSent,m_pendingAutoLogRptRcvd,m_lateAutoLogCall,m_lateAutoLogGrid,m_lateAutoLogRptSent,m_lateAutoLogRptRcvd;QDateTime m_pendingAutoLogOn,m_pendingAutoLogOff,m_lateAutoLogOn,m_lateAutoLogOff,m_lateAutoLogExpires;double m_pendingAutoLogDialFreq=0,m_lateAutoLogDialFreq=0,m_frequency=14074000;Seq m_seqState;int remembered=0;
 QString tx;QString buildCurrentTxMessage()const{return tx;}QString lookupWorldMapGrid(QString const&)const{return "FN42";}void rememberRecentAutoCqAbandoned(QString const&,double,QString const&){++remembered;}
 QString inferredPartnerForAutolog()const;void capturePendingAutoLogSnapshot();QDateTime effectiveQsoLogTimeOnUtc()const;void clearPendingAutoLogSnapshot();void armLateAutoLogSnapshot();void clearLateAutoLogSnapshot();
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;assert(b.inferredPartnerForAutolog().isEmpty());b.capturePendingAutoLogSnapshot();assert(!b.m_pendingAutoLogValid);
 b.tx="TO K1ABC";assert(b.inferredPartnerForAutolog()=="K1ABC");b.m_dxCall=" K2DEF ";b.m_autoCqLockedCall="K3ABC";b.m_reportSent="-10";b.m_reportReceived="-08";b.m_lastReportTxMessage="K2DEF +05";b.capturePendingAutoLogSnapshot();assert(b.m_pendingAutoLogValid&&b.m_pendingAutoLogCall=="K2DEF"&&b.m_pendingAutoLogGrid=="FN42"&&b.m_pendingAutoLogRptSent=="+05"&&b.m_pendingAutoLogRptRcvd.isEmpty()&&b.m_pendingAutoLogDialFreq==14074000&&b.m_pendingAutoLogOff>=b.m_pendingAutoLogOn);
 b.m_reportReceivedReal=true;b.m_dxGrid=" JM75 ";b.armLateAutoLogSnapshot();assert(b.m_lateAutoLogValid&&b.m_lateAutoLogCall=="K2DEF"&&b.m_lateAutoLogGrid=="JM75"&&b.m_lateAutoLogRptRcvd=="-08"&&b.m_lateAutoLogExpires>QDateTime::currentDateTimeUtc().addSecs(40)&&b.remembered==1);
 b.clearPendingAutoLogSnapshot();b.clearLateAutoLogSnapshot();assert(!b.m_pendingAutoLogValid&&!b.m_lateAutoLogValid&&b.m_pendingAutoLogCall.isEmpty()&&b.m_lateAutoLogCall.isEmpty());std::cout<<"autolog partner inference, reports, grid, snapshots and late grace: OK\n";}
