// Banco del database della mappa live (MapIntelligenceService), non e' un test.
//
// Copia il database indicato in una cartella temporanea, avvia il servizio come
// fa l'app e gli passa decodifiche FT8 vere (righe di un ALL.TXT): 40 per slot
// da 15 s, sparse nei primi 3 s come le passate del decoder. Misura I/O e CPU
// del processo dopo i primi 10 s.
//
//   bench_map_database <map_intelligence.sqlite> <ALL.TXT> [secondi=60] [decodifiche_per_slot=40]
#include "src/services/MapIntelligenceService.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace
{
struct Usage
{
    double readMb {0}, readOps {0}, writeKb {0}, cpuSeconds {0};
};

Usage processUsage()
{
    Usage usage;
#ifdef Q_OS_WIN
    IO_COUNTERS io {};
    if (GetProcessIoCounters(GetCurrentProcess(), &io)) {
        usage.readMb = io.ReadTransferCount / 1048576.0;
        usage.readOps = static_cast<double>(io.ReadOperationCount);
        usage.writeKb = io.WriteTransferCount / 1024.0;
    }
    FILETIME created, exited, kernel, user;
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
        auto seconds = [](FILETIME const& t) {
            return ((static_cast<quint64>(t.dwHighDateTime) << 32) | t.dwLowDateTime) / 1e7;
        };
        usage.cpuSeconds = seconds(kernel) + seconds(user);
    }
#endif
    return usage;
}

QList<QVariantMap> loadDecodes(QString const& path, int limit)
{
    QList<QVariantMap> decodes;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return decodes;
    }
    if (file.size() > 4 * 1024 * 1024) {
        file.seek(file.size() - 4 * 1024 * 1024);
        file.readLine();
    }
    static QRegularExpression const line {
        QStringLiteral(R"(^\d{6}_\d{6}\s+\S+\s+Rx\s+FT8\s+(-?\d+)\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+(.+?)\s*$)")};
    while (!file.atEnd() && decodes.size() < limit) {
        QRegularExpressionMatch const m = line.match(QString::fromLatin1(file.readLine()).trimmed());
        if (!m.hasMatch() || m.captured(4).contains(QStringLiteral("<"))) {
            continue;
        }
        QVariantMap entry;
        entry.insert(QStringLiteral("db"), m.captured(1));
        entry.insert(QStringLiteral("dt"), m.captured(2));
        entry.insert(QStringLiteral("freq"), m.captured(3).toInt());
        entry.insert(QStringLiteral("message"), m.captured(4));
        entry.insert(QStringLiteral("mode"), QStringLiteral("FT8"));
        decodes.append(entry);
    }
    return decodes;
}
}  // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    if (argc < 3) {
        std::fprintf(stderr, "uso: bench_map_database <map_intelligence.sqlite> <ALL.TXT> [secondi] [per_slot]\n");
        return 2;
    }
    int const seconds = argc > 3 ? QString::fromLocal8Bit(argv[3]).toInt() : 60;
    int const perSlot = argc > 4 ? QString::fromLocal8Bit(argv[4]).toInt() : 40;

    QTemporaryDir dir;
    // Impostazioni nella cartella temporanea: il servizio legge e scrive
    // QSettings in formato INI dell'utente, che non va toccato.
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.filePath(QStringLiteral("settings")));
    QString const database = dir.filePath(QStringLiteral("map_intelligence.sqlite"));
    if (!QFile::copy(QString::fromLocal8Bit(argv[1]), database)) {
        std::fprintf(stderr, "copia del database non riuscita\n");
        return 2;
    }
    QList<QVariantMap> const decodes = loadDecodes(QString::fromLocal8Bit(argv[2]), 20000);
    if (decodes.isEmpty()) {
        std::fprintf(stderr, "nessuna decodifica FT8 nel file\n");
        return 2;
    }

    MapIntelligenceService service(nullptr, database);
    service.setOfflineMode(true);

    int next = 0;
    int fed = 0;
    auto feedSlot = [&] {
        for (int i = 0; i < perSlot; ++i) {
            // Le passate del decoder consegnano a gruppi nei primi ~3 s.
            QTimer::singleShot((i * 3000) / perSlot, &service, [&] {
                QVariantMap entry = decodes.at(next++ % decodes.size());
                service.ingestDecodeEntry(entry, 7074000, QStringLiteral("40m"));
                ++fed;
            });
        }
    };
    QTimer slot;
    slot.setInterval(15000);
    QObject::connect(&slot, &QTimer::timeout, feedSlot);
    slot.start();
    QTimer::singleShot(0, feedSlot);

    Usage start;
    QElapsedTimer clock;
    QTimer::singleShot(10000, [&] {
        start = processUsage();
        clock.start();
    });
    QTimer::singleShot((10 + seconds) * 1000, [&] {
        Usage const end = processUsage();
        double const s = clock.elapsed() / 1000.0;
        std::printf("decodifiche passate: %d in %.0f s misurati\n", fed, s);
        std::printf("letture: %.1f MB/s (%.0f operazioni/s)  scritture: %.1f KB/s  CPU: %.2f core\n",
                    (end.readMb - start.readMb) / s, (end.readOps - start.readOps) / s,
                    (end.writeKb - start.writeKb) / s, (end.cpuSeconds - start.cpuSeconds) / s);
        std::fflush(stdout);
        app.quit();
    });
    return app.exec();
}
