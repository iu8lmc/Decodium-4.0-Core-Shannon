#include <QDateTime>
#include <QStringList>
#include <QHash>
#include <iostream>
#include "DecodeMessageNormalization.h"
#include "DecodeValidationRules.h"
#include "Sequencer/MessageTokenRules.hpp"
using namespace decodium::decode_message;using namespace decodium::decode_validation;using namespace decodium::seq;
void bridgeLog(QString const&){}
class DecodiumBridge {public:QString m_mode="FT8",m_lastTransmittedMessage;bool m_transmitting=false,m_tuning=false;qint64 m_asyncLastTxEndMs=0;QDateTime m_lastTxActivityUtc;QHash<QString,qint64>m_recentDirectedReportDecodeMs;QHash<QString,QString>m_recentDirectedReportDecodeMessage;int periodMsForMode(QString const&m)const{return m=="FT2"?3750:15000;}bool shouldSuppressConflictingDirectedReportDecode(QStringList const&,QString const&,QString*);bool shouldSuppressRecentLocalTxEchoDecode(QString const&,QString const&,QString*)const;};
#include "method.inc"
#define check(x) do {if(!(x)){std::cerr<<"check failed: "<<#x<<"\n";abort();}} while(false)
int main(){DecodiumBridge b;QString reason;auto row=[](QString m){return QStringList{"1200","-10","0","1500",m};};check(!b.shouldSuppressConflictingDirectedReportDecode(row("IU8LMC K1ABC -10"),"test",&reason));check(!b.shouldSuppressConflictingDirectedReportDecode(row("IU8LMC K1ABC -10"),"test",&reason));check(b.shouldSuppressConflictingDirectedReportDecode(row("IU8LMC K1ABC -12"),"test",&reason)&&reason.contains("conflicting"));check(!b.shouldSuppressConflictingDirectedReportDecode(row("IU8LMC K1ABC R-12"),"test",&reason));check(!b.shouldSuppressConflictingDirectedReportDecode(row("CQ K1ABC FN42"),"test",&reason));b.m_mode="FT2";b.m_lastTransmittedMessage="K1ABC IU8LMC -10";b.m_transmitting=true;check(!b.shouldSuppressRecentLocalTxEchoDecode("k1abc iu8lmc -10","test",&reason));check(b.shouldSuppressRecentLocalTxEchoDecode("K1ABC IU8LMC -10","test",&reason));b.m_transmitting=false;b.m_asyncLastTxEndMs=QDateTime::currentMSecsSinceEpoch()-16000;check(!b.shouldSuppressRecentLocalTxEchoDecode("K1ABC IU8LMC -10","test",&reason));b.m_asyncLastTxEndMs=QDateTime::currentMSecsSinceEpoch()-100;check(b.shouldSuppressRecentLocalTxEchoDecode("K1ABC IU8LMC -10","test",&reason));check(!b.shouldSuppressRecentLocalTxEchoDecode("W1AW IU8LMC -10","test",&reason));std::cout<<"same/conflicting reports, kinds, active/recent/old self echo passed\n";}
