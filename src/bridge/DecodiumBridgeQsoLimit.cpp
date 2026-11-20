#include "DecodiumBridge.h"
#include "DecodiumSubManagers.h"
#include "DecodiumProfileSettings.h"
#include "Radio.hpp"
#include <QFile>
#include <QRegularExpression>
#include <QSettings>

void decodiumBridgeQsoLimitLog(const QString&);

void DecodiumBridge::setMaxQsoPerCall(int v)
{
    int const clamped = qBound(0, v, 3);
    if (m_maxQsoPerCall == clamped) {
        return;
    }
    m_maxQsoPerCall = clamped;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(settings);
    settings.setValue(QStringLiteral("MaxQsoPerCall"), m_maxQsoPerCall);
    emit maxQsoPerCallChanged();
    decodiumBridgeQsoLimitLog(QStringLiteral("Max QSOs per call set: %1").arg(m_maxQsoPerCall));
}

void DecodiumBridge::loadQsoCountFromLog()
{
    // 1.0.731: count the QSOs of today (UTC) in the ADIF log, per call|band|mode, so the limit
    // survives a restart. SUBMODE is preferred over MODE (FT2 is logged as MFSK/FT2).
    m_qsoCountByKey.clear();
    QDate const today = QDateTime::currentDateTimeUtc().date();
    m_qsoCountDay = today;
    if (m_maxQsoPerCall <= 0) {
        return;
    }
    QFile file(defaultAdifLogPath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QString const text = QString::fromUtf8(file.readAll());
    QString const todayToken = today.toString(QStringLiteral("yyyyMMdd"));
    static QRegularExpression const fieldRx(QStringLiteral("<([A-Za-z_]+):(\\d+)(?::[A-Za-z])?>"));
    for (QString const& rec : text.split(QStringLiteral("<EOR>"), Qt::SkipEmptyParts)) {
        QHash<QString, QString> f;
        QRegularExpressionMatchIterator it = fieldRx.globalMatch(rec);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            int const len = m.captured(2).toInt();
            int const start = m.capturedEnd(0);
            f.insert(m.captured(1).toUpper(), rec.mid(start, len).trimmed());
        }
        if (f.value(QStringLiteral("QSO_DATE")) != todayToken) {
            continue;
        }
        QString const call = Radio::base_callsign(f.value(QStringLiteral("CALL"))).trimmed().toUpper();
        QString const band = f.value(QStringLiteral("BAND")).trimmed().toUpper();
        QString mode = f.value(QStringLiteral("SUBMODE")).trimmed().toUpper();
        if (mode.isEmpty()) {
            mode = f.value(QStringLiteral("MODE")).trimmed().toUpper();
        }
        if (call.isEmpty() || band.isEmpty() || mode.isEmpty()) {
            continue;
        }
        m_qsoCountByKey[QStringLiteral("%1|%2|%3").arg(call, band, mode)] += 1;
    }
    decodiumBridgeQsoLimitLog(QStringLiteral("Max QSOs per call: %1 QSO(s) of today loaded from the log").arg(m_qsoCountByKey.size()));
}


namespace { constexpr int kMamRecentWorkedWindowSeconds {600}; }

QString DecodiumBridge::autoCqBandKeyForFrequency(double freqHz) const
{
    if (freqHz <= 0.0) {
        freqHz = m_frequency;
    }

    struct BandRange { double low; double high; const char* key; };
    static BandRange const ranges[] = {
        {  1800000.0,   2000000.0, "160M"  },
        {  3500000.0,   4000000.0, "80M"   },
        {  5000000.0,   5500000.0, "60M"   },
        {  7000000.0,   7300000.0, "40M"   },
        { 10100000.0,  10150000.0, "30M"   },
        { 14000000.0,  14350000.0, "20M"   },
        { 18068000.0,  18168000.0, "17M"   },
        { 21000000.0,  21450000.0, "15M"   },
        { 24890000.0,  24990000.0, "12M"   },
        { 28000000.0,  29700000.0, "10M"   },
        { 40000000.0,  45000000.0, "8M"    },
        { 50000000.0,  54000000.0, "6M"    },
        { 70000000.0,  71000000.0, "4M"    },
        {144000000.0, 148000000.0, "2M"    },
        {420000000.0, 450000000.0, "70CM"  },
    };

    for (BandRange const& range : ranges) {
        if (freqHz >= range.low && freqHz <= range.high) {
            return QString::fromLatin1(range.key);
        }
    }

    QString const currentBand = m_bandManager ? m_bandManager->currentBandLambda().trimmed().toUpper()
                                              : QString {};
    return currentBand.isEmpty() ? QStringLiteral("--") : currentBand;
}

bool DecodiumBridge::qsoLimitReached(const QString& call, double freqHz, const QString& mode) const
{
    if (m_maxQsoPerCall <= 0) {
        return false;
    }
    QString const dedupeCall = Radio::base_callsign(call).trimmed().toUpper();
    QString const dedupeBand = autoCqBandKeyForFrequency(freqHz).trimmed().toUpper();
    QString const dedupeMode = (mode.isEmpty() ? m_mode : mode).trimmed().toUpper();
    if (dedupeCall.isEmpty() || dedupeBand.isEmpty() || dedupeBand == QStringLiteral("--") || dedupeMode.isEmpty()) {
        return false;
    }
    if (m_qsoCountDay != QDateTime::currentDateTimeUtc().date()) {
        return false;   // a new day starts with no count
    }
    QString const key = QStringLiteral("%1|%2|%3").arg(dedupeCall, dedupeBand, dedupeMode);
    return m_qsoCountByKey.value(key, 0) >= m_maxQsoPerCall;
}

bool DecodiumBridge::isRecentMamWorkedOrLoggedDuplicate(const QString& call, double freqHz,
                                                       const QString& mode) const
{
    QString const dedupeCall = Radio::base_callsign(call).trimmed().toUpper();
    if (dedupeCall.isEmpty()) {
        return false;
    }
    QString const dedupeBand = autoCqBandKeyForFrequency(freqHz).trimmed().toUpper();
    QString const dedupeMode = (mode.isEmpty() ? m_mode : mode).trimmed().toUpper();
    if (dedupeBand.isEmpty() || dedupeBand == QStringLiteral("--") || dedupeMode.isEmpty()) {
        return false;
    }
    QString const dedupeKey = QStringLiteral("%1|%2|%3").arg(dedupeCall, dedupeBand, dedupeMode);
    QDateTime const nowUtc = QDateTime::currentDateTimeUtc();
    auto const inWindow = [&nowUtc](QDateTime const& seenAt) {
        qint64 const deltaSec = seenAt.secsTo(nowUtc);
        return deltaSec >= 0 && deltaSec <= kMamRecentWorkedWindowSeconds;
    };
    auto const workedIt = m_recentAutoCqWorkedUtcByKey.constFind(dedupeKey);
    if (workedIt != m_recentAutoCqWorkedUtcByKey.constEnd() && inWindow(workedIt.value())) {
        return true;
    }
    auto const loggedIt = m_recentQsoLogUtcByKey.constFind(dedupeKey);
    return loggedIt != m_recentQsoLogUtcByKey.constEnd() && inWindow(loggedIt.value());
}
