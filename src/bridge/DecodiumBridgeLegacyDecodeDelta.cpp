#include "DecodiumBridge.h"
#include "DecodeListModel.h"

#include <QDateTime>
#include <QTimer>
#include <cmath>
#include <utility>

static QVariantMap decodeDeltaBoundaryEntry(DecodeListModel const* model,
                                            bool first)
{
    if (!model) return {};
    int const count = model->count();
    int index = first ? 0 : count - 1;
    int const step = first ? 1 : -1;
    while (index >= 0 && index < count) {
        QVariantMap const entry = model->entry(index);
        if (!entry.value(QStringLiteral("isSeparator")).toBool()) return entry;
        index += step;
    }
    return {};
}

static QVariantMap decodeDeltaListBoundaryEntry(QVariantList const& entries,
                                                bool first)
{
    int index = first ? 0 : entries.size() - 1;
    int const step = first ? 1 : -1;
    while (index >= 0 && index < entries.size()) {
        QVariantMap const entry = entries.at(index).toMap();
        if (!entry.value(QStringLiteral("isSeparator")).toBool()) return entry;
        index += step;
    }
    return {};
}

static bool decodeDeltaPeriodChanged(QVariantMap const& previous,
                                     QVariantMap const& next)
{
    if (previous.isEmpty() || next.isEmpty()) return false;
    QString const previousTime = previous.value(QStringLiteral("time")).toString();
    QString const nextTime = next.value(QStringLiteral("time")).toString();
    if (!previousTime.isEmpty() && !nextTime.isEmpty()) {
        return previousTime != nextTime;
    }
    qint64 const previousTs = static_cast<qint64>(
        previous.value(QStringLiteral("timestamp")).toULongLong());
    qint64 const nextTs = static_cast<qint64>(
        next.value(QStringLiteral("timestamp")).toULongLong());
    return previousTs > 0 && nextTs > 0
        && std::abs(nextTs - previousTs) > 1500;
}

static QVariantList addDecodeDeltaBoundarySeparator(
    QVariantList entries,
    DecodeListModel const* model,
    bool prepend,
    bool enabled)
{
    if (!enabled || entries.isEmpty() || !model || model->count() <= 0) {
        return entries;
    }

    QVariantMap const previous = prepend
        ? decodeDeltaListBoundaryEntry(entries, false)
        : decodeDeltaBoundaryEntry(model, false);
    QVariantMap const next = prepend
        ? decodeDeltaBoundaryEntry(model, true)
        : decodeDeltaListBoundaryEntry(entries, true);
    if (!decodeDeltaPeriodChanged(previous, next)) return entries;

    QVariantMap separator;
    separator.insert(QStringLiteral("isSeparator"), true);
    separator.insert(QStringLiteral("time"),
                     next.value(QStringLiteral("time")));
    separator.insert(QStringLiteral("timestamp"),
                     next.value(QStringLiteral("timestamp")));
    if (prepend) {
        entries.append(separator);
    } else {
        entries.prepend(separator);
    }
    return entries;
}

void DecodiumBridge::invalidateLegacyDecodeModelDeltaFastPath()
{
    m_skipNextLegacyBandModelSnapshot = false;
    m_skipNextLegacyRxModelSnapshot = false;
    m_pendingLegacyFullSpectrumDelta.clear();
    if (m_legacyFullSpectrumDeltaTimer) {
        m_legacyFullSpectrumDeltaTimer->stop();
    }
}

bool DecodiumBridge::applyLegacyBandModelDelta(QVariantList const& entries)
{
    if (!m_bandActivityModel || !m_fullSpectrumModel) return false;
    // Non sovrapporre un delta a uno snapshot integrale ancora in volo. Il
    // segnale coalesciuto eseguira un nuovo snapshot contenente anche le righe
    // appena aggiunte.
    if (m_decodeUiSnapshotInFlight || m_decodeUiPendingFlags != 0
        || m_fullSpectrumSnapshotPending) {
        return false;
    }

    QVariantList filtered = filterEntriesForBandActivity(entries);
    QVariantList const fullSpectrumDelta = filtered;
    filtered = addDecodeDeltaBoundarySeparator(
        std::move(filtered), m_bandActivityModel, m_decodeNewestFirst,
        m_decodeShowPeriodSeparator);
    if (!filtered.isEmpty()) {
        m_bandActivityModel->appendEntriesBudgeted(
            filtered, m_decodeNewestFirst, qMax(8, decodeUiRowsPerCycle() / 2));
    }
    queueLegacyFullSpectrumModelDelta(fullSpectrumDelta);
    m_skipNextLegacyBandModelSnapshot = true;
    return true;
}

bool DecodiumBridge::applyLegacyRxModelDelta(QVariantList const& entries)
{
    if (!m_rxDecodeModel) return false;
    if (m_decodeUiSnapshotInFlight || m_decodeUiPendingFlags != 0) return false;

    QVariantList filtered = filterEntriesForRxDecode(entries);
    filtered = addDecodeDeltaBoundarySeparator(
        std::move(filtered), m_rxDecodeModel, false,
        m_decodeShowPeriodSeparator);
    if (!filtered.isEmpty()) {
        m_rxDecodeModel->appendEntriesBudgeted(
            filtered, false, qMax(6, decodeUiRowsPerCycle() / 3));
    }
    m_skipNextLegacyRxModelSnapshot = true;
    return true;
}

void DecodiumBridge::queueLegacyFullSpectrumModelDelta(
    QVariantList const& entries)
{
    if (entries.isEmpty()) return;
    if (m_decodeNewestFirst) {
        QVariantList merged = entries;
        merged.reserve(entries.size() + m_pendingLegacyFullSpectrumDelta.size());
        merged += m_pendingLegacyFullSpectrumDelta;
        m_pendingLegacyFullSpectrumDelta = std::move(merged);
    } else {
        m_pendingLegacyFullSpectrumDelta += entries;
    }

    if (!m_legacyFullSpectrumDeltaTimer) {
        m_legacyFullSpectrumDeltaTimer = new QTimer(this);
        m_legacyFullSpectrumDeltaTimer->setSingleShot(true);
        connect(m_legacyFullSpectrumDeltaTimer, &QTimer::timeout,
                this, &DecodiumBridge::applyLegacyFullSpectrumModelDelta);
    }
    qint64 const remainingMs = qMax<qint64>(
        0, m_decodeUiBurstUntilMs - QDateTime::currentMSecsSinceEpoch());
    m_legacyFullSpectrumDeltaTimer->start(
        static_cast<int>(qMax<qint64>(100, remainingMs + 100)));
}

void DecodiumBridge::applyLegacyFullSpectrumModelDelta()
{
    if (m_pendingLegacyFullSpectrumDelta.isEmpty()
        || !m_fullSpectrumModel) {
        return;
    }
    if (decodeUiBurstActive()) {
        qint64 const remainingMs = qMax<qint64>(
            0, m_decodeUiBurstUntilMs - QDateTime::currentMSecsSinceEpoch());
        m_legacyFullSpectrumDeltaTimer->start(
            static_cast<int>(remainingMs + 100));
        return;
    }

    QVariantList delta = std::move(m_pendingLegacyFullSpectrumDelta);
    m_pendingLegacyFullSpectrumDelta.clear();
    delta = addDecodeDeltaBoundarySeparator(
        std::move(delta), m_fullSpectrumModel, m_decodeNewestFirst,
        m_decodeShowPeriodSeparator);
    m_fullSpectrumModel->appendEntriesBudgeted(
        delta, m_decodeNewestFirst, qMax(6, decodeUiRowsPerCycle() / 3));
}

