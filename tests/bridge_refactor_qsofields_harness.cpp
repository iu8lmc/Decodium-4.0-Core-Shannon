#include <QtCore>
#include <cstdio>
#include "DecodeSignalReport.h"
#include "Sequencer/MessageTokenRules.hpp"
using decodium::seq::isGridTokenStrict;
#define DIAG_WARN(x) do { } while (0)
struct Legacy { QString grid; int count=0; void setDxGrid(QString s){grid=s;++count;} };
class DecodiumBridge {public:
 QString m_dxGrid,m_reportSent,m_reportReceived; bool m_reportReceivedReal=false,m_sendRR73=false,legacyActive=false; Legacy instance,*m_legacyBackend=&instance;
 int grids=0,sent=0,received=0,rr=0,regens=0,paths=0;
 bool usingLegacyBackendForTx()const{return legacyActive;}
 void dxGridChanged(){++grids;} void reportSentChanged(){++sent;} void reportReceivedChanged(){++received;} void sendRR73Changed(){++rr;}
 void regenerateTxMessages(){++regens;} void emitCurrentWorldMapQsoPath(){++paths;}
 QString dxGrid() const; void setDxGrid(QString const&);void setReportSent(QString const&);void setReportReceivedPlaceholder(QString const&);void setReportReceived(QString const&);void setSendRR73(bool);
};
#include "bridge_refactor_qsofields.inc"
int main(int argc,char**argv){QCoreApplication app(argc,argv);DecodiumBridge b;
 for(bool legacy:{false,true}){b.legacyActive=legacy;for(auto const& grid:QStringList{"fn42", "FN42", "bad", "", "JM75AA", "rr73"}){b.setDxGrid(grid);printf("grid %s %d %d %d %d\n",b.dxGrid().toUtf8().constData(),b.grids,b.regens,b.paths,b.instance.count);}}
 for(auto const& report:QStringList{"-8","R+09","+99","bad","","-10","-10"}) {b.setReportSent(report);b.setReportReceivedPlaceholder(report);printf("placeholder %s %s %d %d %d %d\n",b.m_reportSent.toUtf8().constData(),b.m_reportReceived.toUtf8().constData(),b.m_reportReceivedReal,b.sent,b.received,b.regens);b.setReportReceived(report);printf("real %d %d\n",b.m_reportReceivedReal,b.regens);}
 for(bool v:{true,true,false,false,true}) {b.setSendRR73(v);printf("rr %d %d %d\n",b.m_sendRR73,b.rr,b.regens);}
}
