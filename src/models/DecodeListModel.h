// 1.0.143 (fase 2 fluidità ListView decode):
// QAbstractListModel nativo che sostituisce i JS-array filtrati di
// DecodeWindow.qml (bandActivityModel / rxDecodeModel). Aggiornamento
// incrementale via diff (beginInsertRows / dataChanged / beginRemoveRows)
// invece di rebuild completo ad ogni decodeListChanged signal.
#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class DecodeListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    struct PreparedSnapshot {
        QVector<QVariantMap> entries;
        QVector<QString> keys;

        bool isConsistent() const { return entries.size() == keys.size(); }
    };

    enum Role {
        TimeRole = Qt::UserRole + 1,
        UtcRole,
        DbRole,
        DtRole,
        FreqRole,
        MessageRole,
        DisplayMessageRole,
        ModeRole,
        TimestampRole,
        IsTxRole,
        IsCQRole,
        IsMyCallRole,
        IsB4Role,
        IsLotwRole,
        IsSeparatorRole,
        MatchesDxCallRole,
        FromCallRole,
        DxCallsignRole,
        DxCountryRole,
        DxContinentRole,
        DxPrefixRole,
        UsStateRole,
        DxBearingRole,
        DxDistanceRole,
        DxIsMostWantedRole,
        DxIsNewCountryRole,
        DxIsNewBandRole,
        DxIsWorkedRole,
        DxIsNewDxccBandRole,
        DxIsNewGridRole,
        DxIsNewCqZoneRole,
        DxIsNewContinentRole,
        DxIsNewCallRole,
        HighlightBgRole,
        IsHighlightedRole,  // 1.0.144: aggregato isTx||isCQ||isMyCall||...
        AptypeRole,
        DriftRole,
        ForceRxPaneRole,
        QualityRole,
        EntryRole,  // tutta la QVariantMap originale (compat per modelData.X path)
    };
    Q_ENUM(Role)

    explicit DecodeListModel(QObject* parent = nullptr);

    int rowCount(QModelIndex const& parent = QModelIndex()) const override;
    QVariant data(QModelIndex const& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QVariantMap entry(int index) const;
    Q_INVOKABLE int count() const { return m_entries.size(); }

    // Sostituisce il contenuto con la nuova lista applicando diff smart:
    //  - new size > old + prefix identico → beginInsertRows in coda
    //  - new size < old + prefix identico → beginRemoveRows in coda
    //  - prepend + prune dalla coda → insert/remove incrementali
    //  - cambi nel mezzo → replace incrementale della sola regione cambiata
    //  - valori cambiati con chiave stabile → dataChanged sulle sole row coinvolte
    // La diff confronta entries via decodeMatchKey() (freq+message+timestamp).
    void setEntries(QVariantList const& newEntries);

    // Applica lo snapshot in piu turni dell'event loop. Il numero di righe
    // modificate per frame resta limitato anche quando un decode pass
    // sostituisce una porzione ampia della history.
    void setEntriesBudgeted(QVariantList const& newEntries, int maxRowsPerCycle = 48);
    // La preparazione allocation-heavy (QVariant -> QVariantMap + match key)
    // puo essere eseguita sul worker e trasferita al model senza ripeterla sul
    // main thread.
    static PreparedSnapshot prepareSnapshot(QVariantList const& newEntries);
    void setEntriesBudgeted(PreparedSnapshot prepared,
                            int maxRowsPerCycle = 48);
    // Aggiunge solo le nuove righe a un target gia in corso senza riconvertire
    // o confrontare l'intera history. prepend=true serve alla vista
    // newest-first; il drain resta limitato allo stesso budget per frame.
    void appendEntriesBudgeted(QVariantList const& newEntries,
                               bool prepend = false,
                               int maxRowsPerCycle = 48);
    bool hasPendingBudgetedUpdate() const;

signals:
    // Emesso una sola volta quando l'intero snapshot e' visibile. QML usa
    // questo segnale per aggiornare contatori e tail-follow senza reagire a
    // ogni tranche rowsInserted/dataChanged del budget per-frame.
    void snapshotApplied();

private:
    QVector<QVariantMap> m_entries;
    QVector<QString> m_entryKeys;
    QHash<int, QByteArray> m_roleNames;

    QVector<QVariantMap> m_budgetTargetEntries;
    QVector<QString> m_budgetTargetKeys;
    QSet<QString> m_budgetTargetKeySet;
    class QTimer* m_budgetTimer {nullptr};
    int m_budgetRowsPerCycle {48};
    bool m_budgetTargetActive {false};
    bool m_completedNonEmptySnapshot {false};
    qint64 m_budgetStepExpectedAtMs {0};
    // Windows can deliver several QML list mutations in the same overloaded
    // scene-graph turn. Keep normal machines at the 16 ms cadence, but briefly
    // spread follow-up work after the model itself observes a late/slow step.
    qint64 m_budgetBackpressureUntilMs {0};
    qint64 m_budgetBackpressureLastLogMs {0};

    static QString decodeMatchKey(QVariantMap const& entry);
    void applyBudgetedStep();
    void scheduleBudgetedStep();
    void clearBudgetedTarget();
    bool budgetedBackpressureActive(qint64 nowMs) const;
    int effectiveBudgetedStepIntervalMs(qint64 nowMs) const;
    int effectiveBudgetRowsPerCycle(qint64 nowMs) const;
    void noteBudgetedStepBackpressure(qint64 lateMs, qint64 workUs);
};
