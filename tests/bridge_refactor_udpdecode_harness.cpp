#include <QtCore>
#include <cstdio>
#include "Radio.hpp"
struct MessageClient {
 QStringList events;
 void decode(bool n,QTime t,int snr,float dt,quint32 df,QString mode,QString msg,bool,bool){events<<QString("D|%1|%2|%3|%4|%5|%6|%7").arg(n).arg(t.toString("hhmmss")).arg(snr).arg(dt).arg(df).arg(mode,msg);}
 void WSPR_decode(bool n,QTime t,int snr,float dt,Radio::Frequency hz,int drift,QString call,QString grid,int power,bool){events<<QString("W|%1|%2|%3|%4|%5|%6|%7|%8|%9").arg(n).arg(t.toString("hhmmss")).arg(snr).arg(dt).arg(hz).arg(drift).arg(call,grid).arg(power);}
};
class DecodiumBridge {public:
 MessageClient *m_udpMessageClient=nullptr,*m_udpSecondaryMessageClient=nullptr,*m_udpTertiaryMessageClient=nullptr;QString m_mode="FT8";bool legacy=false;QHash<QString,QVariant> settings;
 bool usingLegacyBackendForTx()const{return legacy;}QVariant getSetting(QString k,QVariant d)const{return settings.value(k,d);}
 QStringList parseFt8Row(QString r)const{return r.split('|');}bool shouldAcceptDecodedMessage(QString m)const{return !m.contains("REJECT");}
 bool udpTrafficEnabled(QString const&,QString const&)const;void udpSendDecode(bool,QString const&,quint64);void udpSendWsprDecode(bool,QString const&);
};
#include "bridge_refactor_udpdecode.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;MessageClient a,c,d;b.m_udpMessageClient=&a;b.m_udpSecondaryMessageClient=&c;b.m_udpTertiaryMessageClient=&d;
 for(bool legacy:{false,true})for(int mask=0;mask<8;++mask){b.legacy=legacy;b.settings.clear();a.events.clear();c.events.clear();d.events.clear();for(int i=0;i<3;++i){QString p=QStringList{"UDPPrimary","UDPSecondary","UDPTertiary"}[i];b.settings[p+"SendDecode"]=bool(mask&(1<<i));b.settings[p+"SendWspr"]=bool(mask&(1<<i));}b.udpSendDecode(true,"120030|-12|0.2|1500|CQ K1ABC FN42",9);b.udpSendDecode(false,"1200|-2|-0.1|500|REJECT",8);b.udpSendDecode(true,"bad",7);b.udpSendWsprDecode(true,"1200 -21 0.2 14.0971 0 K1ABC FN42 37");b.udpSendWsprDecode(false,"1202 -20 0.1 14.0971 1 9H1SR 30");b.udpSendWsprDecode(true,"short");printf("%d:%d %s / %s / %s\n",legacy,mask,a.events.join(';').toUtf8().constData(),c.events.join(';').toUtf8().constData(),d.events.join(';').toUtf8().constData());}
 b.settings.clear();b.settings["UDPPrimaryLoggedAdifEnabled"]=false;printf("fallback=%d ",b.udpTrafficEnabled("UDPPrimary","Qso"));b.settings["UDPPrimarySendQso"]=true;printf("override=%d\n",b.udpTrafficEnabled("UDPPrimary","Qso"));
}
