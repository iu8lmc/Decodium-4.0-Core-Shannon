#include "DecodiumBridge.h"
#include "DecodiumBridgeWorkedHistory.h"
#include "AdifDocumentInternal.h"
#include "DxccLookup.h"
#include "Network/DecodiumDecoLogLink.h"
#include "Sequencer/MessageTokenRules.hpp"

#include <QDateTime>
#include <QVariantMap>

using namespace decodium::adif_document;
using decodium::seq::normalizedBaseCall;

QString workedCallBandModeKey(const QString& band, const QString& mode, const QString& call)
{
    const QString normalizedBand = band.trimmed().toLower();
    const QString normalizedMode = mode.trimmed().toUpper();
    const QString normalizedCall = call.trimmed().toUpper();
    if (normalizedBand.isEmpty() || normalizedMode.isEmpty() || normalizedCall.isEmpty()) {
        return {};
    }
    return normalizedBand + QLatin1Char('|') + normalizedMode + QLatin1Char('|') + normalizedCall;
}

QString workedCallBandKey(const QString& band, const QString& call)
{
    const QString normalizedBand = band.trimmed().toLower();
    const QString normalizedCall = call.trimmed().toUpper();
    if (normalizedBand.isEmpty() || normalizedCall.isEmpty()) {
        return {};
    }
    return normalizedBand + QLatin1Char('|') + normalizedCall;
}

static QString todayAdifDateTokenUtc()
{
    return QDateTime::currentDateTimeUtc().date().toString(QStringLiteral("yyyyMMdd"));
}

// 1.0.584: "worked today and yesterday" is the WSJT-X / JTDX quick filter the
// Settings switch mirrors.  Yesterday is always the calendar day before the
// current UTC date, so the set rolls over at 00:00Z like the log does.
static QString yesterdayAdifDateTokenUtc()
{
    return QDateTime::currentDateTimeUtc().date().addDays(-1)
        .toString(QStringLiteral("yyyyMMdd"));
}

void DecodiumBridge::appendWorkedQso(const QString& call, const QString& grid, quint64 freqHz,
                                     const QString& mode, const QString& qsoDateUtc)
{
    appendWorkedQsoBand(call, grid, freqHzToBandToken(freqHz), mode, qsoDateUtc);
}

void DecodiumBridge::appendWorkedQsoBand(const QString& call, const QString& grid, const QString& band,
                                         const QString& mode, const QString& qsoDateUtc)
{
    const QString upCall = call.trimmed().toUpper();
    if (upCall.isEmpty()) return;

    const QString callBandKey = workedCallBandKey(band, upCall);
    if (!band.isEmpty()) {
        m_worked.callByBand.insert(callBandKey);
        const QString bandModeKey = workedCallBandModeKey(band, mode, upCall);
        if (!bandModeKey.isEmpty()) {
            m_worked.callByBandMode.insert(bandModeKey);
        }
    }

    // 1.0.584: the date-scoped and ever sets are keyed by BASE call so that a
    // QSO logged as IK8OLM also covers a decode of IK8OLM/P, and vice versa.
    QString const baseCall = normalizedBaseCall(upCall);
    QString const baseBandKey = workedCallBandKey(band, baseCall);
    if (!baseCall.isEmpty()) {
        m_worked.callEver.insert(baseCall);
    }

    QString dateToken = normalizedAdifDateToken(qsoDateUtc);
    if (dateToken.isEmpty()) {
        dateToken = todayAdifDateTokenUtc();
    }
    if (!baseCall.isEmpty() && dateToken == todayAdifDateTokenUtc()) {
        m_worked.callToday.insert(baseCall);
        if (!baseBandKey.isEmpty()) {
            m_worked.callTodayByBand.insert(baseBandKey);
        }
    } else if (!baseCall.isEmpty() && dateToken == yesterdayAdifDateTokenUtc()) {
        m_worked.callYesterday.insert(baseCall);
        if (!baseBandKey.isEmpty()) {
            m_worked.callYesterdayByBand.insert(baseBandKey);
        }
    }

    if (m_dxccLookup && m_dxccLookup->isLoaded()) {
        const DxccEntity ent = m_dxccLookup->lookup(upCall);
        if (ent.isValid()) {
            m_worked.dxccEver.insert(ent.name);
            m_worked.continentEver.insert(ent.continent);
            if (ent.cqZone  > 0) m_worked.cqZoneEver.insert(ent.cqZone);
            if (ent.ituZone > 0) m_worked.ituZoneEver.insert(ent.ituZone);
            if (!band.isEmpty()) {
                m_worked.dxccByBand.insert(band + QLatin1Char('|') + ent.name);
                m_worked.continentByBand.insert(band + QLatin1Char('|') + ent.continent);
                if (ent.cqZone  > 0) m_worked.cqZoneByBand.insert(band + QLatin1Char('|') + QString::number(ent.cqZone));
                if (ent.ituZone > 0) m_worked.ituZoneByBand.insert(band + QLatin1Char('|') + QString::number(ent.ituZone));
            }
        }
    }

    const QString grid4 = grid.trimmed().left(4).toUpper();
    if (!grid4.isEmpty()) {
        m_worked.gridEver.insert(grid4);
        if (!band.isEmpty()) {
            m_worked.gridByBand.insert(band + QLatin1Char('|') + grid4);
        }
    }
}

void DecodiumBridge::applyDecoLogRows()
{
    // DecoLink aggiunge e non toglie: quello che c'e' nel file ADIF di Decodium resta.
    for (const DecoLogRow& row : m_decoLogRows) {
        m_workedCalls.insert(row.call);
        appendWorkedQsoBand(row.call, row.grid, row.band, row.mode, row.date);
    }
}

void DecodiumBridge::configureDecoLogLink()
{
    if (!m_decoLogLink)
        return;
    m_decoLogLink->setIdentity(version(), m_callsign.trimmed().toUpper());
    m_decoLogLink->setPort(static_cast<quint16>(m_decoLogLinkPort));
    m_decoLogLink->setEnabled(m_decoLogLinkEnabled);
    if (!m_decoLogLinkEnabled && m_decoLogConnected) {
        m_decoLogConnected = false;
        emit decoLogStateChanged();
    }
    if (!m_decoLogLinkEnabled) {
        m_decoLogStatus = QStringLiteral("DecoLink disattivato");
        emit decoLogStateChanged();
    }
}

void DecodiumBridge::setDecoLogLinkEnabled(bool v)
{
    if (m_decoLogLinkEnabled == v)
        return;
    m_decoLogLinkEnabled = v;
    configureDecoLogLink();
    emit decoLogLinkChanged();
}

void DecodiumBridge::setDecoLogLinkPort(int v)
{
    v = qBound(1, v, 65535);
    if (m_decoLogLinkPort == v)
        return;
    m_decoLogLinkPort = v;
    configureDecoLogLink();
    emit decoLogLinkChanged();
}

void DecodiumBridge::refreshWorkedBeforeDecodeEntriesForCall(const QString& call)
{
    const QString targetBase = normalizedBaseCall(call);
    if (targetBase.isEmpty())
        return;

    auto refreshList = [this, &targetBase](QVariantList& entries) {
        bool changed = false;
        for (QVariant& value : entries) {
            QVariantMap entry = value.toMap();
            if (entry.isEmpty() || entry.value(QStringLiteral("isSeparator")).toBool())
                continue;

            QString entryCall = entry.value(QStringLiteral("dxCallsign")).toString();
            if (entryCall.trimmed().isEmpty())
                entryCall = entry.value(QStringLiteral("fromCall")).toString();
            if (normalizedBaseCall(entryCall) != targetBase)
                continue;

            // Re-enrich only rows for the QSO just logged. This updates B4 and
            // the related worked/new flags immediately without replaying every
            // decode or inflating the performance counters.
            enrichDecodeEntry(entry, false);
            value = entry;
            changed = true;
        }
        return changed;
    };

    const bool bandChanged = refreshList(m_decodeList);
    const bool rxChanged = refreshList(m_rxDecodeList);
    if (!bandChanged && !rxChanged)
        return;

    invalidateDecodeUiPredicateCaches();
    if (bandChanged)
        rebuildBandActivityModel();
    if (bandChanged || rxChanged)
        rebuildRxDecodeModel();
}

void DecodiumBridge::rebuildWorkedSetsFromAdifRecords(QList<ParsedAdifRecord> const& records)
{
    m_worked.clear();
    QString const today = todayAdifDateTokenUtc();
    QString const yesterday = yesterdayAdifDateTokenUtc();
    for (ParsedAdifRecord const& record : records) {
        const QString call = record.fields.value(QStringLiteral("CALL")).trimmed().toUpper();
        if (call.isEmpty()) continue;

        // Band: prefer explicit BAND field (already a token like "20m"); fall
        // back to FREQ in MHz when missing or malformed.
        QString band = record.fields.value(QStringLiteral("BAND")).trimmed().toLower();
        if (band.isEmpty()) {
            const double freqMhz = record.fields.value(QStringLiteral("FREQ")).trimmed().toDouble();
            if (freqMhz > 0.0) {
                band = freqHzToBandToken(static_cast<quint64>(freqMhz * 1'000'000.0));
            }
        }
        const QString mode = normalizeAdifModeForDisplay(record.fields);

        const QString grid4 = record.fields.value(QStringLiteral("GRIDSQUARE")).trimmed().left(4).toUpper();
        const QString callBandKey = workedCallBandKey(band, call);

        if (!band.isEmpty()) {
            m_worked.callByBand.insert(callBandKey);
            const QString bandModeKey = workedCallBandModeKey(band, mode, call);
            if (!bandModeKey.isEmpty()) {
                m_worked.callByBandMode.insert(bandModeKey);
            }
        }
        QString const baseCall = normalizedBaseCall(call);
        QString const baseBandKey = workedCallBandKey(band, baseCall);
        if (!baseCall.isEmpty()) {
            m_worked.callEver.insert(baseCall);
        }
        QString const qsoDate = normalizedAdifDateToken(record.fields.value(QStringLiteral("QSO_DATE")));
        if (!baseCall.isEmpty() && qsoDate == today) {
            m_worked.callToday.insert(baseCall);
            if (!baseBandKey.isEmpty()) {
                m_worked.callTodayByBand.insert(baseBandKey);
            }
        } else if (!baseCall.isEmpty() && qsoDate == yesterday) {
            m_worked.callYesterday.insert(baseCall);
            if (!baseBandKey.isEmpty()) {
                m_worked.callYesterdayByBand.insert(baseBandKey);
            }
        }

        if (m_dxccLookup && m_dxccLookup->isLoaded()) {
            const DxccEntity ent = m_dxccLookup->lookup(call);
            if (ent.isValid()) {
                m_worked.dxccEver.insert(ent.name);
                m_worked.continentEver.insert(ent.continent);
                if (ent.cqZone  > 0) m_worked.cqZoneEver.insert(ent.cqZone);
                if (ent.ituZone > 0) m_worked.ituZoneEver.insert(ent.ituZone);
                if (!band.isEmpty()) {
                    m_worked.dxccByBand.insert(band + QLatin1Char('|') + ent.name);
                    m_worked.continentByBand.insert(band + QLatin1Char('|') + ent.continent);
                    if (ent.cqZone  > 0) m_worked.cqZoneByBand.insert(band + QLatin1Char('|') + QString::number(ent.cqZone));
                    if (ent.ituZone > 0) m_worked.ituZoneByBand.insert(band + QLatin1Char('|') + QString::number(ent.ituZone));
                }
            }
        }

        if (!grid4.isEmpty()) {
            m_worked.gridEver.insert(grid4);
            if (!band.isEmpty()) {
                m_worked.gridByBand.insert(band + QLatin1Char('|') + grid4);
            }
        }
    }
    applyDecoLogRows();
}


QStringList DecodiumBridge::workedCallsigns() const { return QStringList(m_workedCalls.values()); }
int         DecodiumBridge::workedCount()      const { return m_workedCalls.size(); }
