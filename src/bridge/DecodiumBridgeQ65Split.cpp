#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodiumProfileSettings.h"
#include "Q65DopplerTracker.h"
#include "SatelliteTrackingService.h"
#include "Transceiver/TransceiverFactory.hpp"
#include <QDateTime>
#include <QSettings>

QString decodiumBridgeNormalizedCatSplitMode(const QString&);
bool decodiumBridgeLegacyOwnsRigControl(DecodiumLegacyBackend*);
void decodiumBridgeAudioLevelLog(const QString&);
#define normalizedCatSplitMode decodiumBridgeNormalizedCatSplitMode
#define legacyOwnsRigControl decodiumBridgeLegacyOwnsRigControl
#define bridgeLog decodiumBridgeAudioLevelLog

static QVariant legacySplitModeSettingValue(QString const& value)
{
    QString const normalized = normalizedCatSplitMode(value);
    if (normalized == QStringLiteral("rig")) {
        return QVariant::fromValue(TransceiverFactory::split_mode_rig);
    }
    if (normalized == QStringLiteral("emulate")) {
        return QVariant::fromValue(TransceiverFactory::split_mode_emulate);
    }
    return QVariant::fromValue(TransceiverFactory::split_mode_none);
}

double DecodiumBridge::catSplitTxPttDialFrequencyHz()
{
    if (m_q65Doppler && (m_q65Doppler->enabled() || m_q65Doppler->applied())) {
        refreshQ65Doppler();
        // Unlike UI/log dials the PTT transaction takes the physical CAT/IF
        // frequency. Keep the transverter and calibration mapping exactly once.
        if (m_q65Doppler->applied())
            return applyFrequencyCalibration(m_q65Doppler->txDialHz());
    }
    return catSplitTxDialFrequencyHz();
}

void DecodiumBridge::refreshQ65Doppler()
{
    if (!m_q65Doppler || m_shuttingDown) return;
    Q65DopplerTracker::Context context;
    context.mode = m_mode;
    context.myGrid = m_grid;
    context.dxGrid = m_dxGrid;
    context.nominalHz = m_frequency;
    context.periodMs = effectivePeriodMsForMode(m_mode);
    context.connected = m_catConnected;
    context.supported = m_catBackend == QStringLiteral("hamlib") && m_hamlibCat;
    context.split = context.supported
        && normalizedCatSplitMode(m_hamlibCat->splitMode()) != QStringLiteral("none");
    context.monitoring = m_monitoring;
    context.transmitting = m_transmitting || m_tuning || m_pttPending || m_pttOnDispatched
        || (context.supported && m_hamlibCat->pttActive());
    context.conflict = (usingLegacyBackendForTx() && !shouldUseBridgeAudioForLegacyDigitalTx())
        || (legacyOwnsRigControl(m_legacyBackend) && m_legacyBackend->catConnected())
        || hamlibCatFrequencySettleActive(QStringLiteral("eme"))
        || m_decoPortUseRemote || rtlSdrEnabled() || sstvRxRequested() || sstvTxActive()
        || (m_satelliteTracking && m_satelliteTracking->dopplerTracking());
    auto const action = m_q65Doppler->update(context, QDateTime::currentDateTimeUtc());
    if (!action.tune || !context.supported || !m_hamlibCat->connected()) return;
    double const rx = applyFrequencyCalibration(action.rxHz);
    double const logicalTx = action.restore ? catSplitTxDialFrequencyHz() : action.txHz;
    double const tx = logicalTx > 0.0 ? applyFrequencyCalibration(logicalTx) : 0.0;
    if (rx <= 0.0 || (logicalTx > 0.0 && tx <= 0.0)) {
        m_q65Doppler->disconnected();
        emit errorMessage(tr("Invalid CAT frequency after EME/transverter correction"));
        return;
    }
    m_hamlibCat->setRigFrequency(rx);
    m_hamlibCat->setRigTxFrequency(tx);
    bridgeLog(QStringLiteral("[EME] %1 nominal=%2 rx=%3 tx=%4 method=%5")
                  .arg(action.restore ? QStringLiteral("restore") : QStringLiteral("tracking"))
                  .arg(m_frequency, 0, 'f', 0).arg(rx, 0, 'f', 0).arg(tx, 0, 'f', 0)
                  .arg(m_q65Doppler->method()));
    emit displayFrequencyChanged();
}

void DecodiumBridge::syncCatSplitModeToLegacy(const QString& mode, const QString& reason)
{
    QString const normalized = normalizedCatSplitMode(mode);
    QVariant const legacyValue = legacySplitModeSettingValue(normalized);

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.beginGroup(QStringLiteral("Transceiver"));
    settings.setValue(QStringLiteral("splitMode"), normalized);
    settings.endGroup();
    settings.setValue(QStringLiteral("SplitMode"), legacyValue);
    settings.sync();
    syncSettingToLegacyIni(QStringLiteral("SplitMode"), legacyValue);

    if (legacyBackendAvailable()) {
        m_legacyBackend->setSplitMode(normalized);
    }

    bridgeLog(QStringLiteral("CAT split mode synced to legacy (%1): %2")
                  .arg(reason, normalized));
}

#undef normalizedCatSplitMode
#undef legacyOwnsRigControl
#undef bridgeLog
