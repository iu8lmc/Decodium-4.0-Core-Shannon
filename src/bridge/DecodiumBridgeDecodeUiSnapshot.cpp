#include "DecodiumBridge.h"
#include "DecodiumBridgeDecodeUiFlags.h"
#include "DecodiumBridgeTraceScope.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#include <utility>

QString decodiumBridgeMirrorEntryKey(const QVariantMap&);
bool decodiumBridgeDecodeTelemetryOnly(QString const&);
void decodiumBridgeNormalizeDecodeEntries(QVariantList&, int, QString const&);
QString decodiumBridgeDecodeRxClearEntryKey(QVariantMap const&);
void decodiumBridgeCoalesceRxPaneTxRows(QVariantList&, QString const&);

namespace {
struct DecodeUiClassifiedSource
{
    QVariantList entries;
    QVector<quint8> ghostRejected;
    QVector<quint8> rxMembership;
};

struct DecodeUiSnapshotJob
{
    DecodeUiClassifiedSource bandSource;
    DecodeUiClassifiedSource rxSource;
    QSet<QString> clearedRxKeys;
    QString mode;
    quint64 generation {0};
    int flags {0};
    bool includeBandSourceInRx {false};
    decodium::decode_ui::WorkedFilterOptions workedFilters;
    bool preserveWorkedCq {false};
    bool hideTelemetry {true};
    bool showTxMessagesInRx {true};
    bool newestFirst {false};
    bool showPeriodSeparators {true};
};

struct DecodeUiSnapshotResult
{
    DecodeListModel::PreparedSnapshot bandEntries;
    DecodeListModel::PreparedSnapshot rxEntries;
    DecodeListModel::PreparedSnapshot fullSpectrumEntries;
    quint64 generation {0};
    int flags {0};
    qint64 workerMs {0};
};

static QString decodeUiPredicateCacheKey(QVariantMap const& entry)
{
    QString key = decodiumBridgeMirrorEntryKey(entry);
    key.reserve(key.size() + 160);
    key += QLatin1Char('|') + entry.value(QStringLiteral("mode")).toString();
    key += QLatin1Char('|') + entry.value(QStringLiteral("db")).toString();
    key += QLatin1Char('|') + entry.value(QStringLiteral("aptype")).toString();
    key += QLatin1Char('|') + entry.value(QStringLiteral("quality")).toString();
    key += QLatin1Char('|') + entry.value(QStringLiteral("fromCall")).toString();
    key += QLatin1Char('|') + entry.value(QStringLiteral("dxCallsign")).toString();
    key += QLatin1Char('|') + entry.value(QStringLiteral("dxDistance")).toString();
    key += QLatin1Char('|') + (entry.value(QStringLiteral("isMyCall")).toBool() ? QLatin1String("1") : QLatin1String("0"));
    key += QLatin1Char('|') + (entry.value(QStringLiteral("isB4")).toBool() ? QLatin1String("1") : QLatin1String("0"));
    key += QLatin1Char('|') + (entry.value(QStringLiteral("dxIsWorked")).toBool() ? QLatin1String("1") : QLatin1String("0"));
    key += QLatin1Char('|') + (entry.value(QStringLiteral("dxIsWorkedBand")).toBool() ? QLatin1String("1") : QLatin1String("0"));
    key += QLatin1Char('|') + (entry.value(QStringLiteral("dxIsWorkedToday")).toBool() ? QLatin1String("1") : QLatin1String("0"));
    key += QLatin1Char('|') + (entry.value(QStringLiteral("dxIsWorkedYesterday")).toBool() ? QLatin1String("1") : QLatin1String("0"));
    key += QLatin1Char('|') + (entry.value(QStringLiteral("dxIsWorkedEver")).toBool() ? QLatin1String("1") : QLatin1String("0"));
    key += QLatin1Char('|') + (entry.value(QStringLiteral("forceRxPane")).toBool() ? QLatin1String("1") : QLatin1String("0"));
    return key;
}

static bool decodeUiEntryPassesBandFilters(QVariantMap const& entry,
                                           bool ghostRejected,
                                           DecodeUiSnapshotJob const& job)
{
    if (entry.isEmpty() || ghostRejected) return false;
    bool const isTxEntry = entry.value(QStringLiteral("isTx")).toBool()
        || entry.value(QStringLiteral("db")).toString().trimmed()
               .compare(QStringLiteral("TX"), Qt::CaseInsensitive) == 0;
    if (!isTxEntry
        && decodium::decode_ui::isHiddenByWorkedFilters(
            entry, job.workedFilters, job.preserveWorkedCq)) {
        return false;
    }
    if (!job.hideTelemetry) return true;
    QString const display = entry.value(QStringLiteral("displayMessage")).toString();
    QString const fallback = entry.value(QStringLiteral("message")).toString();
    return !decodiumBridgeDecodeTelemetryOnly(display.isEmpty() ? fallback : display);
}

static void injectDecodeUiPeriodSeparators(QVariantList& entries, bool enabled)
{
    if (!enabled || entries.size() <= 1) return;

    QVariantList withSeparators;
    withSeparators.reserve(entries.size() * 11 / 10 + 1);
    QString previousPeriod;
    qint64 previousTimestamp = 0;
    for (QVariant const& value : std::as_const(entries)) {
        QVariantMap const entry = value.toMap();
        QString const time = entry.value(QStringLiteral("time")).toString();
        qint64 const timestamp =
            static_cast<qint64>(entry.value(QStringLiteral("timestamp")).toULongLong());
        bool newPeriod = false;
        if (!time.isEmpty()) {
            newPeriod = !previousPeriod.isEmpty() && time != previousPeriod;
        } else {
            newPeriod = previousTimestamp > 0 && timestamp > 0
                && (timestamp - previousTimestamp) > 1500;
        }
        if (newPeriod) {
            QVariantMap separator;
            separator.insert(QStringLiteral("isSeparator"), true);
            separator.insert(QStringLiteral("time"), time);
            separator.insert(QStringLiteral("timestamp"),
                             QVariant::fromValue(static_cast<qulonglong>(timestamp)));
            withSeparators.append(separator);
        }
        if (!time.isEmpty()) previousPeriod = time;
        if (timestamp > 0) previousTimestamp = timestamp;
        withSeparators.append(entry);
    }
    entries = withSeparators;
}

static DecodeUiSnapshotResult prepareDecodeUiSnapshot(DecodeUiSnapshotJob job)
{
    QElapsedTimer elapsed;
    elapsed.start();

    DecodeUiSnapshotResult result;
    result.generation = job.generation;
    result.flags = job.flags;

    if (job.flags & (kDecodeUiRefreshBand | kDecodeUiRefreshFullSpectrum)) {
        QVariantList bandEntries;
        bandEntries.reserve(job.bandSource.entries.size());
        for (int i = 0; i < job.bandSource.entries.size(); ++i) {
            QVariantMap const entry = job.bandSource.entries.at(i).toMap();
            bool const ghost = job.bandSource.ghostRejected.value(i, 0) != 0;
            if (decodeUiEntryPassesBandFilters(entry, ghost, job)) {
                bandEntries.append(entry);
            }
        }
        // Full Spectrum and Band Activity snapshots can originate from the
        // legacy backend as a complete widget mirror.  Coalesce and sort the
        // copied data here, on the dedicated snapshot worker, rather than
        // reordering the bridge's live lists on every decoder callback.
        decodiumBridgeNormalizeDecodeEntries(bandEntries, 1500, job.mode);
        if (job.newestFirst) {
            std::reverse(bandEntries.begin(), bandEntries.end());
        }
        injectDecodeUiPeriodSeparators(bandEntries,
                                       job.showPeriodSeparators);
        result.bandEntries = DecodeListModel::prepareSnapshot(bandEntries);
        if (job.flags & kDecodeUiRefreshFullSpectrum) {
            // Qt containers are implicitly shared: this is a cheap handoff and
            // preserves a separately movable target for the delayed panel.
            result.fullSpectrumEntries = result.bandEntries;
        }
    }

    if (job.flags & kDecodeUiRefreshRx) {
        QSet<QString> seen;
        QVariantList rxEntries;
        auto appendSource = [&rxEntries, &job, &seen](DecodeUiClassifiedSource const& source) {
            for (int i = 0; i < source.entries.size(); ++i) {
                QVariantMap const entry = source.entries.at(i).toMap();
                bool const ghost = source.ghostRejected.value(i, 0) != 0;
                if (!decodeUiEntryPassesBandFilters(entry, ghost, job)) continue;
                if (entry.value(QStringLiteral("isTx")).toBool()
                    && !job.showTxMessagesInRx) {
                    continue;
                }
                QString const clearKey = decodiumBridgeDecodeRxClearEntryKey(entry);
                if (!clearKey.isEmpty() && job.clearedRxKeys.contains(clearKey)) {
                    continue;
                }
                if (source.rxMembership.value(i, 0) == 0) continue;

                QString const key = decodiumBridgeMirrorEntryKey(entry);
                if (!key.isEmpty()) {
                    if (seen.contains(key)) continue;
                    seen.insert(key);
                }
                rxEntries.append(entry);
            }
        };

        rxEntries.reserve(job.rxSource.entries.size()
                          + (job.includeBandSourceInRx
                                 ? job.bandSource.entries.size()
                                 : 0));
        appendSource(job.rxSource);
        if (job.includeBandSourceInRx) appendSource(job.bandSource);
        decodiumBridgeCoalesceRxPaneTxRows(rxEntries, job.mode);
        decodiumBridgeNormalizeDecodeEntries(rxEntries, 1500, job.mode);
        injectDecodeUiPeriodSeparators(rxEntries,
                                       job.showPeriodSeparators);
        result.rxEntries = DecodeListModel::prepareSnapshot(rxEntries);
    }

    result.workerMs = elapsed.elapsed();
    return result;
}

} // namespace

void DecodiumBridge::startDecodeUiSnapshotRefresh()
{
    if (m_decodeUiSnapshotInFlight || m_decodeUiPendingFlags == 0) return;

    int const flags = std::exchange(m_decodeUiPendingFlags, 0);
    DecodeUiSnapshotJob job;
    job.flags = flags;
    job.generation = m_decodeUiSnapshotGeneration;
    job.mode = m_mode;
    job.workedFilters = workedFilterOptions();
    job.preserveWorkedCq = !m_filtersBypassed && m_filterCqOnly;
    job.hideTelemetry = m_hideTelemetryOnlyDecodes;
    job.showTxMessagesInRx =
        getSetting(QStringLiteral("TXMessagesToRX"), true).toBool();
    job.newestFirst = m_decodeNewestFirst;
    job.showPeriodSeparators = m_decodeShowPeriodSeparator;
    job.clearedRxKeys = m_clearedRxDecodeKeys;
    job.includeBandSourceInRx =
        (flags & kDecodeUiRefreshRx) && !usingLegacyBackendForTx();

    bool const needsRxMembership = (flags & kDecodeUiRefreshRx) != 0;
    QString const rxStateKey = m_callsign.trimmed().toUpper()
        + QLatin1Char('|') + m_dxCall.trimmed().toUpper()
        + QLatin1Char('|') + m_autoCqLockedCall.trimmed().toUpper()
        + QLatin1Char('|') + inferredPartnerForAutolog().trimmed().toUpper();

    if (m_decodeUiGhostCache.size() > 4096
        || m_decodeUiRxMembershipCache.size() > 4096) {
        invalidateDecodeUiPredicateCaches();
    }

    auto classifySource = [this, needsRxMembership, &rxStateKey]
        (QVariantList const& entries) {
        DecodeUiClassifiedSource classified;
        classified.entries = entries;
        classified.ghostRejected.reserve(entries.size());
        classified.rxMembership.reserve(entries.size());
        for (QVariant const& value : entries) {
            QVariantMap const entry = value.toMap();
            QString const predicateKey = decodeUiPredicateCacheKey(entry);

            bool ghostRejected = false;
            if (m_hideGhostDecodes) {
                auto const ghostIt = m_decodeUiGhostCache.constFind(predicateKey);
                if (ghostIt != m_decodeUiGhostCache.constEnd()) {
                    ghostRejected = ghostIt.value();
                } else {
                    ghostRejected = looksLikeGhostDecode(entry);
                    m_decodeUiGhostCache.insert(predicateKey, ghostRejected);
                }
            }
            classified.ghostRejected.append(ghostRejected ? 1 : 0);

            bool rxMember = false;
            if (needsRxMembership && !ghostRejected) {
                QString const membershipKey = predicateKey
                    + QStringLiteral("|rx=") + rxStateKey;
                auto const memberIt =
                    m_decodeUiRxMembershipCache.constFind(membershipKey);
                if (memberIt != m_decodeUiRxMembershipCache.constEnd()) {
                    rxMember = memberIt.value();
                } else {
                    rxMember = entryBelongsToCurrentQso(entry, true);
                    m_decodeUiRxMembershipCache.insert(membershipKey, rxMember);
                }
            }
            classified.rxMembership.append(rxMember ? 1 : 0);
        }
        return classified;
    };

    bool const needsBandSource =
        (flags & (kDecodeUiRefreshBand | kDecodeUiRefreshFullSpectrum))
        || job.includeBandSourceInRx;
    if (needsBandSource) {
        job.bandSource = classifySource(m_decodeList);
    }
    if (flags & kDecodeUiRefreshRx) {
        job.rxSource = classifySource(m_rxDecodeList);
    }

    if (!m_decodeUiThreadPool) {
        m_decodeUiThreadPool = new QThreadPool(this);
        m_decodeUiThreadPool->setMaxThreadCount(1);
        m_decodeUiThreadPool->setExpiryTimeout(30000);
    }

    m_decodeUiSnapshotInFlight = true;
    quint64 const decodeSession = m_decodeSessionId;
    auto* watcher = new QFutureWatcher<DecodeUiSnapshotResult>(this);
    connect(watcher, &QFutureWatcher<DecodeUiSnapshotResult>::finished,
            this, [this, watcher, decodeSession]() {
        DecodeUiSnapshotResult result = watcher->result();
        watcher->deleteLater();
        m_decodeUiSnapshotInFlight = false;

        bool const stale = result.generation != m_decodeUiSnapshotGeneration
            && m_decodeUiPendingFlags != 0;
        if (decodeSession != m_decodeSessionId) {
            // A mode/session reset is a hard boundary, even when no newer
            // snapshot is pending. Never reinsert rows from the old worker.
        } else if (stale) {
            // Preserve all requested panes when a newer snapshot supersedes
            // this one; otherwise a late RX notification could drop a pending
            // Band Activity refresh.
            m_decodeUiPendingFlags |= result.flags;
        } else {
            MainThreadTraceScope trace(
                QStringLiteral("apply_decode_ui_snapshot"),
                QStringLiteral("generation=%1 flags=%2 band=%3 rx=%4 worker_ms=%5")
                    .arg(result.generation)
                    .arg(result.flags)
                    .arg(result.bandEntries.entries.size())
                    .arg(result.rxEntries.entries.size())
                    .arg(result.workerMs),
                25);
            int const budget = decodeUiRowsPerCycle();
            if ((result.flags & kDecodeUiRefreshBand)
                && m_bandActivityModel) {
                m_bandActivityModel->setEntriesBudgeted(std::move(result.bandEntries),
                                                        budget);
            }
            if ((result.flags & kDecodeUiRefreshRx)
                && m_rxDecodeModel) {
                m_rxDecodeModel->setEntriesBudgeted(std::move(result.rxEntries),
                                                    qMax(12, budget / 2));
            }
            if (result.flags & kDecodeUiRefreshFullSpectrum) {
                m_pendingFullSpectrumModelEntries =
                    std::move(result.fullSpectrumEntries.entries);
                m_pendingFullSpectrumModelKeys =
                    std::move(result.fullSpectrumEntries.keys);
                m_fullSpectrumSnapshotPending = true;
                scheduleFullSpectrumModelRefresh();
            }
        }

        if (m_decodeUiPendingFlags != 0) {
            if (m_decodeUiRefreshTimer) m_decodeUiRefreshTimer->start(0);
        } else {
            qint64 const remainingMs =
                qMax<qint64>(0, m_decodeUiBurstUntilMs
                                   - QDateTime::currentMSecsSinceEpoch());
            scheduleDeferredWorldMapFeedFlush(
                static_cast<int>(remainingMs + 80));
        }
    });

    watcher->setFuture(QtConcurrent::run(
        m_decodeUiThreadPool,
        [job = std::move(job)]() mutable {
            // This short worker feeds the GUI models. LowPriority can starve
            // it behind FT/OpenMP workers on Windows after decoding is done.
            QThread::currentThread()->setPriority(QThread::NormalPriority);
            return prepareDecodeUiSnapshot(std::move(job));
        }));
}

