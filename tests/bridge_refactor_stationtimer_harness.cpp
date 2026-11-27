#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QTimer>
#include <QString>
#include <iostream>
static QString lastLog;
static void bridgeLog(const QString& s) { lastLog = s; }
static bool bridgeTxPeriodIsEven(int p) { return p == 0; }
class DecodiumBridge : public QObject {
public:
 bool m_stationTelemetryPending=false, m_transmitting=false;
 qint64 m_stationTelemetryPendingSinceMs=0, now=0;
 int m_mode=0, m_txPeriod=0, period=100, sends=0;
 int effectivePeriodMsForMode(int) { return period; }
 qint64 correctedUtcMsecsSinceStartOfDay() { return now; }
 void sendStationTelemetry() { ++sends; }
 void armStationTelemetryForNextSlot(const QString&, qint64);
};
#include "bridge_refactor_under_test.inc"
static void wait(int ms) { QEventLoop loop; QTimer::singleShot(ms,&loop,&QEventLoop::quit); loop.exec(); }
int main(int argc,char**argv) {
 QCoreApplication app(argc,argv);
 DecodiumBridge a; a.now=25; a.m_txPeriod=0; a.armStationTelemetryForNextSlot("even",-1);
 std::cout << "even|" << lastLog.toStdString() << "|" << a.m_stationTelemetryPending << '\n';
 DecodiumBridge b; b.now=25; b.m_txPeriod=1; b.armStationTelemetryForNextSlot("odd",-1);
 std::cout << "odd|" << lastLog.toStdString() << '\n';
 DecodiumBridge c; c.period=0; c.armStationTelemetryForNextSlot("fallback",-1);
 std::cout << "fallback|" << lastLog.toStdString() << '\n';
 DecodiumBridge d; d.armStationTelemetryForNextSlot("forced",5); auto stamp=d.m_stationTelemetryPendingSinceMs; d.armStationTelemetryForNextSlot("repeat",6); wait(25);
 std::cout << "forced|" << d.sends << "|" << (stamp==d.m_stationTelemetryPendingSinceMs) << '\n';
 DecodiumBridge e; e.m_transmitting=true; e.armStationTelemetryForNextSlot("busy",5); wait(20);
 DecodiumBridge f; f.armStationTelemetryForNextSlot("cancel",5); f.m_stationTelemetryPending=false; wait(20);
 std::cout << "guards|" << e.sends << "|" << f.sends << '\n';
}
