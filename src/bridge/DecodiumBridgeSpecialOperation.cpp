#include "DecodiumBridge.h"
#include "DecodiumLegacyBackend.h"
#include "DecodiumProfileSettings.h"
#include "BridgeConfigConstants.h"
#include "SpecialOperationBounds.h"
#include "Sequencer/QsoSequencerRules.hpp"
#include <QScopedValueRollback>
#include <QSettings>

void decodiumBridgeSpecialOpLog(const QString&);

int DecodiumBridge::ncontest() const { return m_ncontest; }
void DecodiumBridge::setNcontest(int v) { setSpecialOperationActivity(v); }

void DecodiumBridge::setFoxMode(bool v)
{
    if (v) {
        setSpecialOperationActivity(kSpecialOpFox);
    } else if (m_specialOperationActivity == kSpecialOpFox
               || m_deferredSpecialOperationActivity == kSpecialOpFox) {
        setSpecialOperationActivity(kSpecialOpNone);
    }
}

void DecodiumBridge::setHoundMode(bool v)
{
    if (v) {
        setSpecialOperationActivity(kSpecialOpHound);
    } else if (m_specialOperationActivity == kSpecialOpHound
               || m_deferredSpecialOperationActivity == kSpecialOpHound) {
        setSpecialOperationActivity(kSpecialOpNone);
    }
}

bool DecodiumBridge::specialOperationRequiresLegacyTx() const
{
    return m_specialOperationActivity == kSpecialOpFox
        || m_specialOperationActivity == kSpecialOpHound;
}


bool DecodiumBridge::legacyTxBackendRequested() const
{
    return m_useLegacyTxBackend || m_forceLegacyTxForSpecialOp;
}

void DecodiumBridge::updateSpecialOperationFromLegacy(int activity)
{
    activity = qBound(kSpecialOpNone, activity, kSpecialOpMax);
    bool changed = false;
    if (m_specialOperationActivity != activity) {
        m_specialOperationActivity = activity;
        changed = true;
        emit specialOperationActivityChanged();
    }
    if (m_ncontest != activity) {
        m_ncontest = activity;
        emit ncontestChanged();
    }
    bool const fox = activity == kSpecialOpFox;
    bool const hound = activity == kSpecialOpHound;
    if (!m_useLegacyTxBackend) {
        m_forceLegacyTxForSpecialOp = fox || hound;
    }
    if (m_foxMode != fox) {
        m_foxMode = fox;
        emit foxModeChanged();
    }
    if (m_houndMode != hound) {
        m_houndMode = hound;
        emit houndModeChanged();
    }
    if (changed) {
        udpSendStatus();
    }
}

void DecodiumBridge::syncSpecialOperationToLegacyBackend()
{
    if (!legacyBackendAvailable()) {
        return;
    }

    // Legacy setters emit status changes synchronously. Never read an
    // intermediate activity back into the bridge between these two writes.
    QScopedValueRollback<bool> stateGuard(m_syncingLegacyBackendState, true);
    int const requestedActivity = m_specialOperationActivity;
    bool const superFox = getSetting(QStringLiteral("SuperFox"), false).toBool();
    m_legacyBackend->setSuperFoxEnabled(superFox);
    m_legacyBackend->setSpecialOperationActivity(requestedActivity);
}

bool DecodiumBridge::specialOperationTransitionBlockedByQsoFinalisation() const
{
    return decodium::seq::specialOperationTransitionRequiresDeferral(
        m_logAfterOwn73, m_logPromptOpen);
}

bool DecodiumBridge::applyDeferredSpecialOperationActivityIfReady()
{
    if (m_deferredSpecialOperationActivity < kSpecialOpNone) {
        return false;
    }
    if (m_transmitting || m_tuning || specialOperationTransitionBlockedByQsoFinalisation()) {
        return true;
    }

    int const activity = m_deferredSpecialOperationActivity;
    m_deferredSpecialOperationActivity = -1;
    emit specialOperationChangePendingChanged();
    decodiumBridgeSpecialOpLog(QStringLiteral("SpecialOp deferred transition applying after QSO finalisation: activity=%1")
                  .arg(activity));
    setSpecialOperationActivity(activity);
    return true;
}

void DecodiumBridge::setSpecialOperationActivity(int activity)
{
    if (m_transmitting || m_tuning) {
        emit warningRaised(QStringLiteral("Fox/Hound"),
                           QStringLiteral("Stop TX/Tune before changing operating activity"), QString());
        return;
    }
    activity = qBound(kSpecialOpNone, activity, kSpecialOpMax);
    if (specialOperationTransitionBlockedByQsoFinalisation()) {
        if (activity == m_specialOperationActivity) {
            if (m_deferredSpecialOperationActivity >= kSpecialOpNone) {
                decodiumBridgeSpecialOpLog(QStringLiteral("SpecialOp deferred transition cancelled during QSO finalisation: activity=%1")
                              .arg(m_deferredSpecialOperationActivity));
                m_deferredSpecialOperationActivity = -1;
                emit specialOperationChangePendingChanged();
            }
            return;
        }

        if (m_deferredSpecialOperationActivity != activity) {
            decodiumBridgeSpecialOpLog(QStringLiteral("SpecialOp transition deferred until QSO finalisation: requested=%1 current=%2 own73=%3 prompt=%4")
                          .arg(activity)
                          .arg(m_specialOperationActivity)
                          .arg(m_logAfterOwn73 ? 1 : 0)
                          .arg(m_logPromptOpen ? 1 : 0));
            m_deferredSpecialOperationActivity = activity;
            emit specialOperationChangePendingChanged();
        }
        emit statusMessage(QStringLiteral("Operating activity will change after the final 73 and QSO log"));
        return;
    }

    QScopedValueRollback<bool> stateGuard(m_syncingLegacyBackendState, true);
    bool const wasLegacyTx = usingLegacyBackendForTx();
    bool const wasLegacyRx = usingLegacyBackendForRx();
    bool const wasForced = m_forceLegacyTxForSpecialOp;
    bool const wantLegacyTx = activity == kSpecialOpFox || activity == kSpecialOpHound;

    updateSpecialOperationFromLegacy(activity);

    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
    s.setValue(QStringLiteral("SelectedActivity"), activity);
    s.setValue(QStringLiteral("SpecialOpActivity"), activity != kSpecialOpNone);
    s.sync();
    syncSettingToLegacyIni(QStringLiteral("SelectedActivity"), activity);
    syncSettingToLegacyIni(QStringLiteral("SpecialOpActivity"), activity != kSpecialOpNone);
    emit settingValueChanged(QStringLiteral("SelectedActivity"), activity);
    emit settingValueChanged(QStringLiteral("SpecialOpActivity"), activity != kSpecialOpNone);

    m_forceLegacyTxForSpecialOp = wantLegacyTx;

    if (wantLegacyTx) {
        if (!ensureLegacyBackendAvailable()) {
            m_forceLegacyTxForSpecialOp = wasForced;
            updateSpecialOperationFromLegacy(kSpecialOpNone);
            QSettings resetSettings(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
            decodium::beginActiveSettingsProfile(resetSettings);
            resetSettings.setValue(QStringLiteral("SelectedActivity"), kSpecialOpNone);
            resetSettings.setValue(QStringLiteral("SpecialOpActivity"), false);
            resetSettings.sync();
            syncSettingToLegacyIni(QStringLiteral("SelectedActivity"), kSpecialOpNone);
            syncSettingToLegacyIni(QStringLiteral("SpecialOpActivity"), false);
            emit errorMessage(QStringLiteral("Fox/Hound non disponibile: backend legacy non avviato"));
            return;
        }
        syncLegacyBackendDialogState();
        shutdownUdpMessageClient();
        if (m_monitoring && !m_transmitting && !m_tuning
            && (!wasLegacyRx || !m_legacyBackend->monitoring())) {
            migrateActiveMonitoringToLegacyBackend();
        }
        decodiumBridgeSpecialOpLog(QStringLiteral("SpecialOp enabled through legacy backend: activity=%1").arg(activity));
        return;
    }

    if (legacyBackendAvailable()) {
        syncSpecialOperationToLegacyBackend();
    }

    if (wasLegacyTx && wasForced && !m_useLegacyTxBackend) {
        if (m_legacyBackend) {
            m_legacyBackend->setMonitoring(false);
        }
        initUdpMessageClient();
        if (m_monitoring && !m_transmitting && !m_tuning) {
            bool const monitoringSnapshot = m_monitoring;
            m_monitoring = false;
            if (monitoringSnapshot) {
                emit monitoringChanged();
            }
            startRx();
        }
    }

    decodiumBridgeSpecialOpLog(QStringLiteral("SpecialOp disabled or set to non-DXpedition activity: activity=%1").arg(activity));
}

