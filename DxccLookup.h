#pragma once
//
// DxccLookup — standalone DXCC entity lookup from AD1C cty.dat
//
// This class parses the cty.dat file (country file maintained by Jim Reisert, AD1C)
// and provides fast callsign-to-entity resolution.  It is intentionally self-contained
// (no dependency on Configuration, boost, or pimpl) so that it can be used directly
// from DecodiumBridge / QML contexts.
//

#include <QString>
#include <QHash>
#include <QVector>

// ── Result record ────────────────────────────────────────────────────
struct DxccEntity {
    QString name;           // e.g. "Italy"
    QString prefix;         // primary prefix, e.g. "I"
    QString continent;      // two-letter continent code, e.g. "EU"
    int     cqZone  = 0;
    int     ituZone = 0;
    double  lat     = 0.0;  // degrees, positive = North
    double  lon     = 0.0;  // degrees, positive = West (cty.dat convention)
    double  utcOffset = 0.0;
    bool    waeOnly = false; // DARC WAE-only entity

    bool isValid() const { return !prefix.isEmpty(); }
};

// ── Lookup engine ────────────────────────────────────────────────────
class DxccLookup {
public:
    DxccLookup() = default;

    /// Load (or reload) the cty.dat file.  Returns true on success.
    bool loadCtyDat(const QString& path);

    /// Look up a callsign.  Returns a valid DxccEntity on match,
    /// or an invalid (empty) one when the callsign cannot be resolved.
    DxccEntity lookup(const QString& callsign) const;

    /// Number of DXCC entities loaded.
    int entityCount() const { return m_entities.size(); }

    /// True after a successful loadCtyDat().
    bool isLoaded() const { return !m_entities.isEmpty(); }

private:
    // Strip trailing override annotations  ({...}, (...), [...], <...>, ~...~)
    // from a prefix string and return the bare prefix.
    static QString stripOverrides(const QString& raw);

    // Apply per-prefix override annotations to a base entity and return
    // the adjusted result.
    static DxccEntity applyOverrides(const DxccEntity& base, const QString& rawPrefix);

    // Resolve a callsign to its "effective" base call (handle /P, /MM, etc.).
    static QString effectiveCall(const QString& callsign);

    QVector<DxccEntity>    m_entities;    // indexed by entity-id (0-based)
    QHash<QString, int>    m_exactMap;    // =CALL  → entity index
    QHash<QString, int>    m_prefixMap;   // prefix → entity index

    // For overrides we also store the raw prefix string (with annotations)
    // so that zone / lat-lon overrides can be applied at lookup time.
    QHash<QString, QString> m_exactRaw;   // =CALL  → raw prefix string
    QHash<QString, QString> m_prefixRaw;  // prefix → raw prefix string
};
