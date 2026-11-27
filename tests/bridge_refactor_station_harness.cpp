#include <QCoreApplication>
#include <QVariantList>
#include <iostream>
static int normalizeWsprPowerDbm(int n){return qBound(0,n,60);}static QString wsprPowerDbmLabel(int n){return QString::number(n);}static QString customCqPrefixFromMessage(const QString&m,const QString&){return m.startsWith("CQ DX")?"CQ DX":QString();}static QString standardFtxCqMessage(const QString&c,const QString&g,const QString&){return "CQ "+c.toUpper()+" "+g;}static QString ftxCqMessageWithCustomPrefix(const QString&m,const QString&p){return p.isEmpty()?m:p+" "+m.mid(3);}static QString standardFtxQsoMessage(int n,const QString&dx,const QString&own,const QString&,const QString&,bool,bool,const QString&){return dx+" "+own+" "+QString::number(n);}static void bridgeLog(const QString&){}
struct Legacy{int identity=0;void setStationIdentity(const QString&,const QString&){++identity;}};struct Map{int calls=0;void setRosterStationCall(const QString&){++calls;}};struct Cluster{int calls=0;void setCallsign(const QString&){++calls;}};
class DecodiumBridge:public QObject{public:
 QString m_callsign,m_grid="JM75",m_mode="FT8",m_stationRigInfo,m_stationAntenna,m_dxCall,m_reportSent,m_customCqPrefix,m_customCqMessage,m_tx1,m_tx2,m_tx3,m_tx4,m_tx5,m_tx6;int m_wsprPowerDbm=30,m_currentTx=1,events=0,moon=0,psk=0;bool m_sendRR73=true,m_skipCustomCqPrefixRecovery=false;Legacy*m_legacyBackend=nullptr;Map*m_mapIntelligenceService=nullptr;Cluster*m_dxCluster=nullptr;
 bool legacyBackendAvailable()const{return m_legacyBackend!=nullptr;}void refreshMapMoonOverlay(){++moon;}void refreshPskReporterLocalStation(){++psk;}void callsignChanged(){++events;}void gridChanged(){++events;}void stationRigInfoChanged(){++events;}void stationAntennaChanged(){++events;}void wsprPowerDbmChanged(){++events;}void setTx1(QString s){m_tx1=s;}void setTx2(QString s){m_tx2=s;}void setTx3(QString s){m_tx3=s;}void setTx4(QString s){m_tx4=s;}void setTx5(QString s){m_tx5=s;}void setTx6(QString s){m_tx6=s;}void setCurrentTx(int n){m_currentTx=n;}
 void setCallsign(const QString&);QString grid()const;void setGrid(const QString&);void setStationRigInfo(const QString&);void setStationAntenna(const QString&);void setWsprPowerDbm(int);QVariantList wsprPowerOptions()const;void regenerateTxMessages();
};
#include "bridge_refactor_under_test.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;Legacy l;Map m;Cluster c;b.m_legacyBackend=&l;b.m_mapIntelligenceService=&m;b.m_dxCluster=&c;b.setCallsign("9H1SR");if(b.m_tx6!="CQ 9H1SR JM75"||b.m_currentTx!=6||l.identity!=1||m.calls!=1)return 1;
 b.setGrid("JN70xx");if(b.grid()!="JN70xx"||b.m_tx6!="CQ 9H1SR JN70"||b.moon!=1||l.identity!=2)return 2;b.m_dxCall="K1ABC";b.m_customCqMessage="CQ WWA 9H1SR JN70";b.regenerateTxMessages();if(b.m_tx6!="CQ WWA 9H1SR JN70"||!b.m_tx1.startsWith("K1ABC"))return 3;
 b.m_mode="WSPR";b.m_currentTx=2;b.setWsprPowerDbm(37);if(b.m_tx6!="9H1SR JN70 37"||!b.m_tx1.isEmpty()||b.m_currentTx!=6||b.wsprPowerOptions().size()!=19)return 4;b.setStationRigInfo("Rig");b.setStationAntenna("Dipole");if(b.psk<4)return 5;
 std::cout<<"station identity, grid, CQ override, directed messages and WSPR controls: OK\n";
}
