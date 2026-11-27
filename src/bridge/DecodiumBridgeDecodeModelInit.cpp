#include "DecodiumBridge.h"
#include "DecodiumBridgeDecodeUiFlags.h"
#include "DecodeListModel.h"
#include <utility>

void DecodiumBridge::initialiseDecodeModels()
{
    m_bandActivityModel = new DecodeListModel(this);
    m_bandActivityModel->setObjectName(QStringLiteral("bandActivity"));
    m_fullSpectrumModel = new DecodeListModel(this);
    m_fullSpectrumModel->setObjectName(QStringLiteral("fullSpectrum"));
    m_rxDecodeModel = new DecodeListModel(this);
    m_rxDecodeModel->setObjectName(QStringLiteral("signalRx"));
    // Sync dei model dopo ogni decodeListChanged. Auto-throttled dalla
    // fase 1 (emitDecodeListChangedThrottled max 4 emit/sec).
    connect(this, &DecodiumBridge::decodeListChanged, this, [this]() {
        bool const modelAlreadyUpdated = usingLegacyBackendForTx()
            && std::exchange(m_skipNextLegacyBandModelSnapshot, false);
        if (!modelAlreadyUpdated) {
            rebuildBandActivityModel();
        }
        // In legacy mode Signal RX has its own complete, throttled history and
        // emits rxDecodeListChanged. Avoid rebuilding it a second time for each
        // Band Activity early/final/deep pass.
        if (!usingLegacyBackendForTx()) {
            rebuildRxDecodeModel();
        }
    });
    connect(this, &DecodiumBridge::rxDecodeListChanged, this, [this]() {
        bool const modelAlreadyUpdated = usingLegacyBackendForTx()
            && std::exchange(m_skipNextLegacyRxModelSnapshot, false);
        if (!modelAlreadyUpdated) {
            rebuildRxDecodeModel();
        }
    });
    connect(this, &DecodiumBridge::frequencyChanged,
            this, &DecodiumBridge::displayFrequencyChanged);
    connect(this, &DecodiumBridge::txFrequencyChanged,
            this, &DecodiumBridge::displayFrequencyChanged);
    connect(this, &DecodiumBridge::transmittingChanged,
            this, &DecodiumBridge::displayFrequencyChanged);
    connect(this, &DecodiumBridge::tuningChanged,
            this, &DecodiumBridge::displayFrequencyChanged);
    connect(this, &DecodiumBridge::modeChanged,
            this, &DecodiumBridge::displayFrequencyChanged);
    connect(this, &DecodiumBridge::modeChanged, this, [this]() {
        invalidateLegacyDecodeModelDeltaFastPath();
        invalidateDecodeUiPredicateCaches();
        scheduleDecodeUiRefresh(kDecodeUiRefreshBand
                                | kDecodeUiRefreshRx
                                | kDecodeUiRefreshFullSpectrum);
    });
    // RX model dipende anche da dxCall / rxFrequency / callsign / mode.
    connect(this, &DecodiumBridge::dxCallChanged, this, [this]() {
        m_skipNextLegacyRxModelSnapshot = false;
        m_clearedRxDecodeKeys.clear();
        invalidateDecodeUiPredicateCaches();
        rebuildRxDecodeModel();
    });
    // 1.0.266 (fork-only iu8lmc) — RIMOSSA connect rxFrequencyChanged -> rebuildRxDecodeModel.
    // Il filtro Signal RX non dipende piu' dalla frequenza RX (rimosso il match +/-200Hz),
    // quindi un cambio di rxFrequency non richiede piu' alcun rebuild. Evita rebuild
    // inutili ogni volta che l'utente trascina il marker nel waterfall.
    connect(this, &DecodiumBridge::callsignChanged, this, [this]() {
        m_skipNextLegacyRxModelSnapshot = false;
        m_clearedRxDecodeKeys.clear();
        invalidateDecodeUiPredicateCaches();
        rebuildRxDecodeModel();
    });
    connect(this, &DecodiumBridge::settingValueChanged, this,
        [this](QString const& key, QVariant const&) {
            if (key == QStringLiteral("TXMessagesToRX")
                || key == QStringLiteral("Tx2QSO")) {
                rebuildRxDecodeModel();
            }
            if (key == QStringLiteral("FiltersHideWorkedBand")
                || key == QStringLiteral("FiltersHideWorkedToday")
                || key == QStringLiteral("FiltersHideWorkedEver")
                || key == QStringLiteral("FiltersWorkedTodayIncludesYesterday")
                || key == QStringLiteral("HideB4")
                || key == QStringLiteral("HideWorkedBand")
                || key == QStringLiteral("HideToday")
                || key == QStringLiteral("HideWorkedToday")) {
                invalidateLegacyDecodeModelDeltaFastPath();
                invalidateDecodeUiPredicateCaches();
                // 1.0.434: passa per il throttle 500ms (come il path decode
                // normale) invece di rebuildare i 2 model in diretta. La 1.0.432
                // li chiamava sincroni qui, bypassando il coalescing e aggiungendo
                // rebuild durante i burst -> churn. emitDecodeListChangedThrottled
                // -> decodeListChanged -> rebuild di entrambi i model, coalescato.
                emitDecodeListChangedThrottled();
            }
        });

}
