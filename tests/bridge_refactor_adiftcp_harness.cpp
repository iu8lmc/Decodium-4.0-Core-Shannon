#include <QtCore>
#include <QtNetwork>
#include <cstdio>
#include "UdpTargetHelpers.h"
static QStringList logs;
static void bridgeLog(QString const& s){logs<<s;}
class DecodiumBridge : public QObject {public:
 QHash<QString,QVariant> settings;QVariant getSetting(QString k,QVariant d)const{return settings.value(k,d);}void tcpSendLoggedAdifQso(QString const&,QByteArray const&);
};
#include "bridge_refactor_adiftcp.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);QTcpServer server;if(!server.listen(QHostAddress::LocalHost,0))return 2;DecodiumBridge b;b.settings["ADIFTcpServer"]="127.0.0.1";b.settings["ADIFTcpPort"]=int(server.serverPort());QByteArray record="<CALL:5>K1ABC <BAND:3>20M";
 b.tcpSendLoggedAdifQso("K1ABC",record);QCoreApplication::processEvents();printf("disabled=%d\n",server.hasPendingConnections());b.settings["ADIFTcpEnabled"]=true;b.tcpSendLoggedAdifQso("K1ABC",{});QCoreApplication::processEvents();printf("empty=%d\n",server.hasPendingConnections());b.tcpSendLoggedAdifQso("K1ABC",record);
 QElapsedTimer timer;timer.start();while(!server.hasPendingConnections() && timer.elapsed()<3000)QCoreApplication::processEvents(QEventLoop::AllEvents,20);if(!server.hasPendingConnections())return 3;QTcpSocket* peer=server.nextPendingConnection();QByteArray data;timer.restart();while(!data.contains("<EOR>\n")&&timer.elapsed()<3000){QCoreApplication::processEvents(QEventLoop::AllEvents,20);data+=peer->readAll();}printf("payload=%s",data.constData());if(data!=record+" <EOR>\n")return 4;
 quint16 const closedPort=server.serverPort();server.close();b.settings["ADIFTcpPort"]=int(closedPort);logs.clear();b.tcpSendLoggedAdifQso("K1ABC",record);timer.restart();bool failed=false;while(!failed&&timer.elapsed()<3000){QCoreApplication::processEvents(QEventLoop::AllEvents,20);for(auto const& line:logs)if(line.contains("ADIF TCP failed:"))failed=true;}printf("refused=%d\n",failed);return failed?0:5;
}
