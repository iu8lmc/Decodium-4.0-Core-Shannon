#include <QtCore>
#include <QtNetwork>
#include <cstdio>
#include <tuple>
#include "Radio.hpp"
#include "UdpTargetHelpers.h"
#include "Network/AdifUdpPayload.hpp"
static void bridgeLog(QString const&){}
struct MessageClient {using Frequency=Radio::Frequency;QStringList events;template<class...T>void qso_logged(T...args){auto t=std::make_tuple(args...);events<<QString("Q:%1:%2:%3:%4:%5").arg(std::get<1>(t),std::get<2>(t)).arg(std::get<3>(t)).arg(std::get<5>(t),std::get<6>(t));}void logged_ADIF(QByteArray p){events<<"A:"+QString::fromUtf8(p);}QHostAddress server_address()const{return QHostAddress::LocalHost;}quint16 server_port()const{return 2237;}};
class DecodiumBridge {public:
 MessageClient *m_udpMessageClient=nullptr,*m_udpSecondaryMessageClient=nullptr,*m_udpTertiaryMessageClient=nullptr;bool legacy=false,m_udpNoTargetWarningSent=false;int m_stationPowerWatts=50;QString m_callsign="IU8LMC",m_grid="JN70";QHash<QString,QVariant> settings;QStringList raw,status,warnings;
 bool usingLegacyBackendForTx()const{return legacy;}void initUdpMessageClient(bool){}QVariant getSetting(QString k,QVariant d)const{return settings.value(k,d);}bool udpTrafficEnabled(QString p,QString)const{return settings.value(p+"SendQso",true).toBool();}
 bool udpSendRawAdifDatagram(QString label,QString,quint16,QString,QByteArray bytes){raw<<label+":"+QString::fromUtf8(bytes);return true;}void statusMessage(QString s){status<<s;}void warningRaised(QString a,QString,QString){warnings<<a;}
 void udpSendLoggedQso(QString const&,QString const&,double,QString const&,QDateTime const&,QDateTime const&,QString const&,QString const&,QByteArray const&,QString const&,QString const&,QString const&,QString const&,QString const&);
};
#include "bridge_refactor_udpqso.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);for(bool legacy:{false,true})for(int mask=0;mask<8;mask++)for(bool clients:{false,true}){DecodiumBridge b;MessageClient a,c,d;b.legacy=legacy;if(clients){b.m_udpMessageClient=&a;b.m_udpSecondaryMessageClient=&c;b.m_udpTertiaryMessageClient=&d;}for(int i=0;i<3;i++)b.settings[QStringList{"UDPPrimary","UDPSecondary","UDPTertiary"}[i]+"SendQso"]=bool(mask&(1<<i));b.settings["UDPSecondaryEnabled"]=true;b.settings["UDPTertiaryEnabled"]=true;QDateTime now=QDateTime::fromString("2026-11-07T12:00:00Z",Qt::ISODate);b.udpSendLoggedQso("K1ABC","FN42",14074000,"FT8",now,now,"", "-08","<CALL:5>K1ABC <BAND:3>20M"," hello ","","","","");printf("%d:%d:%d %s/%s/%s raw=%s statuses=%d warnings=%d\n",legacy,mask,clients,a.events.join(';').toUtf8().constData(),c.events.join(';').toUtf8().constData(),d.events.join(';').toUtf8().constData(),b.raw.join(';').toUtf8().constData(),int(b.status.size()),int(b.warnings.size()));}}
