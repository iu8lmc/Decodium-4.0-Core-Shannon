#include "DecodiumBridge.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimeZone>
#include <algorithm>
#include <utility>

extern "C" void ftx_ft8_stage4_seed_known_cq_c(char const*, char const*, float, float, int);
extern "C" void ftx_ft8_stage4_seed_hash_call_c(char const*);
void decodiumBridgeKnownCqLog(const QString& message);

void DecodiumBridge::moveLegacyAllTxtCursorToEnd()
{
    QString const path = legacyAllTxtPath().trimmed();
    QFileInfo const info(path);
    m_legacyAllTxtConsumedPath = info.exists() ? info.absoluteFilePath() : path;
    m_legacyAllTxtConsumedSize = info.exists() ? info.size() : -1;
}

void DecodiumBridge::seedFt8KnownCqCacheFromAllTxt()
{
    QString const path = legacyAllTxtPath().trimmed();
    QFileInfo const info(path);
    if (path.isEmpty() || !info.exists() || !info.isFile()) {
        return;
    }

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    // The CQ replay is explicitly opt-in and only supported for JTDX input.
    // Hash seeding below is independent and must still inspect every FT8 line.
    bool const knownCqSourceAllowed =
        info.absolutePath().contains(QStringLiteral("JTDX"), Qt::CaseInsensitive)
        || info.fileName().startsWith(QStringLiteral("JTDX"), Qt::CaseInsensitive);
    QString const seedKnownCqText =
        qEnvironmentVariable("DECODIUM_FT8_SEED_KNOWN_CQ_REPLAY").trimmed().toLower();
    bool const seedKnownCqReplay =
        knownCqSourceAllowed
        && (seedKnownCqText == QStringLiteral("1")
            || seedKnownCqText == QStringLiteral("true")
            || seedKnownCqText == QStringLiteral("yes"));

    constexpr qint64 kTailBytes = 8 * 1024 * 1024;
    if (info.size() > kTailBytes) {
        file.seek(info.size() - kTailBytes);
        file.readLine();
    }

    static QRegularExpression const cqPattern(
        QStringLiteral(R"(^(\d{6})_(\d{6})\s+[0-9]+(?:\.[0-9]+)?\s+Rx\s+FT8\s+-?\d+\s+(-?\d+(?:\.\d+)?)\s+(\d+)\s+CQ\s+([A-Z0-9/]{3,12})\s+([A-R]{2}\d{2})\b)"),
        QRegularExpression::CaseInsensitiveOption);

    struct KnownCqSeed {
        QString call;
        QString grid;
        float freq {0.0f};
        float dt {0.0f};
        int nutc {-1};
        qint64 order {0};
        int hits {1};
    };

    QHash<QString, KnownCqSeed> seeds;
    QHash<QString, qint64> hashCallOrders;
    qint64 order = 0;
    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    constexpr qint64 kKnownCqSeedMaxAgeSeconds = 3 * 3600 + 15;
    static QRegularExpression const ft8LinePattern(
        QStringLiteral(R"(^\d{6}_(\d{6})\s+[0-9]+(?:\.[0-9]+)?\s+RX\s+FT8\s+-?\d+\s+-?\d+(?:\.\d+)?\s+\d+\s+(.+)$)"),
        QRegularExpression::CaseInsensitiveOption);
    static QRegularExpression const wordSplitPattern(QStringLiteral(R"(\s+)"));
    auto looksLikeHashCallSeed = [] (QString call) {
        call = call.trimmed().toUpper();
        if (call.size() < 3 || call.size() > 13 || call == QStringLiteral("...")) {
            return false;
        }
        if (call == QStringLiteral("CQ") || call == QStringLiteral("DE")
            || call == QStringLiteral("QRZ") || call == QStringLiteral("RRR")
            || call == QStringLiteral("RR73") || call == QStringLiteral("73")
            || call == QStringLiteral("TU")) {
            return false;
        }
        bool hasLetter = false;
        bool hasDigit = false;
        for (QChar const ch : call) {
            ushort const code = ch.unicode();
            bool const letter = code >= 'A' && code <= 'Z';
            bool const digit = code >= '0' && code <= '9';
            if (!letter && !digit && code != '/') {
                return false;
            }
            hasLetter = hasLetter || letter;
            hasDigit = hasDigit || digit;
        }
        // Four-character Maidenhead locators pass the alphabet and mixed
        // letter/digit checks but were excluded by the original grid pattern.
        if (call.size() == 4
            && call[0] >= QLatin1Char('A') && call[0] <= QLatin1Char('R')
            && call[1] >= QLatin1Char('A') && call[1] <= QLatin1Char('R')
            && call[2] >= QLatin1Char('0') && call[2] <= QLatin1Char('9')
            && call[3] >= QLatin1Char('0') && call[3] <= QLatin1Char('9')) {
            return false;
        }
        return hasLetter && hasDigit;
    };

    auto fixedField = [] (QString const& value, int width) {
        QByteArray field = value.toLatin1().left(width);
        if (field.size() < width) {
            field.append(QByteArray(width - field.size(), ' '));
        }
        return field;
    };

    auto rememberHashCallsFromMessage = [&] (QString const& message, qint64 lineOrder) {
        QString const normalized = message.trimmed().toUpper();
        for (QString token : normalized.split(wordSplitPattern, Qt::SkipEmptyParts)) {
            while (!token.isEmpty()
                   && (token.front() == QLatin1Char('<')
                       || token.front() == QLatin1Char(';')
                       || token.front() == QLatin1Char(','))) {
                token.remove(0, 1);
            }
            while (!token.isEmpty()
                   && (token.back() == QLatin1Char('>')
                       || token.back() == QLatin1Char(';')
                       || token.back() == QLatin1Char(','))) {
                token.chop(1);
            }
            if (looksLikeHashCallSeed(token)) {
                hashCallOrders.insert(token, lineOrder);
            }
        }
    };

    while (!file.atEnd()) {
        QString const line = QString::fromUtf8(file.readLine()).trimmed().toUpper();
        ++order;
        QRegularExpressionMatch const ft8LineMatch = ft8LinePattern.match(line);
        if (ft8LineMatch.hasMatch()) {
            rememberHashCallsFromMessage(ft8LineMatch.captured(2), order);
        }

        if (!seedKnownCqReplay) {
            continue;
        }
        QRegularExpressionMatch const match = cqPattern.match(line);
        if (!match.hasMatch()) {
            continue;
        }

        QString const yymmdd = match.captured(1);
        QDate const lineDate(2000 + yymmdd.left(2).toInt(),
                             yymmdd.mid(2, 2).toInt(),
                             yymmdd.mid(4, 2).toInt());
        QTime const lineTime = QTime::fromString(match.captured(2), QStringLiteral("HHmmss"));
        QDateTime const lineDateTime(lineDate, lineTime, QTimeZone::UTC);
        qint64 const ageSeconds = lineDateTime.isValid() ? lineDateTime.secsTo(nowUtc) : -1;
        if (ageSeconds < 0 || ageSeconds > kKnownCqSeedMaxAgeSeconds) {
            continue;
        }

        QString const call = match.captured(5).trimmed().toUpper();
        QString const grid = match.captured(6).trimmed().toUpper();
        if (grid == QStringLiteral("RR73")) {
            continue;
        }

        bool okFreq = false;
        float const freq = match.captured(4).toFloat(&okFreq);
        if (!okFreq || freq <= 0.0f) {
            continue;
        }
        bool okDt = false;
        float const dt = match.captured(3).toFloat(&okDt);

        bool okTime = false;
        int const nutc = match.captured(2).toInt(&okTime);

        QString const key = call + QLatin1Char('|') + grid;
        KnownCqSeed seed {call, grid, freq, okDt ? dt : 0.0f,
                          okTime ? nutc : -1, order, 1};
        if (seeds.contains(key)) {
            seed.hits = qMin(seeds.value(key).hits + 1, 20);
        }
        seeds.insert(key, seed);
    }

    QList<KnownCqSeed> orderedSeeds = seeds.values();
    std::sort(orderedSeeds.begin(), orderedSeeds.end(),
              [](KnownCqSeed const& lhs, KnownCqSeed const& rhs) {
                  return lhs.order < rhs.order;
              });

    int seeded = 0;

    if (seedKnownCqReplay) {
        for (KnownCqSeed const& seed : orderedSeeds) {
            QString const& call = seed.call;
            QString const& grid = seed.grid;
            QByteArray callField = fixedField(call, 12);
            QByteArray gridField = fixedField(grid, 4);

            for (int hit = 0; hit < seed.hits; ++hit) {
                ftx_ft8_stage4_seed_known_cq_c(callField.constData(), gridField.constData(),
                                               seed.freq, seed.dt, seed.nutc);
            }
            ++seeded;
        }
    }

    QList<QString> hashCalls = hashCallOrders.keys();
    std::sort(hashCalls.begin(), hashCalls.end(),
              [&hashCallOrders] (QString const& lhs, QString const& rhs) {
                  return hashCallOrders.value(lhs) < hashCallOrders.value(rhs);
              });
    if (hashCalls.size() > 4096) {
        hashCalls = hashCalls.mid(hashCalls.size() - 4096);
    }

    int hashSeeded = 0;
    for (QString const& call : std::as_const(hashCalls)) {
        QByteArray const callField = fixedField(call, 13);
        ftx_ft8_stage4_seed_hash_call_c(callField.constData());
        ++hashSeeded;
    }

    if (seeded > 0 || hashSeeded > 0) {
        QString const message = QStringLiteral("FT8 known-CQ cache seeded: entries=%1 hashCalls=%2 source=%3")
                                    .arg(seeded)
                                    .arg(hashSeeded)
                                    .arg(info.absoluteFilePath());
        qInfo().noquote() << message;
        decodiumBridgeKnownCqLog(message);
    }
}

