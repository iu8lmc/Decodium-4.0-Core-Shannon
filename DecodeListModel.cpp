#include "DecodeListModel.h"

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
    int const newCount = newEntries.size();
    int const oldCount = m_entries.size();

    // 1.0.144: scoped dataChanged — emette dataChanged SOLO per regioni
    // consecutive di row effettivamente cambiate, invece di "tutto il prefix".
    // Su FT2 attivo con append-only, normalmente il prefix è invariato →
    // zero dataChanged emit, solo InsertRows in coda.
    auto applyPrefixDiff = [this, &newEntries](int prefixEnd) {
        int regionStart = -1;
        for (int i = 0; i < prefixEnd; ++i) {
            QVariantMap const candidate = newEntries.at(i).toMap();
            bool const changed = (m_entries[i] != candidate);
            if (changed) {
                m_entries[i] = candidate;
                if (regionStart < 0) regionStart = i;
            } else if (regionStart >= 0) {
                emit dataChanged(index(regionStart), index(i - 1));
                regionStart = -1;
            }
        }
        if (regionStart >= 0) {
            emit dataChanged(index(regionStart), index(prefixEnd - 1));
        }
    };

    // --- Caso 1: append-only (prefix identico, append in coda) ---
    if (newCount >= oldCount) {
        bool prefixMatches = true;
        for (int i = 0; i < oldCount; ++i) {
            QVariantMap const candidate = newEntries.at(i).toMap();
            if (decodeMatchKey(m_entries.at(i)) != decodeMatchKey(candidate)) {
                prefixMatches = false;
                break;
            }
        }
        if (prefixMatches) {
            applyPrefixDiff(oldCount);
            if (newCount > oldCount) {
                beginInsertRows(QModelIndex(), oldCount, newCount - 1);
                for (int i = oldCount; i < newCount; ++i) {
                    m_entries.append(newEntries.at(i).toMap());
                }
                endInsertRows();
            }
            return;
        }
    }

    // --- Caso 2: shrink-only (prefix identico, remove dalla coda) ---
    if (newCount < oldCount) {
        bool prefixMatches = true;
        for (int i = 0; i < newCount; ++i) {
            QVariantMap const candidate = newEntries.at(i).toMap();
            if (decodeMatchKey(m_entries.at(i)) != decodeMatchKey(candidate)) {
                prefixMatches = false;
                break;
            }
        }
        if (prefixMatches) {
            beginRemoveRows(QModelIndex(), newCount, oldCount - 1);
            m_entries.resize(newCount);
            endRemoveRows();
            applyPrefixDiff(newCount);
            return;
        }
    }

    // --- Caso 3 (fallback): struttura cambiata, reset model ---
    beginResetModel();
    m_entries.clear();
    m_entries.reserve(newCount);
    for (int i = 0; i < newCount; ++i) {
        m_entries.append(newEntries.at(i).toMap());
    }
    endResetModel();
}
