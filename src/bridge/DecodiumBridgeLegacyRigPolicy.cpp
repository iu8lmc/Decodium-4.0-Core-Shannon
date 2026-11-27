#include "DecodiumLegacyBackend.h"
#include <QString>

static inline bool useLegacyRigControlFallback(DecodiumLegacyBackend* legacy, const QString& backend)
{
    // Disabilitato su tutte le piattaforme — usa sempre il CAT manager QML
    // Il legacy fallback causava problemi su Windows (Bug #1: CAT auto-connect non funziona)
    Q_UNUSED(legacy);
    Q_UNUSED(backend);
    return false;
}

bool decodiumBridgeLegacyRigControlFallback(DecodiumLegacyBackend* legacy, const QString& backend)
{
    return useLegacyRigControlFallback(legacy, backend);
}

static inline bool embeddedLegacyRigControlEnabledForBackend(const QString& backend,
                                                             bool legacyTxBackendRequested)
{
    if (legacyTxBackendRequested) {
        return false;
    }

    // OmniRig data mode is defined by each rig INI.  Let the modern OmniRig
    // manager own the COM object so the embedded legacy backend cannot issue a
    // second PM_DIG_U/SetMode request that some Kenwood INI files map to FSK
    // or plain USB instead of USB-D.
    QString const normalized = backend.trimmed().toLower();
    return normalized != QStringLiteral("omnirig")
        && normalized != QStringLiteral("cat4om");
}

bool decodiumBridgeEmbeddedLegacyRigEnabled(const QString& backend, bool requested)
{
    return embeddedLegacyRigControlEnabledForBackend(backend, requested);
}

static inline bool legacyOwnsRigControl(DecodiumLegacyBackend* legacy)
{
    return legacy && legacy->available() && legacy->rigControlEnabled();
}

bool decodiumBridgeLegacyOwnsRigControl(DecodiumLegacyBackend* legacy)
{
    return legacyOwnsRigControl(legacy);
}

