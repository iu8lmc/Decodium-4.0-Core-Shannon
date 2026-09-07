#include "DecodeListModel.h"
#include "DecodiumLogging.hpp"

#include <QScopeGuard>
#include <QElapsedTimer>
#include <QDateTime>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace {
// Mapping role → field name nella QVariantMap. Allinea con quello che
// enrichDecodeEntry() di DecodiumBridge popola.
struct RoleSpec {
    int role;
    char const* qmlName;
    char const* mapKey;
};

static RoleSpec const kRoleSpecs[] = {
    { DecodeListModel::TimeRole,                 "time",               "time" },
    { DecodeListModel::UtcRole,                  "utc",                "utc" },
    { DecodeListModel::DbRole,                   "db",                 "db" },
    { DecodeListModel::DtRole,                   "dt",                 "dt" },
    { DecodeListModel::FreqRole,                 "freq",               "freq" },
    { DecodeListModel::MessageRole,              "message",            "message" },
    { DecodeListModel::DisplayMessageRole,       "displayMessage",     "displayMessage" },
    { DecodeListModel::ModeRole,                 "mode",               "mode" },
    { DecodeListModel::TimestampRole,            "timestamp",          "timestamp" },
    { DecodeListModel::IsTxRole,                 "isTx",               "isTx" },
    { DecodeListModel::IsCQRole,                 "isCQ",               "isCQ" },
    { DecodeListModel::IsMyCallRole,             "isMyCall",           "isMyCall" },
    { DecodeListModel::IsB4Role,                 "isB4",               "isB4" },
    { DecodeListModel::IsLotwRole,               "isLotw",             "isLotw" },
    { DecodeListModel::IsSeparatorRole,          "isSeparator",        "isSeparator" },
    { DecodeListModel::MatchesDxCallRole,        "matchesDxCall",      "matchesDxCall" },
    { DecodeListModel::FromCallRole,             "fromCall",           "fromCall" },
    { DecodeListModel::DxCallsignRole,           "dxCallsign",         "dxCallsign" },
    { DecodeListModel::DxCountryRole,            "dxCountry",          "dxCountry" },
    { DecodeListModel::DxContinentRole,          "dxContinent",        "dxContinent" },
    { DecodeListModel::DxPrefixRole,             "dxPrefix",           "dxPrefix" },
    { DecodeListModel::UsStateRole,              "usState",            "usState" },
    { DecodeListModel::DxBearingRole,            "dxBearing",          "dxBearing" },
    { DecodeListModel::DxDistanceRole,           "dxDistance",         "dxDistance" },
    { DecodeListModel::DxIsMostWantedRole,       "dxIsMostWanted",     "dxIsMostWanted" },
    { DecodeListModel::DxIsNewCountryRole,       "dxIsNewCountry",     "dxIsNewCountry" },
    { DecodeListModel::DxIsNewBandRole,          "dxIsNewBand",        "dxIsNewBand" },
    { DecodeListModel::DxIsWorkedRole,           "dxIsWorked",         "dxIsWorked" },
    { DecodeListModel::DxIsNewDxccBandRole,      "dxIsNewDxccBand",    "dxIsNewDxccBand" },
    { DecodeListModel::DxIsNewGridRole,          "dxIsNewGrid",        "dxIsNewGrid" },
    { DecodeListModel::DxIsNewCqZoneRole,        "dxIsNewCqZone",      "dxIsNewCqZone" },
    { DecodeListModel::DxIsNewContinentRole,     "dxIsNewContinent",   "dxIsNewContinent" },
    { DecodeListModel::DxIsNewCallRole,          "dxIsNewCall",        "dxIsNewCall" },
    { DecodeListModel::HighlightBgRole,          "highlightBg",        "highlightBg" },
    { DecodeListModel::IsHighlightedRole,        "isHighlighted",      "isHighlighted" },
    { DecodeListModel::AptypeRole,               "aptype",             "aptype" },
    { DecodeListModel::DriftRole,                "drift",              "drift" },
    { DecodeListModel::ForceRxPaneRole,          "forceRxPane",        "forceRxPane" },
    { DecodeListModel::QualityRole,              "quality",            "quality" },
    // Role speciale che ritorna l'intera entry (per delegate che usano
    // modelData come oggetto-tipo: model.modelData.X). Qt6 espone anche
    // i singoli ruoli, ma "modelData" si bind via questo all'oggetto intero.
    { DecodeListModel::EntryRole,                "modelData",          "" },
};

QString roleFieldKey(int role)
{
    for (auto const& spec : kRoleSpecs) {
        if (spec.role == role) {
            return QString::fromLatin1(spec.mapKey);
        }
    }
    return QString();
}

} // namespace

DecodeListModel::DecodeListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    for (auto const& spec : kRoleSpecs) {
        m_roleNames.insert(spec.role, QByteArray(spec.qmlName));
    }
}

int DecodeListModel::rowCount(QModelIndex const& parent) const
{
    if (parent.isValid()) return 0;
    return m_entries.size();
}

QVariant DecodeListModel::data(QModelIndex const& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return QVariant();
    }
    QVariantMap const& entry = m_entries.at(index.row());

    if (role == EntryRole) return entry;

    QString const key = roleFieldKey(role);
    if (key.isEmpty()) return QVariant();
    return entry.value(key);
}

QHash<int, QByteArray> DecodeListModel::roleNames() const
{
    return m_roleNames;
}

QVariantMap DecodeListModel::entry(int index) const
{
    if (index < 0 || index >= m_entries.size()) return QVariantMap();
    return m_entries.at(index);
}

QString DecodeListModel::decodeMatchKey(QVariantMap const& entry)
{
    if (entry.value(QStringLiteral("isSeparator")).toBool()) {
        // 1.0.149: chiave separator stabile = time + timestamp ms. Per FT2
        // async time e' vuoto e tutti i separator avevano stessa key "sep|"
        // -> il diff teneva solo UN separator. Includere il ts li' rende
        // univoci per period FT2.
        QString const t = entry.value(QStringLiteral("time")).toString();
        QString const ts = entry.value(QStringLiteral("timestamp")).toString();
        return QStringLiteral("sep|") + t + QStringLiteral("|") + ts;
    }
    QString const ts = entry.value(QStringLiteral("timestamp")).toString();
    QString const freq = entry.value(QStringLiteral("freq")).toString();
    QString const msg = entry.value(QStringLiteral("message")).toString();
    QString const time = entry.value(QStringLiteral("time")).toString();
    QString const isTx = entry.value(QStringLiteral("isTx")).toBool() ? QStringLiteral("T") : QStringLiteral("R");
    if (!ts.isEmpty()) {
        return isTx + QStringLiteral("|") + ts + QStringLiteral("|") + freq + QStringLiteral("|") + msg;
    }
    return isTx + QStringLiteral("|") + time + QStringLiteral("|") + freq + QStringLiteral("|") + msg;
}

void DecodeListModel::setEntries(QVariantList const& newEntries)
{
    if (m_budgetTimer) m_budgetTimer->stop();
    clearBudgetedTarget();
    auto snapshotGuard = qScopeGuard([this]() { emit snapshotApplied(); });

    int const newCount = newEntries.size();
    int const oldCount = m_entries.size();

    // QVariant::toMap() and decodeMatchKey() are both allocation-heavy. Cache
    // their results once per snapshot so the structural probes below stay
    // linear even when early/final/deep passes replace a busy 500-row model.
    QVector<QVariantMap> incomingEntries;
    incomingEntries.reserve(newCount);
    QVector<QString> newKeys;
    newKeys.reserve(newCount);
    for (QVariant const& value : newEntries) {
        QVariantMap const entry = value.toMap();
        incomingEntries.append(entry);
        newKeys.append(decodeMatchKey(entry));
    }

    if (m_entryKeys.size() != oldCount) {
        m_entryKeys.clear();
        m_entryKeys.reserve(oldCount);
        for (QVariantMap const& entry : std::as_const(m_entries)) {
            m_entryKeys.append(decodeMatchKey(entry));
        }
    }
    QVector<QString> const oldKeys = m_entryKeys;

    // 1.0.144: scoped dataChanged — emette dataChanged SOLO per regioni
    // consecutive di row effettivamente cambiate, invece di "tutto il prefix".
    // Su FT2 attivo con append-only, normalmente il prefix è invariato →
    // zero dataChanged emit, solo InsertRows in coda.
    auto applyRangeDiff = [this, &incomingEntries](int modelStart, int newStart, int count) {
        int regionStart = -1;
        for (int i = 0; i < count; ++i) {
            int const modelIndex = modelStart + i;
            QVariantMap const& candidate = incomingEntries.at(newStart + i);
            bool const changed = (m_entries[modelIndex] != candidate);
            if (changed) {
                m_entries[modelIndex] = candidate;
                if (regionStart < 0) regionStart = modelIndex;
            } else if (regionStart >= 0) {
                emit dataChanged(index(regionStart), index(modelIndex - 1));
                regionStart = -1;
            }
        }
        if (regionStart >= 0) {
            emit dataChanged(index(regionStart), index(modelStart + count - 1));
        }
    };

    auto applyPrefixDiff = [&applyRangeDiff](int prefixEnd) {
        applyRangeDiff(0, 0, prefixEnd);
    };

    // --- Caso 1: append-only (prefix identico, append in coda) ---
    if (newCount >= oldCount) {
        bool prefixMatches = true;
        for (int i = 0; i < oldCount; ++i) {
            if (oldKeys.at(i) != newKeys.at(i)) {
                prefixMatches = false;
                break;
            }
        }
        if (prefixMatches) {
            applyPrefixDiff(oldCount);
            if (newCount > oldCount) {
                beginInsertRows(QModelIndex(), oldCount, newCount - 1);
                for (int i = oldCount; i < newCount; ++i) {
                    m_entries.append(incomingEntries.at(i));
                }
                endInsertRows();
            }
            m_entryKeys = newKeys;
            return;
        }
    }

    // --- Caso 2: shrink-only (prefix identico, remove dalla coda) ---
    if (newCount < oldCount) {
        bool prefixMatches = true;
        for (int i = 0; i < newCount; ++i) {
            if (oldKeys.at(i) != newKeys.at(i)) {
                prefixMatches = false;
                break;
            }
        }
        if (prefixMatches) {
            beginRemoveRows(QModelIndex(), newCount, oldCount - 1);
            m_entries.resize(newCount);
            endRemoveRows();
            applyPrefixDiff(newCount);
            m_entryKeys = newKeys;
            return;
        }
    }

    // --- Caso 3 (1.0.207): shift-N-from-head + append-M-to-tail ---
    // Tipico quando cap m_decodeList rimuove oldest e nuovi decode entrano in
    // coda (1.0.206 cap 500). Senza questo caso si cadeva in beginResetModel
    // = ridisegno totale ListView ad ogni decode → Full Spectrum scattoso.
    // Cerco lo shift N tale che oldEntries[N..oldCount-1] match newEntries[0..oldCount-N-1].
    // 1.0.478: busy FT8/FT4/FT2 slots can trim more than 64 old rows at once.
    // Raising the cap avoids beginResetModel() during high decode pile-up.
    if (oldCount > 0 && newCount > 0) {
        int const maxShift = qMin(oldCount, 256);
        for (int shift = 1; shift <= maxShift; ++shift) {
            int const overlapLen = oldCount - shift;
            if (overlapLen <= 0 || overlapLen > newCount) continue;
            bool overlapMatches = true;
            for (int i = 0; i < overlapLen; ++i) {
                if (oldKeys.at(i + shift) != newKeys.at(i)) {
                    overlapMatches = false;
                    break;
                }
            }
            if (!overlapMatches) continue;
            // Match! Applica: rimuovi shift entries dalla testa, poi append M nuove in coda.
            beginRemoveRows(QModelIndex(), 0, shift - 1);
            m_entries.remove(0, shift);
            endRemoveRows();
            int const tailNew = newCount - overlapLen;
            if (tailNew > 0) {
                beginInsertRows(QModelIndex(), overlapLen, overlapLen + tailNew - 1);
                for (int i = overlapLen; i < newCount; ++i) {
                    m_entries.append(incomingEntries.at(i));
                }
                endInsertRows();
            }
            applyPrefixDiff(overlapLen);  // catch in-place value updates su overlap
            m_entryKeys = newKeys;
            return;
        }
    }

    // --- Caso 4: prepend-N + prune-M dalla coda ---
    // Usato dalle viste newest-first e da snapshot che inseriscono il nuovo
    // slot davanti alla history. Prima cadeva nel reset completo del model:
    // Qt Quick distruggeva e ricreava tutti i delegate proprio alla consegna
    // dei risultati FT4/FT8, producendo il blocco grafico periodico.
    if (oldCount > 0 && newCount > 0) {
        int const maxPrepend = qMin(newCount, 256);
        for (int prepend = 1; prepend <= maxPrepend; ++prepend) {
            int const overlapLen = newCount - prepend;
            if (overlapLen <= 0 || overlapLen > oldCount) continue;

            bool overlapMatches = true;
            for (int i = 0; i < overlapLen; ++i) {
                if (oldKeys.at(i) != newKeys.at(prepend + i)) {
                    overlapMatches = false;
                    break;
                }
            }
            if (!overlapMatches) continue;

            if (overlapLen < oldCount) {
                beginRemoveRows(QModelIndex(), overlapLen, oldCount - 1);
                m_entries.resize(overlapLen);
                endRemoveRows();
            }

            beginInsertRows(QModelIndex(), 0, prepend - 1);
            for (int i = prepend - 1; i >= 0; --i) {
                m_entries.insert(0, incomingEntries.at(i));
            }
            endInsertRows();

            applyRangeDiff(prepend, prepend, overlapLen);
            m_entryKeys = newKeys;
            return;
        }
    }

    // --- Caso 5: replace della sola regione realmente cambiata ---
    // Le passate early/final/deep dello stesso slot non sono necessariamente
    // append-only: il decoder puo sostituire il tail provvisorio mantenendo
    // intatta tutta la history. Un model reset distrugge comunque tutti i
    // delegate visibili e coincide con gli scatti FT4/FT8. Conserva invece il
    // prefix e il suffix comuni e sostituisci soltanto la regione centrale.
    int commonPrefix = 0;
    int const commonLimit = qMin(oldCount, newCount);
    while (commonPrefix < commonLimit
           && oldKeys.at(commonPrefix) == newKeys.at(commonPrefix)) {
        ++commonPrefix;
    }

    int commonSuffix = 0;
    while (commonSuffix < oldCount - commonPrefix
           && commonSuffix < newCount - commonPrefix
           && oldKeys.at(oldCount - 1 - commonSuffix)
                == newKeys.at(newCount - 1 - commonSuffix)) {
        ++commonSuffix;
    }

    applyRangeDiff(0, 0, commonPrefix);

    int const oldMiddleCount = oldCount - commonPrefix - commonSuffix;
    int const newMiddleCount = newCount - commonPrefix - commonSuffix;
    int const replaceCount = qMin(oldMiddleCount, newMiddleCount);

    // Aggiorna prima le righe sovrapposte in-place. Il vecchio remove-all +
    // insert-all portava temporaneamente rowCount() a zero quando due snapshot
    // erano disgiunti; ListView mostrava "No decodes" e le transizioni potevano
    // lasciare Full Spectrum vuoto fino al frame successivo sotto carico.
    applyRangeDiff(commonPrefix, commonPrefix, replaceCount);

    if (oldMiddleCount > newMiddleCount) {
        int const removeStart = commonPrefix + newMiddleCount;
        int const removeCount = oldMiddleCount - newMiddleCount;
        beginRemoveRows(QModelIndex(), removeStart, removeStart + removeCount - 1);
        m_entries.remove(removeStart, removeCount);
        endRemoveRows();
    } else if (newMiddleCount > oldMiddleCount) {
        int const insertStart = commonPrefix + oldMiddleCount;
        int const insertCount = newMiddleCount - oldMiddleCount;
        beginInsertRows(QModelIndex(), insertStart, insertStart + insertCount - 1);
        for (int i = 0; i < insertCount; ++i) {
            m_entries.insert(insertStart + i, incomingEntries.at(insertStart + i));
        }
        endInsertRows();
    }

    if (commonSuffix > 0) {
        int const suffixStart = commonPrefix + newMiddleCount;
        applyRangeDiff(suffixStart, suffixStart, commonSuffix);
    }
    m_entryKeys = newKeys;
}

bool DecodeListModel::hasPendingBudgetedUpdate() const
{
    if (!m_budgetTargetActive) return false;
    if (m_budgetTargetEntries.size() != m_entries.size()
        || m_budgetTargetKeys.size() != m_entryKeys.size()) {
        return true;
    }
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entryKeys.at(i) != m_budgetTargetKeys.at(i)
            || m_entries.at(i) != m_budgetTargetEntries.at(i)) {
            return true;
        }
    }
    return false;
}

void DecodeListModel::clearBudgetedTarget()
{
    m_budgetTargetActive = false;
    m_budgetTargetEntries.clear();
    m_budgetTargetKeys.clear();
    m_budgetTargetKeySet.clear();
}

bool DecodeListModel::budgetedBackpressureActive(qint64 nowMs) const
{
#ifdef Q_OS_WIN
    return nowMs < m_budgetBackpressureUntilMs;
#else
    Q_UNUSED(nowMs);
    return false;
#endif
}

int DecodeListModel::effectiveBudgetedStepIntervalMs(qint64 nowMs) const
{
    return budgetedBackpressureActive(nowMs) ? 32 : 16;
}

int DecodeListModel::effectiveBudgetRowsPerCycle(qint64 nowMs) const
{
    return budgetedBackpressureActive(nowMs)
        ? qMin(m_budgetRowsPerCycle, 4)
        : m_budgetRowsPerCycle;
}

void DecodeListModel::noteBudgetedStepBackpressure(qint64 lateMs, qint64 workUs)
{
#ifdef Q_OS_WIN
    // A late timer means the event loop was already unable to service the
    // previous slice. A long model slice has the same practical effect once
    // QML delegates are attached. Pace only the visual model for a short
    // recovery window; the decoder, audio and CAT paths are not involved.
    static constexpr qint64 kLateThresholdMs = 75;
    static constexpr qint64 kWorkThresholdUs = 10000;
    static constexpr qint64 kRecoveryMs = 2200;
    if (lateMs < kLateThresholdMs && workUs < kWorkThresholdUs) {
        return;
    }

    qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
    bool const wasActive = budgetedBackpressureActive(nowMs);
    m_budgetBackpressureUntilMs = qMax(m_budgetBackpressureUntilMs,
                                       nowMs + kRecoveryMs);
    if (!wasActive || nowMs - m_budgetBackpressureLastLogMs >= 5000) {
        m_budgetBackpressureLastLogMs = nowMs;
        DIAG_WARN(QStringLiteral("[MODELSTEP] visual backpressure model=%1 late_ms=%2 work_us=%3 interval_ms=32 rows_per_cycle=4 recovery_ms=%4")
                      .arg(objectName().isEmpty() ? QStringLiteral("unnamed") : objectName())
                      .arg(lateMs)
                      .arg(workUs)
                      .arg(kRecoveryMs));
    }
#else
    Q_UNUSED(lateMs);
    Q_UNUSED(workUs);
#endif
}

void DecodeListModel::scheduleBudgetedStep()
{
    if (!m_budgetTimer) {
        m_budgetTimer = new QTimer(this);
        m_budgetTimer->setSingleShot(true);
        connect(m_budgetTimer, &QTimer::timeout,
                this, &DecodeListModel::applyBudgetedStep);
    }
    if (m_budgetTargetActive && !m_budgetTimer->isActive()) {
        qint64 const nowMs = QDateTime::currentMSecsSinceEpoch();
        int const intervalMs = effectiveBudgetedStepIntervalMs(nowMs);
        if (m_budgetTimer->interval() != intervalMs) {
            m_budgetTimer->setInterval(intervalMs);
        }
        m_budgetStepExpectedAtMs = nowMs + intervalMs;
        m_budgetTimer->start();
    }
}

DecodeListModel::PreparedSnapshot DecodeListModel::prepareSnapshot(
    QVariantList const& newEntries)
{
    PreparedSnapshot prepared;
    prepared.entries.reserve(newEntries.size());
    prepared.keys.reserve(newEntries.size());
    for (QVariant const& value : newEntries) {
        QVariantMap const entry = value.toMap();
        prepared.entries.append(entry);
        prepared.keys.append(decodeMatchKey(entry));
    }
    return prepared;
}

void DecodeListModel::setEntriesBudgeted(QVariantList const& newEntries,
                                         int maxRowsPerCycle)
{
    setEntriesBudgeted(prepareSnapshot(newEntries), maxRowsPerCycle);
}

void DecodeListModel::setEntriesBudgeted(PreparedSnapshot prepared,
                                         int maxRowsPerCycle)
{
    m_budgetRowsPerCycle = qBound(1, maxRowsPerCycle, 256);
    if (!prepared.isConsistent()) {
        prepared.keys.clear();
        prepared.keys.reserve(prepared.entries.size());
        for (QVariantMap const& entry : std::as_const(prepared.entries)) {
            prepared.keys.append(decodeMatchKey(entry));
        }
    }
    m_budgetTargetEntries = std::move(prepared.entries);
    m_budgetTargetKeys = std::move(prepared.keys);
    m_budgetTargetKeySet.clear();
    m_budgetTargetKeySet.reserve(m_budgetTargetKeys.size());
    for (QString const& key : std::as_const(m_budgetTargetKeys)) {
        if (!key.isEmpty()) m_budgetTargetKeySet.insert(key);
    }
    m_budgetTargetActive = true;

    if (m_entryKeys.size() != m_entries.size()) {
        m_entryKeys.clear();
        m_entryKeys.reserve(m_entries.size());
        for (QVariantMap const& entry : std::as_const(m_entries)) {
            m_entryKeys.append(decodeMatchKey(entry));
        }
    }

    if (!hasPendingBudgetedUpdate()) {
        if (m_budgetTimer) m_budgetTimer->stop();
        clearBudgetedTarget();
        emit snapshotApplied();
        return;
    }

    // Una nuova snapshot arrivata mentre il modello sta drenando sostituisce
    // il target precedente. Anche la prima tranche parte nel tick successivo:
    // il callback del worker resta un handoff breve e non somma conversione,
    // diff e notifiche QML nello stesso frame.
    if (m_budgetTimer && m_budgetTimer->isActive()) return;
    scheduleBudgetedStep();
}

void DecodeListModel::appendEntriesBudgeted(QVariantList const& newEntries,
                                            bool prepend,
                                            int maxRowsPerCycle)
{
    if (newEntries.isEmpty()) return;

    m_budgetRowsPerCycle = qBound(1, maxRowsPerCycle, 256);
    if (m_entryKeys.size() != m_entries.size()) {
        m_entryKeys.clear();
        m_entryKeys.reserve(m_entries.size());
        for (QVariantMap const& entry : std::as_const(m_entries)) {
            m_entryKeys.append(decodeMatchKey(entry));
        }
    }

    bool const hadActiveTarget = m_budgetTargetActive;
    if (!hadActiveTarget) {
        m_budgetTargetEntries = m_entries;
        m_budgetTargetKeys = m_entryKeys;
        m_budgetTargetKeySet.clear();
        m_budgetTargetKeySet.reserve(m_budgetTargetKeys.size() + newEntries.size());
        for (QString const& key : std::as_const(m_budgetTargetKeys)) {
            if (!key.isEmpty()) m_budgetTargetKeySet.insert(key);
        }
        m_budgetTargetActive = true;
    }

    QVector<QVariantMap> acceptedEntries;
    QVector<QString> acceptedKeys;
    acceptedEntries.reserve(newEntries.size());
    acceptedKeys.reserve(newEntries.size());
    for (QVariant const& value : newEntries) {
        QVariantMap const entry = value.toMap();
        QString const key = decodeMatchKey(entry);
        if (!key.isEmpty() && m_budgetTargetKeySet.contains(key)) continue;
        if (!key.isEmpty()) m_budgetTargetKeySet.insert(key);
        acceptedEntries.append(entry);
        acceptedKeys.append(key);
    }
    if (acceptedEntries.isEmpty()) {
        if (!hadActiveTarget) clearBudgetedTarget();
        return;
    }

    if (prepend) {
        QVector<QVariantMap> mergedEntries;
        QVector<QString> mergedKeys;
        mergedEntries.reserve(acceptedEntries.size() + m_budgetTargetEntries.size());
        mergedKeys.reserve(acceptedKeys.size() + m_budgetTargetKeys.size());
        mergedEntries += acceptedEntries;
        mergedEntries += m_budgetTargetEntries;
        mergedKeys += acceptedKeys;
        mergedKeys += m_budgetTargetKeys;
        m_budgetTargetEntries.swap(mergedEntries);
        m_budgetTargetKeys.swap(mergedKeys);
    } else {
        m_budgetTargetEntries += acceptedEntries;
        m_budgetTargetKeys += acceptedKeys;
    }

    // Se una tranche precedente e gia programmata, il target appena esteso
    // verra drenato da quel timer. Evitiamo due tranche nello stesso frame.
    if (m_budgetTimer && m_budgetTimer->isActive()) return;
    scheduleBudgetedStep();
}

void DecodeListModel::applyBudgetedStep()
{
#ifdef Q_OS_WIN
    qint64 const enteredAtMs = QDateTime::currentMSecsSinceEpoch();
    qint64 const lateMs = m_budgetStepExpectedAtMs > 0
        ? qMax<qint64>(0, enteredAtMs - m_budgetStepExpectedAtMs)
        : 0;
    QElapsedTimer totalTimer;
    totalTimer.start();
    noteBudgetedStepBackpressure(lateMs, 0);
    auto workBackpressureGuard = qScopeGuard([this, lateMs, &totalTimer]() {
        noteBudgetedStepBackpressure(lateMs, totalTimer.nsecsElapsed() / 1000);
    });
#endif
    m_budgetStepExpectedAtMs = 0;

    int const rowBudget = effectiveBudgetRowsPerCycle(
        QDateTime::currentMSecsSinceEpoch());

    if (!hasPendingBudgetedUpdate()) {
        bool const completedSnapshot = m_budgetTargetActive;
        clearBudgetedTarget();
        if (completedSnapshot) {
            if (!m_entries.isEmpty()) m_completedNonEmptySnapshot = true;
            emit snapshotApplied();
        }
        return;
    }

    int const oldCount = m_entries.size();
    int const newCount = m_budgetTargetEntries.size();
    // A row budget alone is not stable across GPUs, QML delegates and
    // machines. Bound the synchronous model work as well; the next 16 ms
    // timer tick continues exactly where this one stopped.
    static constexpr qint64 kBudgetedStepMaxMs = 4;
    QElapsedTimer stepBudget;
    stepBudget.start();
    int const commonLimit = qMin(oldCount, newCount);

    int commonPrefix = 0;
    while (commonPrefix < commonLimit
           && m_entryKeys.at(commonPrefix) == m_budgetTargetKeys.at(commonPrefix)) {
        ++commonPrefix;
    }

    int commonSuffix = 0;
    while (commonSuffix < oldCount - commonPrefix
           && commonSuffix < newCount - commonPrefix
           && m_entryKeys.at(oldCount - 1 - commonSuffix)
                  == m_budgetTargetKeys.at(newCount - 1 - commonSuffix)) {
        ++commonSuffix;
    }

    int const oldMiddleCount = oldCount - commonPrefix - commonSuffix;
    int const newMiddleCount = newCount - commonPrefix - commonSuffix;
    int rowsChanged = 0;

    if (oldMiddleCount > 0 && newMiddleCount > 0) {
        int replaceCount = 0;
        int const maxReplace = std::min({rowBudget,
                                         oldMiddleCount,
                                         newMiddleCount});
        while (replaceCount < maxReplace
               && (replaceCount == 0 || stepBudget.elapsed() < kBudgetedStepMaxMs)) {
            ++replaceCount;
        }
        for (int i = 0; i < replaceCount; ++i) {
            int const row = commonPrefix + i;
            m_entries[row] = m_budgetTargetEntries.at(row);
            m_entryKeys[row] = m_budgetTargetKeys.at(row);
        }
        emit dataChanged(index(commonPrefix),
                         index(commonPrefix + replaceCount - 1));
        rowsChanged = replaceCount;
    } else if (newMiddleCount > 0) {
        // beginInsertRows/endInsertRows can wake many QML delegates. Keep
        // insertion chunks deliberately small even on high-core desktops.
        int insertCap = 12;
#ifdef Q_OS_WIN
        // The first FT8 snapshot fans out to multiple animated ListViews. On
        // Windows, creating twelve delegates per model in the same scene-graph
        // turn can monopolize the GUI for seconds. Stagger only this initial
        // population; later incremental updates retain the normal throughput.
        if (!m_completedNonEmptySnapshot) {
            insertCap = 1;
        }
#endif
        int const insertCount = qMin(qMin(rowBudget, newMiddleCount), insertCap);
#ifdef Q_OS_WIN
        qint64 const beforeBeginUs = totalTimer.nsecsElapsed() / 1000;
#endif
        beginInsertRows(QModelIndex(), commonPrefix,
                        commonPrefix + insertCount - 1);
#ifdef Q_OS_WIN
        qint64 const afterBeginUs = totalTimer.nsecsElapsed() / 1000;
#endif
        for (int i = 0; i < insertCount; ++i) {
            int const sourceRow = commonPrefix + i;
            m_entries.insert(commonPrefix + i,
                             m_budgetTargetEntries.at(sourceRow));
            m_entryKeys.insert(commonPrefix + i,
                               m_budgetTargetKeys.at(sourceRow));
        }
#ifdef Q_OS_WIN
        qint64 const beforeEndUs = totalTimer.nsecsElapsed() / 1000;
#endif
        endInsertRows();
#ifdef Q_OS_WIN
        qint64 const afterEndUs = totalTimer.nsecsElapsed() / 1000;
#endif
        rowsChanged = insertCount;
#ifdef Q_OS_WIN
        if (!m_completedNonEmptySnapshot || lateMs >= 90 || afterEndUs >= 25000) {
            DIAG_INFO(QStringLiteral("[MODELSTEP] model=%1 old=%2 target=%3 inserted=%4 late_ms=%5 begin_us=%6 mutate_us=%7 end_us=%8 total_us=%9 pending=%10")
                          .arg(objectName().isEmpty() ? QStringLiteral("unnamed") : objectName())
                          .arg(oldCount)
                          .arg(newCount)
                          .arg(insertCount)
                          .arg(lateMs)
                          .arg(afterBeginUs - beforeBeginUs)
                          .arg(beforeEndUs - afterBeginUs)
                          .arg(afterEndUs - beforeEndUs)
                          .arg(afterEndUs)
                          .arg(hasPendingBudgetedUpdate()));
        }
#endif
    } else if (oldMiddleCount > 0) {
        int const removeCount = qMin(qMin(rowBudget, oldMiddleCount), 12);
        beginRemoveRows(QModelIndex(), commonPrefix,
                        commonPrefix + removeCount - 1);
        m_entries.remove(commonPrefix, removeCount);
        m_entryKeys.remove(commonPrefix, removeCount);
        endRemoveRows();
        rowsChanged = removeCount;
    } else {
        // Le chiavi coincidono: restano solo metadati arricchiti cambiati.
        int regionStart = -1;
        int regionEnd = -1;
        QVector<QPair<int, int>> changedRegions;
        for (int i = 0;
             i < newCount
             && rowsChanged < rowBudget
             && (rowsChanged == 0 || stepBudget.elapsed() < kBudgetedStepMaxMs);
             ++i) {
            if (m_entries.at(i) == m_budgetTargetEntries.at(i)) {
                if (regionStart >= 0) {
                    changedRegions.append(qMakePair(regionStart, regionEnd));
                    regionStart = -1;
                }
                continue;
            }
            m_entries[i] = m_budgetTargetEntries.at(i);
            ++rowsChanged;
            if (regionStart < 0) regionStart = i;
            regionEnd = i;
        }
        if (regionStart >= 0) {
            changedRegions.append(qMakePair(regionStart, regionEnd));
        }
        for (auto const& region : std::as_const(changedRegions)) {
            emit dataChanged(index(region.first), index(region.second));
        }
    }

    if (rowsChanged <= 0 || !hasPendingBudgetedUpdate()) {
        clearBudgetedTarget();
        if (!m_entries.isEmpty()) m_completedNonEmptySnapshot = true;
        emit snapshotApplied();
        return;
    }
    scheduleBudgetedStep();
}
