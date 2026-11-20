#include <QCoreApplication>
#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <QTextStream>

bool decodiumBridgeOwnsLegacyJtRxWhenCatSuppressed(QString const&);
bool decodiumBridgeOwnsFst4Audio(QString const&);

static bool previous(QString const& mode)
{
    QString const normalized = mode.trimmed().toUpper();
    return decodiumBridgeOwnsLegacyJtRxWhenCatSuppressed(normalized)
        || decodiumBridgeOwnsFst4Audio(normalized);
}

static bool optimized(QString const& mode)
{
    if (mode == QStringLiteral("FT8") || mode == QStringLiteral("FT4")
        || mode == QStringLiteral("FT2"))
        return false;
    return previous(mode);
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const QStringList modes {QStringLiteral("FT8"), QStringLiteral("FT4"),
        QStringLiteral("FT2"), QStringLiteral(" ft8 "), QStringLiteral("JT4"),
        QStringLiteral("JT9"), QStringLiteral("JT65"), QStringLiteral("FST4"),
        QStringLiteral("FST4W"), QStringLiteral("fst4-120"),
        QStringLiteral(" FST4W-300 "), QStringLiteral("Q65-30A"), QString()};
    for (const QByteArray value : {QByteArray(), QByteArray("1"), QByteArray("false")}) {
        if (value.isEmpty()) qunsetenv("DECODIUM_DISABLE_CAT");
        else qputenv("DECODIUM_DISABLE_CAT", value);
        for (const auto& mode : modes) {
            if (previous(mode) != optimized(mode)) return 1;
        }
    }
    qunsetenv("DECODIUM_DISABLE_CAT");
    constexpr int iterations = 500000;
    volatile int sink = 0;
    const QString measuredMode = argc > 1 ? QString::fromUtf8(argv[1]) : QStringLiteral("FT8");
    QElapsedTimer timer;
    timer.start();
    for (int i = 0; i < iterations; ++i) sink += previous(measuredMode);
    const qint64 before = timer.nsecsElapsed();
    timer.restart();
    for (int i = 0; i < iterations; ++i) sink += optimized(measuredMode);
    const qint64 after = timer.nsecsElapsed();
    QTextStream(stdout) << "iterations=" << iterations << " previous_ns=" << before
                        << " optimized_ns=" << after << " sink=" << sink << Qt::endl;
    return 0;
}
