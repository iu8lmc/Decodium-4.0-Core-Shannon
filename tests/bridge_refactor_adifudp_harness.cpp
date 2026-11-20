#include <QtCore>
#include <QtNetwork>
#include <cstdio>
#include "UdpTargetHelpers.h"
#include "Network/AdifUdpPayload.hpp"
static void bridgeLog(QString const&){}
class DecodiumBridge {public:
 QHash<QString,QVariant> settings;QStringList notices;
 QVariant getSetting(QString k,QVariant d)const{return settings.value(k,d);}void statusMessage(QString s){notices<<s;}
 bool udpSendRawAdifDatagram(QString const&,QString const&,quint16,QString const&,QByteArray const&);void udpSendN1mmLoggedQso(QString const&,QByteArray const&);
};
#include "bridge_refactor_adifudp.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QUdpSocket sink;if(!sink.bind(QHostAddress(QHostAddress::LocalHost),quint16(0)))return 2;DecodiumBridge b;auto port=sink.localPort();QByteArray input="<CALL:5>K1ABC <BAND:3>20M";
 printf("invalid=%d,%d,%d\n",b.udpSendRawAdifDatagram("test","",port,"K1ABC",input),b.udpSendRawAdifDatagram("test","127.0.0.1",0,"K1ABC",input),b.udpSendRawAdifDatagram("test","127.0.0.1",port,"K1ABC",{}));
 printf("raw=%d\n",b.udpSendRawAdifDatagram("test","127.0.0.1",port,"K1ABC",input));if(!sink.waitForReadyRead(1000))return 3;QByteArray data;data.resize(int(sink.pendingDatagramSize()));sink.readDatagram(data.data(),data.size());printf("payload=%s\n",data.constData());
 b.udpSendN1mmLoggedQso("K1ABC",input);printf("disabled=%d\n",sink.hasPendingDatagrams());b.settings["BroadcastToN1MM"]=true;b.settings["N1MMServer"]="127.0.0.1";b.settings["N1MMServerPort"]=int(port);b.udpSendN1mmLoggedQso("K1ABC",input);if(!sink.waitForReadyRead(1000))return 4;data.resize(int(sink.pendingDatagramSize()));sink.readDatagram(data.data(),data.size());printf("n1mm=%s notices=%d\n",data.constData(),int(b.notices.size()));
 printf("ports=%u,%u,%u,%u\n",udpPortFromSettingValue(0,2237),udpPortFromSettingValue(0,2237,true),udpPortFromSettingValue(65536,2237),udpPortFromSettingValue(1234,2237));
}
