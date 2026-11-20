#include <QString>
#include <QStringList>
#include <iostream>
#include "FtxMessageHelpers.h"
namespace decodium::txmsg {bool isStandardFtxCall(QString const& c){return c=="K1ABC" || c=="IU8LMC";}QString bracketHashCall(QString const& c){return "<"+c+">";}}
class DecodiumBridge {public:QString m_callsign="IU8LMC",m_grid="JM75AA",m_mode="FT8",m_reportSent="-10",m_customCqPrefix,m_customCqMessage,m_tx6;bool m_sendRR73=true,m_quickQsoEnabled=false;QStringList out=QStringList(6,QString());
 void setTx1(QString s){out[0]=s;}void setTx2(QString s){out[1]=s;}void setTx3(QString s){out[2]=s;}void setTx4(QString s){out[3]=s;}void setTx5(QString s){out[4]=s;}void setTx6(QString s){out[5]=s;m_tx6=s;}void genStdMsgs(QString const&,QString const&);};
#include "method.inc"
int main(){for(QString mode:{"FT8","FT4","FT2"})for(bool quick:{false,true})for(bool rr:{false,true}){DecodiumBridge b;b.m_mode=mode;b.m_quickQsoEnabled=quick;b.m_sendRR73=rr;b.genStdMsgs("K1ABC","FN42");for(auto const&s:b.out)if(s.isEmpty())abort();std::cout<<mode.toStdString()<<'|'<<quick<<'|'<<rr<<'|'<<b.out.join(";").toStdString()<<'\n';}DecodiumBridge c;c.m_customCqMessage="CQ TEST IU8LMC JM75";c.genStdMsgs("K1ABC","");if(c.out[5]!=c.m_customCqMessage)abort();std::cout<<c.out[5].toStdString()<<'\n';}
