#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QPointer>
#include <QRegularExpression>
#include <QRunnable>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QUrl>
#include <QVariantMap>
#include <atomic>
#include <iostream>

class FakeDesktopServices {
public:
    static bool openUrl(const QUrl&) { return true; }
};
#define QDesktopServices FakeDesktopServices
class DecodiumBridge : public QObject {
public:
    std::atomic<quint64> m_ft2LinkReceivedFileIoSerial{0};
    QList<QPair<quint64, QVariantMap>> saves, opens;
    QString defaultFt2LinkReceivedFilesDirectory() const;
    quint64 saveFt2LinkReceivedFileAsync(const QString&, const QString&, const QString&, const QString&, bool, bool);
    quint64 openFt2LinkReceivedFilesDirectoryAsync(const QString&);
    void ft2LinkReceivedFileSaveFinished(quint64 id, const QVariantMap& result) { saves.append({id,result}); }
    void ft2LinkReceivedFilesDirectoryOpenFinished(quint64 id, const QVariantMap& result) { opens.append({id,result}); }
};
#include "bridge_refactor_under_test.inc"
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    if (!temp.isValid()) return 1;
    DecodiumBridge bridge;
    const auto directory = temp.path() + "/received";
    auto first = bridge.saveFt2LinkReceivedFileAsync(directory, "../CON.txt", "alpha", "", false, true);
    auto second = bridge.saveFt2LinkReceivedFileAsync(directory, "binary.dat", "", "AQIDBA==", true, true);
    auto invalid = bridge.saveFt2LinkReceivedFileAsync(directory, "invalid.dat", "", "!!!", true, true);
    auto opened = bridge.openFt2LinkReceivedFilesDirectoryAsync(directory);
    QThreadPool::globalInstance()->waitForDone();
    QCoreApplication::processEvents();
    if (bridge.saves.size()!=3 || bridge.opens.size()!=1) return 2;
    auto find = [&](quint64 id) { for (const auto& p : bridge.saves) if (p.first==id) return p.second; return QVariantMap{}; };
    auto a=find(first), b=find(second), bad=find(invalid);
    QFile text(a.value("path").toString()), binary(b.value("path").toString());
    if (!text.open(QIODevice::ReadOnly) || !binary.open(QIODevice::ReadOnly)) return 3;
    if (!a.value("ok").toBool() || a.value("fileName").toString()!="_CON.txt" || text.readAll()!="alpha" || !b.value("ok").toBool() || binary.readAll()!=QByteArray::fromHex("01020304") || bad.value("ok").toBool() || bridge.opens[0].first!=opened || !bridge.opens[0].second.value("ok").toBool()) return 4;
    auto duplicate=writeFt2LinkReceivedFile(directory,"../CON.txt","second","",false,true);
    if (!duplicate.value("ok").toBool() || duplicate.value("fileName").toString()!="_CON (2).txt") return 5;
    std::cout << "async text, binary, invalid base64, safe names, collision and directory: OK\n";
}
