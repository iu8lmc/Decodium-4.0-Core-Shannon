#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>
#include <iostream>
class DecodiumBridge : public QObject {
public:
 bool offline=false, m_updateAvailable=false; QString m_latestVersion; QStringList messages; int availableSignals=0, versionSignals=0;
 bool offlineMode() const { return offline; }
 QString version() const { return QStringLiteral("1.0.737"); }
 void statusMessage(const QString &s) { messages << s; }
 void updateAvailableChanged() { ++availableSignals; }
 void latestVersionChanged() { ++versionSignals; }
 void checkForUpdates();
};
static constexpr bool kDecodiumUpdateCheckerEnabled = false;
#include "bridge_refactor_under_test.inc"
int main(int argc,char **argv) {
 QCoreApplication app(argc,argv); DecodiumBridge bridge;
 bridge.offline=true; bridge.m_updateAvailable=true; bridge.m_latestVersion="2.0.0"; bridge.checkForUpdates();
 if (!bridge.m_updateAvailable || bridge.m_latestVersion!="2.0.0" || bridge.messages.last()!="Update checks are paused by Decodium Offline mode." || bridge.availableSignals || bridge.versionSignals) return 1;
 bridge.offline=false; bridge.checkForUpdates();
 if (bridge.m_updateAvailable || !bridge.m_latestVersion.isEmpty() || bridge.availableSignals!=1 || bridge.versionSignals!=1 || bridge.messages.last()!="Update checks are disabled in this build.") return 2;
 bridge.checkForUpdates();
 if (bridge.availableSignals!=1 || bridge.versionSignals!=1 || bridge.messages.size()!=3) return 3;
 std::cout << "offline preserves state; disabled clears state once; repeated check: OK\n";
}
