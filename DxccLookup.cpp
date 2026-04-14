#include "DxccLookup.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

// ─────────────────────────────────────────────────────────────────────
// loadCtyDat — parse cty.dat
// ─────────────────────────────────────────────────────────────────────
bool DxccLookup::loadCtyDat(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[DxccLookup] cannot open" << path << file.errorString();
        return false;
    }

    QVector<DxccEntity>     entities;
    QHash<QString, int>     exactMap;
    QHash<QString, int>     prefixMap;
    QHash<QString, QString> exactRaw;
    QHash<QString, QString> prefixRaw;

    QTextStream in(&file);
    while (!in.atEnd()) {
        // ── 1. Read entity header line ──────────────────────────────
        QString headerLine = in.readLine();
        if (headerLine.trimmed().isEmpty())
            continue;

        QStringList hdr = headerLine.split(':');
        if (hdr.size() < 8)
            continue;   // not a valid entity header

        DxccEntity ent;
        ent.name      = hdr[0].trimmed();
        ent.cqZone    = hdr[1].trimmed().toInt();
        ent.ituZone   = hdr[2].trimmed().toInt();
        ent.continent = hdr[3].trimmed();
        ent.lat       = hdr[4].trimmed().toDouble();
        ent.lon       = hdr[5].trimmed().toDouble();
        ent.utcOffset = hdr[6].trimmed().toDouble();

        QString primaryPfx = hdr[7].trimmed();
        if (primaryPfx.startsWith('*')) {
            primaryPfx = primaryPfx.mid(1);
            ent.waeOnly = true;
        }
        ent.prefix = primaryPfx;

        int entityIdx = entities.size();
        entities.append(ent);

        // ── 2. Read prefix detail lines (until ';') ─────────────────
        QString detail;
        while (!in.atEnd()) {
            QString line = in.readLine();
            detail += line;
            if (detail.contains(';'))
                break;
        }
        // Remove trailing ';' and any whitespace after it
        int semiPos = detail.lastIndexOf(';');
        if (semiPos >= 0)
            detail = detail.left(semiPos);

        QStringList prefixes = detail.split(',', Qt::SkipEmptyParts);
        for (const QString& rawPfxUntrimmed : prefixes) {
            QString rawPfx = rawPfxUntrimmed.trimmed();
            if (rawPfx.isEmpty())
                continue;

            bool exact = false;
            QString workPfx = rawPfx;
            if (workPfx.startsWith('=')) {
                workPfx = workPfx.mid(1);
                exact = true;
            }

            // Strip override annotations to get the bare prefix/call
            QString key = stripOverrides(workPfx).toUpper();
            if (key.isEmpty())
                continue;

            if (exact) {
                // Exact-match entries: only match if callsign == key
                if (!exactMap.contains(key)) {
                    exactMap.insert(key, entityIdx);
                    exactRaw.insert(key, workPfx);
                }
            } else {
                // Prefix-match entries: longest prefix wins, so if two
                // prefixes map to different entities we keep the first one
                // encountered (cty.dat lists more-specific entries later,
                // but within the same entity block order does not matter;
                // cross-entity conflicts are resolved at lookup time by
                // longest-match).
                if (!prefixMap.contains(key)) {
                    prefixMap.insert(key, entityIdx);
                    prefixRaw.insert(key, workPfx);
                }
            }
        }
    }

    // Commit
    m_entities  = std::move(entities);
    m_exactMap  = std::move(exactMap);
    m_prefixMap = std::move(prefixMap);
    m_exactRaw  = std::move(exactRaw);
    m_prefixRaw = std::move(prefixRaw);

    qInfo() << "[DxccLookup] loaded" << m_entities.size() << "entities,"
            << m_prefixMap.size() << "prefixes,"
            << m_exactMap.size() << "exact calls from" << path;
    return true;
}

// ─────────────────────────────────────────────────────────────────────
// lookup — resolve callsign → DxccEntity
// ─────────────────────────────────────────────────────────────────────
DxccEntity DxccLookup::lookup(const QString& callsign) const
{
    if (m_entities.isEmpty())
        return {};

    QString call = effectiveCall(callsign).toUpper();
    if (call.isEmpty())
        return {};

    // Skip maritime-mobile and air-mobile — no DXCC entity
    if (call.endsWith("/MM") || call.endsWith("/AM"))
        return {};

    // ── 1. Try exact match first ────────────────────────────────────
    {
        auto it = m_exactMap.constFind(call);
        if (it != m_exactMap.constEnd()) {
            int idx = it.value();
            return applyOverrides(m_entities[idx], m_exactRaw.value(call));
        }
    }

    // ── 2. Longest-prefix match ─────────────────────────────────────
    // Progressively shorten from the full call down to a single character.
    for (int len = call.size(); len >= 1; --len) {
        QString tryPfx = call.left(len);
        auto it = m_prefixMap.constFind(tryPfx);
        if (it != m_prefixMap.constEnd()) {
            int idx = it.value();
            return applyOverrides(m_entities[idx], m_prefixRaw.value(tryPfx));
        }
    }

    return {};
}

// ─────────────────────────────────────────────────────────────────────
// stripOverrides — remove (...), [...], <...>, {...}, ~...~ annotations
// ─────────────────────────────────────────────────────────────────────
QString DxccLookup::stripOverrides(const QString& raw)
{
    QString result;
    result.reserve(raw.size());
    int i = 0;
    while (i < raw.size()) {
        QChar ch = raw[i];
        if (ch == '(' || ch == '[' || ch == '<' || ch == '{' || ch == '~') {
            QChar close = (ch == '(') ? ')' :
                          (ch == '[') ? ']' :
                          (ch == '<') ? '>' :
                          (ch == '{') ? '}' : '~';
            int end = raw.indexOf(close, i + 1);
            if (end >= 0) {
                i = end + 1;
                continue;
            }
        }
        result += ch;
        ++i;
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────
// applyOverrides — adjust entity fields for per-prefix annotations
// ─────────────────────────────────────────────────────────────────────
DxccEntity DxccLookup::applyOverrides(const DxccEntity& base, const QString& rawPrefix)
{
    DxccEntity result = base;

    auto extractBetween = [&](QChar open, QChar close) -> QString {
        int s = rawPrefix.indexOf(open);
        if (s < 0) return {};
        int e = rawPrefix.indexOf(close, s + 1);
        if (e < 0) return {};
        return rawPrefix.mid(s + 1, e - s - 1);
    };

    // (nn) → CQ zone override
    QString v = extractBetween('(', ')');
    if (!v.isEmpty()) {
        bool ok = false;
        int z = v.toInt(&ok);
        if (ok) result.cqZone = z;
    }

    // [nn] → ITU zone override
    v = extractBetween('[', ']');
    if (!v.isEmpty()) {
        bool ok = false;
        int z = v.toInt(&ok);
        if (ok) result.ituZone = z;
    }

    // <lat/lon> → latitude/longitude override
    v = extractBetween('<', '>');
    if (!v.isEmpty()) {
        QStringList parts = v.split('/');
        if (parts.size() == 2) {
            bool ok1 = false, ok2 = false;
            double la = parts[0].toDouble(&ok1);
            double lo = parts[1].toDouble(&ok2);
            if (ok1 && ok2) {
                result.lat = la;
                result.lon = lo;
            }
        }
    }

    // {XX} → continent override
    v = extractBetween('{', '}');
    if (!v.isEmpty())
        result.continent = v;

    // ~nn~ → UTC offset override
    // Note: tilde is used for both open and close, so we search manually
    {
        int s = rawPrefix.indexOf('~');
        if (s >= 0) {
            int e = rawPrefix.indexOf('~', s + 1);
            if (e > s) {
                QString utcStr = rawPrefix.mid(s + 1, e - s - 1);
                bool ok = false;
                double utc = utcStr.toDouble(&ok);
                if (ok) result.utcOffset = utc;
            }
        }
    }

    return result;
}

// ─────────────────────────────────────────────────────────────────────
// effectiveCall — handle compound calls (VP2E/K1XX → VP2E, W1AW/KH6 → KH6)
// ─────────────────────────────────────────────────────────────────────
QString DxccLookup::effectiveCall(const QString& callsign)
{
    QString call = callsign.toUpper().trimmed();

    int slash = call.indexOf('/');
    if (slash < 0)
        return call;

    QString left  = call.left(slash);
    QString right = call.mid(slash + 1);

    // This mirrors the logic of Radio::effective_prefix from the
    // upstream codebase: the shorter part is assumed to be the
    // prefix/suffix indicating the operating entity.  If the
    // right side is longer or equal, the left side is the prefix;
    // otherwise the right side is.  However, common non-prefix
    // suffixes (/P, /M, /QRP, /R, /A, /B) are ignored — return
    // the native call in that case.
    static const QStringList nonPrefixSuffix = {
        "P", "M", "QRP", "R", "A", "B", "MM", "AM"
    };

    int rightSize = call.size() - slash - 1;
    if (rightSize >= slash) {
        // Left part is shorter (or equal) → left is the prefix
        // e.g. F/IU8LMC → use "F" for lookup
        return left;
    } else {
        // Right part is shorter → right is the suffix
        // e.g. IU8LMC/VP9 → use "VP9" for lookup
        // But ignore non-prefix suffixes (IU8LMC/P → use "IU8LMC")
        if (nonPrefixSuffix.contains(right))
            return left;
        return right;
    }
}
