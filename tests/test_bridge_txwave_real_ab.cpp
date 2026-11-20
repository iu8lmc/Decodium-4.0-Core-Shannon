#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QString>
#include <QVector>
#include <iostream>

#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"
#include "src/bridge/DecodiumBridgeTxWave.h"
#include "src/bridge/DecodiumBridgeTxTimingMath.h"

#define q65PeriodSecondsFromModeLabel originalQ65Period
#define q65SubmodeIndexFromModeLabel originalQ65Submode
#define q65SamplesPerSymbolAt48k originalQ65Samples
#define fst4PeriodSecondsForMode originalFst4Period
#define fst4SamplesPerSymbolAt12k originalFst4Samples12k
#define fst4SamplesPerSymbolAt48k originalFst4Samples48k
#define decodiumBridgeFst4SamplesPerSymbol originalFst4Wrapper
#define decodiumBridgeQ65SamplesPerSymbol originalQ65SamplesWrapper
#define decodiumBridgeQ65SubmodeIndex originalQ65SubmodeWrapper
#include "bridge_refactor_fixtures/txwave_timing_before.inc"
#define buildTxWaveformForMessage originalTxWaveformForMessage
#include "bridge_refactor_fixtures/txwave_before.inc"
#undef buildTxWaveformForMessage
#undef q65PeriodSecondsFromModeLabel
#undef q65SubmodeIndexFromModeLabel
#undef q65SamplesPerSymbolAt48k
#undef fst4PeriodSecondsForMode
#undef fst4SamplesPerSymbolAt12k
#undef fst4SamplesPerSymbolAt48k
#undef decodiumBridgeFst4SamplesPerSymbol
#undef decodiumBridgeQ65SamplesPerSymbol
#undef decodiumBridgeQ65SubmodeIndex

int main()
{
    QFile source(QStringLiteral(DECODIUM_SOURCE_DIR "/src/bridge/DecodiumBridgeTxTimingMath.cpp"));
    QFile fixture(QStringLiteral(DECODIUM_SOURCE_DIR "/tests/bridge_refactor_fixtures/txwave_timing_before.inc"));
    if (!source.open(QIODevice::ReadOnly) || !fixture.open(QIODevice::ReadOnly)) return 1;
    const QByteArray original = source.readAll();
    const QByteArray first = "int q65PeriodSecondsFromModeLabel(";
    const auto start = original.indexOf(first);
    QByteArray expectedTiming = fixture.readAll();
    expectedTiming.replace("static int ", "int ");
    if (start < 0 || original.mid(start) != expectedTiming) {
        std::cerr << "Timing helpers changed; update the A/B test deliberately\n";
        return 1;
    }
    for (const QString& label : {QStringLiteral("Q65"), QStringLiteral("Q65-15A"),
                                 QStringLiteral("Q65-30B"), QStringLiteral("Q65-60C"),
                                 QStringLiteral("Q65-120D"), QStringLiteral("Q65-300E"),
                                 QStringLiteral("Q65-99Z"), QStringLiteral("FST4-15"),
                                 QStringLiteral("FST4-30"), QStringLiteral("FST4-60"),
                                 QStringLiteral("FST4W-120"), QStringLiteral("FST4-300"),
                                 QStringLiteral("FST4-900"), QStringLiteral("FST4-1800")}) {
        if (originalQ65Period(label) != q65PeriodSecondsFromModeLabel(label)
            || originalQ65Submode(label) != q65SubmodeIndexFromModeLabel(label)
            || originalQ65Samples(label) != q65SamplesPerSymbolAt48k(label)
            || originalFst4Period(label, 15) != fst4PeriodSecondsForMode(label, 15)
            || originalFst4Samples48k(label, 15) != fst4SamplesPerSymbolAt48k(label, 15)) {
            std::cerr << "Timing mismatch: " << label.toStdString() << '\n';
            return 1;
        }
    }
    struct Case { const char* mode; const char* message; double period; };
    const Case cases[] = {
        {"FT2", "CQ 9H1SR JM75", 15}, {"FT4", "CQ 9H1SR JM75", 15},
        {"FT8", "CQ 9H1SR JM75", 15}, {"JT65", "CQ 9H1SR JM75", 60},
        {"JT4", "CQ 9H1SR JM75", 60}, {"JT9", "CQ 9H1SR JM75", 60},
        {"MSK144", "CQ 9H1SR JM75", 15}, {"WSPR", "9H1SR JM75 37", 120},
        {"FST4-15", "CQ 9H1SR JM75", 15}, {"FST4W-120", "9H1SR JM75 37", 120},
        {"Q65-15A", "CQ 9H1SR JM75", 15}
    };
    for (const auto& c : cases) {
        const QString mode = QString::fromLatin1(c.mode);
        const QString message = QString::fromLatin1(c.message);
        QString oldError, newError;
        const auto before = originalTxWaveformForMessage(mode, message, 1500, c.period, &oldError);
        const auto after = buildTxWaveformForMessage(mode, message, 1500, c.period, &newError);
        if (before.isEmpty() || before != after || oldError != newError) {
            std::cerr << c.mode << " mismatch/empty: " << oldError.toStdString() << " / " << newError.toStdString() << '\n';
            return 1;
        }
        const auto bytes = QByteArray::fromRawData(reinterpret_cast<const char*>(after.constData()),
                                                   static_cast<int>(after.size() * sizeof(float)));
        const auto digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
        std::cout << c.mode << ' ' << after.size() << ' ' << digest.constData() << '\n';
    }
    return 0;
}
