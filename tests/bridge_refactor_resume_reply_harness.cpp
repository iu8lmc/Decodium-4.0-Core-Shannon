#include <QDateTime>
#include <QRegularExpression>
#include <QStringList>
#include <iostream>
#include "Sequencer/MessageTokenRules.hpp"
using namespace decodium::seq;
void bridgeLog(QString const&){}
class DecodiumBridge {public:bool m_resumeQsoOnReply=true,m_manualTxHold=true,accept=true,echo=false,ghost=false;QString m_resumeTargetCall="K1ABC",m_resumeArmedMode="FT8",m_mode="FT8",m_callsign="IU8LMC";qint64 m_resumeArmedMs=QDateTime::currentMSecsSinceEpoch();int m_resumeArmedTx=3,m_resumeArmedProgress=3,clicks=0;
 QStringList parseFt8Row(QString const& r){return r.split('|');}bool shouldAcceptDecodedMessage(QString const&){return accept;}bool shouldSuppressRecentLocalTxEchoDecode(QString const&,QString const&,QString*){return echo;}bool shouldSuppressDirectedGhostDecode(QStringList const&,QString const&){return ghost;}void processDecodeDoubleClick(QString const&,QString const&,QString const&,int){++clicks;}
 void clearResumeQsoOnReplyArm(QString const&);bool tryResumeQsoOnReply(QStringList const&);};
#include "method.inc"
void check(bool x){if(!x)abort();}
int main(){QString row="1200|-10|0|1500|IU8LMC K1ABC R-10";DecodiumBridge b;b.m_manualTxHold=false;check(!b.tryResumeQsoOnReply({row})&&b.clicks==0);b.m_manualTxHold=true;check(!b.tryResumeQsoOnReply({"1200|-10|0|1500|IU8LMC W1AW R-10"}));b.echo=true;check(!b.tryResumeQsoOnReply({row}));b.echo=false;b.ghost=true;check(!b.tryResumeQsoOnReply({row}));b.ghost=false;check(b.tryResumeQsoOnReply({row})&&b.clicks==1&&b.m_resumeTargetCall.isEmpty());DecodiumBridge c;c.m_resumeArmedMode="FT4";check(!c.tryResumeQsoOnReply({row})&&c.m_resumeTargetCall.isEmpty());DecodiumBridge d;d.m_resumeArmedMs-=121000;check(!d.tryResumeQsoOnReply({row})&&d.m_resumeTargetCall.isEmpty());std::cout<<"hold, partner, echo, ghost, matching reply, mode and timeout passed\n";}
