#include <QtCore>
#include <cstdio>
#include <tuple>
#include "Radio.hpp"
#include "SpecialOperationBounds.h"
struct MessageClient {
 using Frequency=Radio::Frequency;QStringList events;
 template<class...T> void status_update(T... args){auto t=std::make_tuple(args...);events<<QString("%1|%2|%3|%4|%5|%6|%7|%8|%9").arg(std::get<0>(t)).arg(std::get<1>(t),std::get<2>(t),std::get<3>(t)).arg(std::get<6>(t)).arg(std::get<8>(t)).arg(std::get<10>(t)).arg(std::get<16>(t)).arg(std::get<20>(t));}
};
class DecodiumBridge {public:
 MessageClient *m_udpMessageClient=nullptr,*m_udpSecondaryMessageClient=nullptr,*m_udpTertiaryMessageClient=nullptr;QHash<QString,bool> flags;
 double m_frequency=14074000;QString m_mode="FT8",m_dxCall="K1ABC",m_reportSent="",m_callsign="IU8LMC",m_grid="JN70",m_dxGrid="FN42",m_lastTransmittedMessage="CQ IU8LMC JN70";bool m_txEnabled=false,m_transmitting=false,m_decoding=true,legacy=false;int m_rxFrequency=1500,m_txFrequency=1600,m_specialOperationActivity=0,m_qsoProgress=0,m_txPeriod=1;
 bool usingLegacyBackendForTx()const{return legacy;}int periodMsForMode(QString)const{return 15000;}bool udpTrafficEnabled(QString p,QString)const{return flags.value(p,true);}void udpSendStatus();
};
#include "bridge_refactor_udpstatus.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;MessageClient a,c,d;b.m_udpMessageClient=&a;b.m_udpSecondaryMessageClient=&c;b.m_udpTertiaryMessageClient=&d;
 for(bool legacy:{false,true})for(int mask=0;mask<8;++mask)for(int special:{-1,5,20}){b.legacy=legacy;b.m_specialOperationActivity=special;b.m_qsoProgress=special;b.m_reportSent=mask%2?" +03 ":"";a.events.clear();c.events.clear();d.events.clear();for(int i=0;i<3;++i)b.flags[QStringList{"UDPPrimary","UDPSecondary","UDPTertiary"}[i]]=bool(mask&(1<<i));b.udpSendStatus();printf("%d:%d:%d %s / %s / %s\n",legacy,mask,special,a.events.join(';').toUtf8().constData(),c.events.join(';').toUtf8().constData(),d.events.join(';').toUtf8().constData());}
}
