#include "DecodiumBridge.h"
#include "AdifDocumentInternal.h"
#include "DecodiumLegacyBackend.h"
#include "DecodeListModel.h"
#include "controllers/ActiveStationsModel.hpp"

#include <QDateTime>
#include <QFileInfo>
#include <QTimer>
#include <utility>
#include <cmath>

using namespace decodium::adif_document;

void decodiumBridgeDecodeClearLog(const QString& message);
QString decodiumBridgeDecodeRxClearEntryKey(QVariantMap const& entry);
QString decodiumBridgeMirrorEntryKey(QVariantMap const& entry);

void DecodiumBridge::clearDecodeList()
{
    resetWorldMapDisplayFromCurrentDecodes();

    if (usingLegacyBackendForTx() && !m_ft2LinkTxActive) {
        decodiumBridgeDecodeClearLog("clearDecodeList: delegating band activity clear to legacy backend");
        m_legacyBackend->clearBandActivity();
        m_legacyBandActivityRevision = -1;
        QString const path = legacyAllTxtPath().trimmed();
        QFileInfo const info(path);
        m_legacyAllTxtConsumedPath = info.exists() ? info.absoluteFilePath() : path;
        m_legacyAllTxtConsumedSize = info.exists() ? info.size() : -1;
    }
    m_legacyPrunedBandMirrorKeys.clear();
    m_clearedRxDecodeKeys.clear();
    m_decodeList.clear();
    resetNativeDecodeDedupIndex();
    // 1.0.179 — Smooth Decode Flow: reset coda + ferma drain timer.
    if (m_decodeReleaseTimer) m_decodeReleaseTimer->stop();
    m_pendingDecodeReleaseQueue.clear();
    m_nativeDecodeSecondaryQueue.clear();
    m_nativeDecodeSecondaryPendingKeys.clear();
    clearRemoteActivityCache(true);
    emit decodeListChanged();
    if (m_activeStations) {
        m_activeStations->clear();
    }
}

void DecodiumBridge::clearRxDecodes()
{
    resetWorldMapDisplayFromCurrentDecodes();

    auto rememberClearedRxEntry = [this](QVariantMap const& entry) {
        if (!entryBelongsToCurrentQso(entry)) {
            return;
        }
        QString const key = decodiumBridgeDecodeRxClearEntryKey(entry);
        if (!key.isEmpty()) {
            m_clearedRxDecodeKeys.insert(key);
        }
    };
    for (QVariant const& value : std::as_const(m_rxDecodeList)) {
        rememberClearedRxEntry(value.toMap());
    }
    for (QVariant const& value : std::as_const(m_decodeList)) {
        rememberClearedRxEntry(value.toMap());
    }

    if (usingLegacyBackendForTx()) {
        m_legacyBackend->clearRxFrequency();
        m_legacyRxFrequencyRevision = -1;
        QString const path = legacyAllTxtPath().trimmed();
        QFileInfo const info(path);
        m_legacyAllTxtConsumedPath = info.exists() ? info.absoluteFilePath() : path;
        m_legacyAllTxtConsumedSize = info.exists() ? info.size() : -1;
        m_legacyAllTxtRevisionKey.clear();
        m_legacyClearedRxMirrorKeys.clear();
        for (QVariant const& value : std::as_const(m_rxDecodeList)) {
            m_legacyClearedRxMirrorKeys.insert(decodiumBridgeMirrorEntryKey(value.toMap()));
        }
    }
    m_rxDecodeList.clear();
    m_rxDecodeMirrorKeys.clear();
    if (m_rxDecodeModel) {
        m_rxDecodeModel->setEntries(QVariantList {});
    }
    emit rxDecodeListChanged();
}

void DecodiumBridge::primeLegacyAllTxtCursor()
{
    QString const path = legacyAllTxtPath().trimmed();
    if (path.isEmpty()) {
        m_legacyAllTxtConsumedPath.clear();
        m_legacyAllTxtConsumedSize = -1;
        m_legacyAllTxtRevisionKey.clear();
        return;
    }

    QFileInfo const info(path);
    if (info.exists()) {
        m_legacyAllTxtConsumedPath = info.absoluteFilePath();
        m_legacyAllTxtConsumedSize = info.size();
        m_legacyAllTxtRevisionKey =
            info.absoluteFilePath()
            + QStringLiteral("|")
            + QString::number(info.size())
            + QStringLiteral("|")
            + QString::number(info.lastModified().toMSecsSinceEpoch());
    } else {
        m_legacyAllTxtConsumedPath = path;
        m_legacyAllTxtConsumedSize = -1;
        m_legacyAllTxtRevisionKey = path;
    }
}

void DecodiumBridge::clearDecodeWindowsForModeChange(const QString& previousMode, const QString& nextMode)
{
    decodiumBridgeDecodeClearLog(QStringLiteral("mode-change decode/map reset: %1 -> %2").arg(previousMode, nextMode));
    invalidateDecodeSession();
    invalidateLegacyDecodeModelDeltaFastPath();

    // Cancel every queued UI handoff. The in-flight worker is invalidated by
    // the session above and allowed to finish without blocking the GUI.
    ++m_decodeUiSnapshotGeneration;
    m_decodeUiPendingFlags = 0;
    if (m_decodeUiRefreshTimer) m_decodeUiRefreshTimer->stop();
    if (m_fullSpectrumRefreshTimer) m_fullSpectrumRefreshTimer->stop();
    m_fullSpectrumSnapshotPending = false;
    m_pendingFullSpectrumModelEntries.clear();
    m_pendingFullSpectrumModelKeys.clear();
    invalidateDecodeUiPredicateCaches();
    if (m_decodeReleaseTimer) m_decodeReleaseTimer->stop();
    m_pendingDecodeReleaseQueue.clear();

    resetWorldMapDisplayFromCurrentDecodes();

    m_legacyModeChangeClearedDecodeKeys.clear();
    m_legacyPrunedBandMirrorKeys.clear();
    auto rememberClearedKeys = [this](QVariantList const& entries) {
        for (QVariant const& value : entries) {
            QString const key = decodiumBridgeMirrorEntryKey(value.toMap());
            if (!key.isEmpty()) {
                m_legacyModeChangeClearedDecodeKeys.insert(key);
            }
        }
    };
    rememberClearedKeys(m_decodeList);
    rememberClearedKeys(m_rxDecodeList);

    if (usingLegacyBackendForTx() && legacyBackendAvailable()) {
        rememberClearedKeys(mirrorLegacyDecodeLines(m_legacyBackend->bandActivityLines(), false, m_decodeList));
        rememberClearedKeys(mirrorLegacyDecodeLines(m_legacyBackend->rxFrequencyLines(), true, m_rxDecodeList));
        m_legacyBackend->clearBandActivity();
        m_legacyBackend->clearRxFrequency();
        m_legacyBandActivityRevision = -1;
        m_legacyRxFrequencyRevision = -1;
        m_legacyClearedRxMirrorKeys = m_legacyModeChangeClearedDecodeKeys;
        primeLegacyAllTxtCursor();
    } else {
        m_legacyAllTxtRevisionKey.clear();
        m_legacyClearedRxMirrorKeys.clear();
    }

    m_clearedRxDecodeKeys.clear();
    m_decodeList.clear();
    m_rxDecodeList.clear();
    resetNativeDecodeDedupIndex();
    m_rxDecodeMirrorKeys.clear();
    m_nativeDecodeSecondaryQueue.clear();
    m_nativeDecodeSecondaryPendingKeys.clear();
    clearRemoteActivityCache(true);
    // Do not wait for the next asynchronous snapshot to clear visible rows.
    // A model reset also removes delegates retained by ListView transitions.
    if (m_bandActivityModel) m_bandActivityModel->resetForContextChange();
    if (m_rxDecodeModel) m_rxDecodeModel->resetForContextChange();
    if (m_fullSpectrumModel) m_fullSpectrumModel->resetForContextChange();
    if (m_activeStations) {
        m_activeStations->clear();
    }
    emit decodeListChanged();
    emit rxDecodeListChanged();
}

void DecodiumBridge::clearDecodeWindowsForBandChange(double previousFrequency,
                                                     double nextFrequency,
                                                     const QString& reason)
{
    if (previousFrequency <= 0.0 || nextFrequency <= 0.0) {
        return;
    }

    QString const previousBand =
        freqHzToBandToken(static_cast<quint64>(previousFrequency)).toUpper();
    QString const nextBand =
        freqHzToBandToken(static_cast<quint64>(nextFrequency)).toUpper();
    bool const bandChanged =
        !previousBand.isEmpty()
        && !nextBand.isEmpty()
        && previousBand.compare(nextBand, Qt::CaseInsensitive) != 0;
    bool const largeQsy = std::abs(nextFrequency - previousFrequency) > 100000.0;
    if (!bandChanged && !largeQsy) {
        return;
    }

    decodiumBridgeDecodeClearLog(QStringLiteral("band-change decode/map reset: %1 Hz (%2) -> %3 Hz (%4), reason=%5")
                  .arg(QString::number(previousFrequency, 'f', 0),
                       previousBand.isEmpty() ? QStringLiteral("--") : previousBand,
                       QString::number(nextFrequency, 'f', 0),
                       nextBand.isEmpty() ? QStringLiteral("--") : nextBand,
                       reason));
    invalidateDecodeSession();
    invalidateLegacyDecodeModelDeltaFastPath();

    resetWorldMapDisplayFromCurrentDecodes();

    m_legacyModeChangeClearedDecodeKeys.clear();
    m_legacyPrunedBandMirrorKeys.clear();
    auto rememberClearedKeys = [this](QVariantList const& entries) {
        for (QVariant const& value : entries) {
            QString const key = decodiumBridgeMirrorEntryKey(value.toMap());
            if (!key.isEmpty()) {
                m_legacyModeChangeClearedDecodeKeys.insert(key);
            }
        }
    };
    rememberClearedKeys(m_decodeList);
    rememberClearedKeys(m_rxDecodeList);

    if (usingLegacyBackendForTx() && legacyBackendAvailable()) {
        rememberClearedKeys(mirrorLegacyDecodeLines(m_legacyBackend->bandActivityLines(), false, m_decodeList));
        rememberClearedKeys(mirrorLegacyDecodeLines(m_legacyBackend->rxFrequencyLines(), true, m_rxDecodeList));
        m_legacyBackend->clearBandActivity();
        m_legacyBackend->clearRxFrequency();
        m_legacyBandActivityRevision = -1;
        m_legacyRxFrequencyRevision = -1;
        m_legacyClearedRxMirrorKeys = m_legacyModeChangeClearedDecodeKeys;
        primeLegacyAllTxtCursor();
    } else {
        m_legacyAllTxtRevisionKey.clear();
        m_legacyClearedRxMirrorKeys.clear();
    }

    m_clearedRxDecodeKeys.clear();
    m_decodeList.clear();
    m_rxDecodeList.clear();
    resetNativeDecodeDedupIndex();
    m_rxDecodeMirrorKeys.clear();
    if (m_decodeReleaseTimer) {
        m_decodeReleaseTimer->stop();
    }
    m_pendingDecodeReleaseQueue.clear();
    m_nativeDecodeSecondaryQueue.clear();
    m_nativeDecodeSecondaryPendingKeys.clear();
    clearRemoteActivityCache(true);
    if (m_activeStations) {
        m_activeStations->clear();
    }
    emit decodeListChanged();
    emit rxDecodeListChanged();
}

