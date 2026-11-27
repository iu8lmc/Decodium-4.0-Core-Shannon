#include "DecodiumBridge.h"
#include "DecodiumProfileSettings.h"
#include "DecodiumLegacyBackend.h"
#include "Q65DopplerTracker.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QSettings>

bool decodiumBridgeEmbeddedLegacyRigEnabled(const QString& backend, bool legacyTxBackendRequested);

void DecodiumBridge::setCatBackend(const QString& v)
{
    QString normalized = v.trimmed().toLower();
    if (normalized != QStringLiteral("hamlib")
        && normalized != QStringLiteral("tci")
        && normalized != QStringLiteral("cat4om")
        && normalized != QStringLiteral("omnirig")) {
        normalized = QStringLiteral("hamlib");
    }
    if (m_catBackend == normalized) return;
    if (m_q65Doppler) m_q65Doppler->setEnabled(false);
    if (sstvTxActive()) {
        cancelSstvTx();
        emit errorMessage(tr("SSTV TX is releasing PTT: change the CAT backend again after transmission has stopped"));
        return;
    }
    halt();
    // Disconnetti TUTTI i manager per difesa: anche quelli non segnati come
    // "connected" possono tenere risorse (es. QAxObject di OmniRig o porta
    // seriale QSerialPort del backend nativo). Evita conflitti "serial port
    // already open" quando si alterna fra omnirig, hamlib e native.
    if (m_cat4OmCat)  m_cat4OmCat->disconnectRig();
    if (m_omniRigCat) m_omniRigCat->disconnectRig();
    if (m_hamlibCat)  m_hamlibCat->disconnectRig();
    // Lasciamo girare gli eventi in modo che i deleteLater() dei QAxObject
    // OmniRig e la chiusura della porta seriale del backend nativo si
    // completino prima di cedere il controllo al nuovo backend.
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    m_catConnected = false;
    emit catConnectedChanged();
    refreshPskReporterLocalStation();
    updateRigTelemetry(0.0, 0.0);
    if (!m_lastCatError.isEmpty()) {
        m_lastCatError.clear();
        emit lastCatErrorChanged();
    }
    m_rttyRigModeState.clear();
    m_catBackend = normalized;
    emit catBackendChanged();
    emit catManagerChanged();
    if (legacyBackendAvailable()) {
        m_legacyBackend->setRigControlEnabled(
            decodiumBridgeEmbeddedLegacyRigEnabled(m_catBackend, legacyTxBackendRequested()));
    }
    refreshPskReporterLocalStation();
    QSettings s2(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s2);
    s2.setValue("catBackend", m_catBackend);
}

