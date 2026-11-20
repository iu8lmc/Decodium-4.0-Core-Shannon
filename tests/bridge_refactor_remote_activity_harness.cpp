#include <QCoreApplication>
#include <QDateTime>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <iostream>

// The server is a local substitute: no socket is opened by this harness.
struct Server {
    bool running = true;
    QStringList lines;
    int clears = 0;
    bool isRunning() const { return running; }
    void clearBandActivity() { ++clears; lines.clear(); }
    void publishBandActivityLine(QString const& line) { lines.append(line); }
};
static QString normalizeUtcDisplayToken(QString value) {
    return value.trimmed().remove(':');
}
class DecodiumBridge {
public:
    Server* m_remoteServer = nullptr;
    QSet<QString> m_remoteActivityKeys;
    QStringList m_remoteActivityKeyOrder;
    int m_txFrequency = 1700, m_rxFrequency = 1400;
    QString m_mode = QStringLiteral("FT8");
    void clearRemoteActivityCache(bool);
    void publishRemoteActivityEntries(QVariantList const&);
    void publishRemoteActivityEntry(QVariantMap const&);
};
#include "bridge_refactor_remote_activity.inc"

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    auto check = [](bool ok, char const* name) {
        if (!ok) { std::cerr << name << " failed\n"; std::exit(1); }
        std::cout << name << '\n';
    };
    DecodiumBridge b;
    QVariantMap rx{{"time", "12:34:56"}, {"message", " CQ K1ABC FN42 "}};
    b.publishRemoteActivityEntry(rx);
    check(b.m_remoteActivityKeys.isEmpty(), "no-server");
    Server server; b.m_remoteServer = &server; server.running = false;
    b.publishRemoteActivityEntry(rx);
    check(server.lines.isEmpty(), "stopped-server");
    server.running = true;
    b.publishRemoteActivityEntry({{"time", "123456"}, {"message", " "}});
    b.publishRemoteActivityEntries({rx, rx});
    check(server.lines == QStringList{"123456 0 0.0 1400 CQ K1ABC FN42"}, "rx-and-dedup");
    QVariantMap tx{{"time", "123456"}, {"message", "CQ TEST"}, {"isTx", true}};
    b.publishRemoteActivityEntry(tx);
    check(server.lines.last() == "123456 TX 0.0 1700 CQ TEST", "tx-defaults");
    tx.insert("mode", "FT4"); b.publishRemoteActivityEntry(tx);
    check(server.lines.size() == 3, "mode-key");
    for (int i = 0; i < 4100; ++i)
        b.publishRemoteActivityEntry({{"time", "123456"}, {"message", QString::number(i)}});
    check(b.m_remoteActivityKeys.size() == 4096 && b.m_remoteActivityKeyOrder.size() == 4096, "bounded-cache");
    b.clearRemoteActivityCache(false);
    check(b.m_remoteActivityKeys.isEmpty() && server.clears == 0, "clear-local");
    b.clearRemoteActivityCache(true);
    check(server.clears == 1 && server.lines.isEmpty(), "clear-remote");
    return 0;
}
