#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include "MapIntelligenceService.h"
#include "Network/DecodiumPskReporterLite.h"
#include "Sequencer/MessageTokenRules.hpp"

#include <QDateTime>
#include <QDebug>
#include <QSettings>
#include <cmath>

using decodium::seq::normalizedBaseCall;
using decodium::seq::isGridTokenStrict;
bool decodiumBridgeUnresolvedGridOnlyDecode(const QString&, QString*);
bool decodiumBridgeFst4wUnresolvedHashDecode(QString const&, QString const&);
bool decodiumBridgePskDebugEnabled();
QString decodiumBridgeExtractRightCallsign(const QString&);
int decodiumBridgeSecondsFromUtcDisplayToken(QString);

bool DecodiumBridge::pskReporterConnected() const { return m_pskReporter && m_pskReporter->isConnected(); }

void DecodiumBridge::setPskReporterEnabled(bool v)
{
    if (m_pskReporterEnabled == v) return;
    m_pskReporterEnabled = v;
    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
    s.setValue(QStringLiteral("pskReporterEnabled"), v);
    s.setValue(QStringLiteral("PSKReporter"), v);
    s.sync();
    syncSettingToLegacyIni(QStringLiteral("PSKReporter"), v);
    if (m_pskReporter) {
        m_pskReporter->setEnabled(v);
        if (v) {
            // Make sure the reporter knows our local station as soon as the
            // user enables it — otherwise the first batch flush would skip
            // because m_myCall is empty.
            refreshPskReporterLocalStation();
        }
    }
    emit pskReporterEnabledChanged();
    // setEnabled() above already emits connectedChanged() via the reporter,
    // but emit here too in case m_pskReporter is null at this point.
    emit pskReporterConnectedChanged();
}

void DecodiumBridge::setPskReporterTimeSpanMinutes(int minutes)
{
    // PSK Reporter accepts a negative number of seconds. Keep the UI and
    // persisted value on the requested 5-minute grid even if a profile was
    // edited manually or was created by an older build.
    minutes = qBound(5, minutes, 60);
    minutes = qBound(5, ((minutes + 2) / 5) * 5, 60);
    if (m_pskReporterTimeSpanMinutes == minutes) {
        return;
    }

    m_pskReporterTimeSpanMinutes = minutes;
    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
    s.setValue(QStringLiteral("PskReporterTimeSpanMinutes"), minutes);
    s.sync();
    emit pskReporterTimeSpanMinutesChanged();
    emit settingValueChanged(QStringLiteral("PskReporterTimeSpanMinutes"), minutes);
}

void DecodiumBridge::syncPskMapSpotWindowToMap()
{
    if (!m_mapIntelligenceService) {
        return;
    }

    QVariantMap decayMinutes = m_mapIntelligenceService->sourceDecayMinutes();
    if (decayMinutes.value(QStringLiteral("psk")).toInt() == m_pskMapSpotWindowMinutes) {
        return;
    }

    decayMinutes.insert(QStringLiteral("psk"), m_pskMapSpotWindowMinutes);
    m_mapIntelligenceService->setSourceDecayMinutes(decayMinutes);
}

void DecodiumBridge::setPskMapSpotWindowMinutes(int minutes)
{
    minutes = qBound(5, minutes, 60);
    minutes = qBound(5, ((minutes + 2) / 5) * 5, 60);
    if (m_pskMapSpotWindowMinutes == minutes) {
        syncPskMapSpotWindowToMap();
        return;
    }

    m_pskMapSpotWindowMinutes = minutes;
    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
    s.setValue(QStringLiteral("PskMapSpotWindowMinutes"), minutes);
    s.sync();
    syncPskMapSpotWindowToMap();
    emit pskMapSpotWindowMinutesChanged();
    emit settingValueChanged(QStringLiteral("PskMapSpotWindowMinutes"), minutes);
}

QString DecodiumBridge::pskReporterProgramInfo() const
{
    return QStringLiteral("Decodium4 v%1").arg(version()).simplified();
}

QString DecodiumBridge::pskReporterAntennaInfo() const
{
    return m_stationAntenna.trimmed();
}

QString DecodiumBridge::pskReporterRigInfo() const
{
    QString rig = m_stationRigInfo.trimmed();

    if (rig.isEmpty() && m_catConnected) {
        rig = m_catRigName.trimmed();
    }
    if (rig.isEmpty()) {
        QObject* cat = catManagerObj();
        if (cat && cat->property("connected").toBool()) {
            rig = cat->property("rigName").toString().trimmed();
        }
    }
    if (rig.isEmpty()) {
        rig = m_catConnected ? m_catBackend.trimmed() : QStringLiteral("No CAT control");
    }

    if (rig.compare(QStringLiteral("None"), Qt::CaseInsensitive) == 0) {
        rig = QStringLiteral("No CAT control");
    } else if (rig.compare(QStringLiteral("Ham Radio Deluxe"), Qt::CaseInsensitive) == 0) {
        rig = QStringLiteral("N/A (HRD)");
    } else if (rig.compare(QStringLiteral("DX Lab Suite Commander"), Qt::CaseInsensitive) == 0) {
        rig = QStringLiteral("N/A (DXLab)");
    } else if (rig.contains(QStringLiteral("OmniRig"), Qt::CaseInsensitive)) {
        rig = QStringLiteral("N/A (OmniRig)");
    } else if (rig.compare(QStringLiteral("FLRig"), Qt::CaseInsensitive) == 0) {
        rig = QStringLiteral("N/A (FLRig)");
    } else if (rig.contains(QStringLiteral("TCI Cli"), Qt::CaseInsensitive)) {
        rig = QStringLiteral("N/A (TCI)");
    }

    return rig.trimmed().isEmpty() ? QStringLiteral("No CAT control") : rig.trimmed();
}

void DecodiumBridge::refreshPskReporterLocalStation()
{
    if (!m_pskReporter) {
        return;
    }

    m_pskReporter->setLocalStation(m_callsign,
                                    m_grid,
                                    pskReporterProgramInfo(),
                                    pskReporterAntennaInfo(),
                                    pskReporterRigInfo());
}

void DecodiumBridge::maybeQueuePskReporterSpot(const QVariantMap& entry,
                                               const QString& message,
                                               bool isCQ,
                                               const QString& audioFreqHz,
                                               const QString& snrText,
                                               const QString& mode)
{
    if (!m_pskReporterEnabled || !m_pskReporter || m_callsign.isEmpty()) {
        return;
    }

    QString reportMode = mode.trimmed().isEmpty()
        ? entry.value(QStringLiteral("mode"), m_mode).toString().trimmed()
        : mode.trimmed();
    if (entry.value(QStringLiteral("partialDecode")).toBool()
        || entry.value(QStringLiteral("unresolvedHash")).toBool()) {
        return;
    }

    QString const dxGridExtracted = entry.value(QStringLiteral("dxGrid")).toString().trimmed().toUpper();
    QString spCall = entry.value(QStringLiteral("dxCallsign")).toString().trimmed().toUpper();
    bool const unresolvedNonOperationalDecode =
        entry.value(QStringLiteral("partialDecode")).toBool()
        || entry.value(QStringLiteral("hasUnresolvedGridOnly")).toBool()
        || entry.value(QStringLiteral("unresolvedHash")).toBool()
        || decodiumBridgeUnresolvedGridOnlyDecode(message, nullptr)
        || decodiumBridgeFst4wUnresolvedHashDecode(reportMode, message);
    QString const myCallUpper = m_callsign.trimmed().toUpper();
    QString const myBaseUpper = normalizedBaseCall(myCallUpper);

    auto sameAsMyCall = [&myCallUpper, &myBaseUpper](QString const& call) {
        QString const base = normalizedBaseCall(call);
        return call == myCallUpper
            || (!base.isEmpty() && !myBaseUpper.isEmpty() && base == myBaseUpper);
    };

    if (!spCall.isEmpty() && sameAsMyCall(spCall)) {
        QString const fromCall = entry.value(QStringLiteral("fromCall")).toString().trimmed().toUpper();
        if (!fromCall.isEmpty() && !sameAsMyCall(fromCall)) {
            spCall = fromCall;
        }
    }

    auto logIgnored = [&](QString const& reason) {
        if (!decodiumBridgePskDebugEnabled()) {
            return;
        }
        static qint64 lastLogMs = 0;
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - lastLogMs < 10'000) {
            return;
        }
        lastLogMs = nowMs;
        qInfo().noquote() << "[PSKDBG] PSK Reporter spot ignored reason=" << reason
                          << "msg=" << message
                          << "call=" << (spCall.isEmpty() ? QStringLiteral("<empty>") : spCall)
                          << "grid=" << (dxGridExtracted.isEmpty() ? QStringLiteral("<empty>") : dxGridExtracted)
                          << "cq=" << (isCQ ? 1 : 0);
    };

    if (unresolvedNonOperationalDecode) {
        logIgnored(QStringLiteral("unresolved-placeholder"));
        return;
    }
    if (spCall.isEmpty()) {
        spCall = decodiumBridgeExtractRightCallsign(message).trimmed().toUpper();
    }
    if (spCall.isEmpty()) {
        logIgnored(QStringLiteral("empty-call"));
        return;
    }
    if (sameAsMyCall(spCall)) {
        logIgnored(QStringLiteral("self"));
        return;
    }

    quint64 const audioHz = audioFreqHz.trimmed().toULongLong();
    quint64 const absFreqHz = static_cast<quint64>(m_frequency) + audioHz;
    reportMode = reportMode.isEmpty() ? m_mode : reportMode;
    if (absFreqHz == 0 || reportMode.trimmed().isEmpty()) {
        logIgnored(absFreqHz == 0 ? QStringLiteral("empty-frequency") : QStringLiteral("empty-mode"));
        return;
    }

    if (decodiumBridgePskDebugEnabled()) {
        qInfo().noquote() << "[PSKDBG] PSK Reporter spot candidate msg=" << message
                          << "call=" << spCall
                          << "grid=" << (isGridTokenStrict(dxGridExtracted) ? dxGridExtracted : QStringLiteral("<empty>"))
                          << "cq=" << (isCQ ? 1 : 0)
                          << "freqHz=" << absFreqHz
                          << "mode=" << reportMode;
    }
    m_pskReporter->addSpot(spCall,
                            isGridTokenStrict(dxGridExtracted) ? dxGridExtracted : QString(),
                            absFreqHz,
                            reportMode,
                            snrText.toInt());
}

void DecodiumBridge::replayPskReporterRecentEntries(const QVariantList& entries, const QString& source)
{
    if (!m_pskReporterEnabled || !m_pskReporter || m_callsign.isEmpty()) {
        return;
    }

    int const periodMs = periodMsForMode(m_mode);
    int const freshnessWindowSeconds = qMax(90, (periodMs > 0 ? periodMs / 1000 : 15) * 4);
    int const nowSeconds =
        QDateTime::currentDateTimeUtc().time().msecsSinceStartOfDay() / 1000;

    int inspected = 0;
    int recent = 0;
    int candidates = 0;
    QString lastTimeToken;
    int const start = qMax(0, entries.size() - 80);
    for (int i = start; i < entries.size(); ++i) {
        QVariantMap const entry = entries.at(i).toMap();
        if (entry.value(QStringLiteral("isTx")).toBool()) {
            continue;
        }
        lastTimeToken = entry.value(QStringLiteral("time")).toString();
        ++inspected;
        int const decodeSeconds = decodiumBridgeSecondsFromUtcDisplayToken(entry.value(QStringLiteral("time")).toString());
        if (decodeSeconds >= 0) {
            int diff = std::abs(nowSeconds - decodeSeconds);
            diff = qMin(diff, 86400 - diff);
            if (diff > freshnessWindowSeconds) {
                continue;
            }
        }
        ++recent;

        QString const message = entry.value(QStringLiteral("message")).toString().trimmed();
        if (message.isEmpty()) {
            continue;
        }
        ++candidates;
        maybeQueuePskReporterSpot(entry,
                                  message,
                                  entry.value(QStringLiteral("isCQ")).toBool(),
                                  entry.value(QStringLiteral("freq")).toString(),
                                  entry.value(QStringLiteral("db")).toString(),
                                  entry.value(QStringLiteral("mode"), m_mode).toString());
    }

    if (decodiumBridgePskDebugEnabled()) {
        static qint64 lastProbeLogMs = 0;
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        if (nowMs - lastProbeLogMs >= 5'000) {
            lastProbeLogMs = nowMs;
            qInfo().noquote() << "[PSKDBG] PSK Reporter decode-list probe source=" << source
                              << "total=" << entries.size()
                              << "inspected=" << inspected
                              << "recent=" << recent
                              << "candidates=" << candidates
                              << "lastTime=" << (lastTimeToken.isEmpty() ? QStringLiteral("<empty>") : lastTimeToken)
                              << "enabled=" << (m_pskReporterEnabled ? 1 : 0);
        }
    }
}

