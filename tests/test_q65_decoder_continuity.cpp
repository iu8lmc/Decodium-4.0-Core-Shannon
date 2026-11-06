#include "wsjtx_config.h"

#include "Detector/Q65DecodeWorker.hpp"
#include "commons.h"

#include <QByteArray>
#include <QtTest>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

extern "C"
{
void q65_encode_message_(char* msg0, char* msgsent, int* payload, int* codeword, int* itone,
                         fortran_charlen_t len1, fortran_charlen_t len2);
}

namespace {

constexpr int kMessageChars {37};
constexpr int kPayloadSymbols {13};
constexpr int kCodewordSymbols {63};
constexpr int kChannelSymbols {85};
constexpr double kTwoPi {6.28318530717958647692};

QVector<short> makeQ65Window(const QString& message, int periodSeconds, int submode,
                              int audioFrequencyHz)
{
    int const samplesPerSymbol = periodSeconds == 15 ? 1800
        : periodSeconds == 30 ? 3600
        : periodSeconds == 60 ? 7200
        : periodSeconds == 120 ? 16000
        : 41472;
    int const totalSamples = periodSeconds * RX_SAMPLE_RATE;
    std::array<char, kMessageChars> source {};
    std::array<char, kMessageChars> encoded {};
    std::array<int, kPayloadSymbols> payload {};
    std::array<int, kCodewordSymbols> codeword {};
    std::array<int, kChannelSymbols> tones {};
    std::fill(source.begin(), source.end(), ' ');
    std::fill(encoded.begin(), encoded.end(), ' ');
    QByteArray const latin = message.leftJustified(kMessageChars, QLatin1Char(' '), true).toLatin1();
    std::memcpy(source.data(), latin.constData(),
                static_cast<size_t>(qMin(latin.size(), kMessageChars)));
    q65_encode_message_(source.data(), encoded.data(), payload.data(), codeword.data(), tones.data(),
                        static_cast<fortran_charlen_t>(source.size()),
                        static_cast<fortran_charlen_t>(encoded.size()));
    if (QString::fromLatin1(encoded.data(), static_cast<int>(encoded.size())).trimmed().isEmpty()) {
        return {};
    }

    QVector<short> window(totalSamples, 0);
    int const initialOffset = (periodSeconds >= 60 ? 1 : 0) * RX_SAMPLE_RATE + RX_SAMPLE_RATE / 2;
    double const toneSpacing = static_cast<double>(RX_SAMPLE_RATE) / samplesPerSymbol;
    double phase = 0.0;
    double phaseIncrement = 0.0;
    int previousSymbol = -1;
    int outputSample = initialOffset;
    int const submodeSpacing = 1 << qBound(0, submode, 4);
    for (int inputSample = 1; inputSample <= totalSamples; ++inputSample) {
        int const symbol = inputSample / samplesPerSymbol + 1;
        if (symbol > kChannelSymbols) {
            break;
        }
        if (symbol != previousSymbol) {
            double const frequency = audioFrequencyHz
                + tones.at(static_cast<size_t>(symbol - 1)) * toneSpacing * submodeSpacing;
            phaseIncrement = kTwoPi * frequency / RX_SAMPLE_RATE;
            previousSymbol = symbol;
        }
        phase += phaseIncrement;
        if (phase > kTwoPi) {
            phase -= kTwoPi;
        }
        ++outputSample;
        if (outputSample >= 1 && outputSample <= totalSamples) {
            window[outputSample - 1] = static_cast<short>(qRound(32767.0 * std::sin(phase)));
        }
    }
    return window;
}

bool containsMessage(const QStringList& rows, const QString& message)
{
    for (QString const& row : rows) {
        if (row.contains(message, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

} // namespace

class TestQ65DecoderContinuity final : public QObject
{
    Q_OBJECT

private slots:
    void decodesThreeConsecutiveFullWindowsWithoutCoherentAveraging();
    void decodesDefaultAndNonDefaultQ65Submodes();
};

void TestQ65DecoderContinuity::decodesThreeConsecutiveFullWindowsWithoutCoherentAveraging()
{
    constexpr int periodSeconds {15};
    constexpr int submode {0}; // Q65-15A: quick enough for a release regression test.
    QString const message = QStringLiteral("K1ABC W9XYZ EN37");
    QVector<short> const audio = makeQ65Window(message, periodSeconds, submode, 1500);
    QVERIFY2(!audio.isEmpty(), "Q65 test message must encode");

    decodium::q65::Q65DecodeWorker worker;
    QVector<quint64> serials;
    QVector<QStringList> resultRows;
    QObject::connect(&worker, &decodium::q65::Q65DecodeWorker::decodeReady,
                     [&serials, &resultRows](quint64 serial, const QStringList& rows) {
        serials.append(serial);
        resultRows.append(rows);
    });

    decodium::q65::DecodeRequest request;
    request.audio = audio;
    request.nutc = 120000;
    request.ntrperiod = periodSeconds;
    request.nsubmode = submode;
    request.nfqso = 1500;
    request.ntol = 20;
    request.ndepth = 3;
    request.nfa = 200;
    request.nfb = 4000;
    request.nclearave = 1;
    request.coherentAvgEnabled = false;
    request.lnewdat = 1;
    request.zapEnabled = false;

    for (quint64 serial = 1; serial <= 3; ++serial) {
        request.serial = serial;
        worker.decode(request);
    }

    QCOMPARE(serials.size(), 3);
    QCOMPARE(resultRows.size(), 3);
    for (int index = 0; index < resultRows.size(); ++index) {
        QCOMPARE(serials.at(index), static_cast<quint64>(index + 1));
        QVERIFY2(containsMessage(resultRows.at(index), message),
                 qPrintable(QStringLiteral("Q65 window %1 did not recover %2: %3")
                                .arg(index + 1)
                                .arg(message, resultRows.at(index).join(QStringLiteral(" | ")))));
    }
}

void TestQ65DecoderContinuity::decodesDefaultAndNonDefaultQ65Submodes()
{
    struct Profile
    {
        int periodSeconds;
        int submode;
        int frequencyHz;
        QString message;
    };

    // Q65-60A is the compatibility default for existing Decodium profiles.
    // Q65-60B proves that the wider tone spacing is passed through rather
    // than silently decoding every GUI choice as submode A.
    QList<Profile> const profiles {
        {60, 0, 1500, QStringLiteral("K1ABC W9XYZ EN37")},
        {60, 1, 1500, QStringLiteral("DL1ABC EA8XYZ IL18")},
    };

    quint64 serial = 10;
    for (Profile const& profile : profiles) {
        QVector<short> const audio = makeQ65Window(profile.message,
                                                    profile.periodSeconds,
                                                    profile.submode,
                                                    profile.frequencyHz);
        QVERIFY2(!audio.isEmpty(), "Q65 profile test message must encode");

        decodium::q65::Q65DecodeWorker worker;
        QVector<quint64> resultSerials;
        QStringList rows;
        QObject::connect(&worker, &decodium::q65::Q65DecodeWorker::decodeReady,
                         [&resultSerials, &rows](quint64 resultSerial,
                                                 const QStringList& resultRows) {
            resultSerials.append(resultSerial);
            rows = resultRows;
        });

        decodium::q65::DecodeRequest request;
        request.serial = serial;
        request.audio = audio;
        request.nutc = 120000;
        request.ntrperiod = profile.periodSeconds;
        request.nsubmode = profile.submode;
        request.nfqso = profile.frequencyHz;
        request.ntol = 20;
        request.ndepth = 3;
        request.nfa = 200;
        request.nfb = 4000;
        request.nclearave = 1;
        request.coherentAvgEnabled = false;
        request.lnewdat = 1;
        request.zapEnabled = false;
        worker.decode(request);

        QCOMPARE(resultSerials, QVector<quint64> {serial});
        QVERIFY2(containsMessage(rows, profile.message),
                 qPrintable(QStringLiteral("Q65-%1%2 did not recover %3: %4")
                                .arg(profile.periodSeconds)
                                .arg(QChar(QLatin1Char('A').unicode() + profile.submode))
                                .arg(profile.message, rows.join(QStringLiteral(" | ")))));
        ++serial;
    }
}

QTEST_MAIN(TestQ65DecoderContinuity)
#include "test_q65_decoder_continuity.moc"
