// 1.0.143 (fase 2 fluidità ListView decode):
// QAbstractListModel nativo che sostituisce i JS-array filtrati di
// DecodeWindow.qml (bandActivityModel / rxDecodeModel). Aggiornamento
// incrementale via diff (beginInsertRows / dataChanged / beginRemoveRows)
// invece di rebuild completo ad ogni decodeListChanged signal.
#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class DecodeListModel : public QAbstractListModel
{
    Q_OBJECT
public:
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
        AptypeRole,
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
    //  - cambi nel mezzo → dataChanged per le row modificate
    //  - altrimenti (struttura cambiata) → fallback beginResetModel
    // La diff confronta entries via decodeMatchKey() (freq+message+timestamp).
    void setEntries(QVariantList const& newEntries);

private:
    QVector<QVariantMap> m_entries;
    QHash<int, QByteArray> m_roleNames;

    static QString decodeMatchKey(QVariantMap const& entry);
};
