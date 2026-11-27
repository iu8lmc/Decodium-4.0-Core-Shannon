#include <QCoreApplication>
#include <QDebug>
#include <QStringList>
#include <iostream>
class FakeMap {
public:
 bool offline = true, refuse = false;
 bool offlineMode() const { return offline; }
 void setNetworkOfflineMode(bool v) { if (!refuse) offline = v; }
};
class DecodiumBridge {
public:
 FakeMap* m_mapIntelligenceService = nullptr;
 QStringList errors, statuses;
 static QString tr(const char* s) { return QString::fromUtf8(s); }
 void errorMessage(const QString& s) { errors << s; }
 void statusMessage(const QString& s) { statuses << s; }
 bool offlineMode() const;
 void resumeOnlineServices();
};
#include "bridge_refactor_under_test.inc"
int main(int argc, char** argv) {
 QCoreApplication app(argc,argv); DecodiumBridge b; FakeMap m;
 if (b.offlineMode()) return 1;
 b.resumeOnlineServices(); if (b.errors.size()!=1 || !b.statuses.isEmpty()) return 2;
 b.m_mapIntelligenceService=&m; if (!b.offlineMode()) return 3;
 m.refuse=true; b.resumeOnlineServices(); if (b.errors.size()!=2 || !b.offlineMode()) return 4;
 m.refuse=false; b.resumeOnlineServices(); if (b.offlineMode() || b.statuses.size()!=1) return 5;
 std::cout << "missing, refused and resumed: " << b.errors.join(" | ").toStdString() << " | " << b.statuses.join(" | ").toStdString() << '\n';
}
