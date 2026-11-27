#include "DecodiumBridge.h"
#include <QTimer>

void decodiumBridgeRttyRigModeLog(const QString& message);
bool decodiumBridgeIsHamlibFamilyBackend(QString const& backend);
bool decodiumBridgeDecoPortIsCat(const QString& backend);

QString DecodiumBridge::rttyCatContext() const
{
    if (!m_catConnected || m_decoPortUseRemote || decodiumBridgeDecoPortIsCat(m_catBackend)) return {};
    return m_catBackend + QChar('|') + m_catRigName;
}

void DecodiumBridge::applyRttyRigMode(const QString& reason)
{
    if (m_mode.compare(QStringLiteral("RTTY"), Qt::CaseInsensitive) != 0)
        return;

    QString const rigMode = rttyRigMode();
    if (rigMode.isEmpty())
        return;

    // Durante l'assestamento di un QSY il comando andrebbe perso: si rimanda.
    if (hamlibCatFrequencySettleActive(reason)) {
        decodiumBridgeRttyRigModeLog(QStringLiteral("RTTY: modo radio rimandato durante il QSY (%1): %2")
                      .arg(reason, rigMode));
        QTimer::singleShot(600, this, [this, reason, rigMode]() {
            if (m_mode.compare(QStringLiteral("RTTY"), Qt::CaseInsensitive) == 0)
                applyRttyRigMode(reason + QStringLiteral("-ritardato"));
        });
        return;
    }

    if (!m_catMode.trimmed().isEmpty()
        && m_catMode.compare(rigMode, Qt::CaseInsensitive) == 0) {
        return;   // gia' nel modo giusto
    }

    if (m_catBackend == QStringLiteral("cat4om")) {
        if (m_cat4OmCat && m_cat4OmCat->connected()) {
            decodiumBridgeRttyRigModeLog(QStringLiteral("RTTY: modo radio (%1): cat4om -> %2").arg(reason, rigMode));
            m_rttyRigModeState.overrideRequested(rttyCatContext(), rigMode);
            m_cat4OmCat->setRigMode(rigMode);
        }
        return;
    }
    if (decodiumBridgeIsHamlibFamilyBackend(m_catBackend)) {
        if (m_hamlibCat && m_hamlibCat->connected()) {
            decodiumBridgeRttyRigModeLog(QStringLiteral("RTTY: modo radio (%1): %2 -> %3")
                          .arg(reason, m_catBackend, rigMode));
            m_rttyRigModeState.overrideRequested(rttyCatContext(), rigMode);
            m_hamlibCat->setRigMode(rigMode);
        }
        return;
    }
    if (m_catBackend == QStringLiteral("omnirig") && m_omniRigCat && m_omniRigCat->connected()) {
        decodiumBridgeRttyRigModeLog(QStringLiteral("RTTY: modo radio (%1): omnirig -> %2").arg(reason, rigMode));
        m_rttyRigModeState.overrideRequested(rttyCatContext(), rigMode);
        m_omniRigCat->setRigMode(rigMode);
    }
}

