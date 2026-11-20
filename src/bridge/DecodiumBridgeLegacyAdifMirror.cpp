#include "DecodiumBridge.h"
#include "AdifDocumentInternal.h"
#include "MapIntelligenceService.h"
#include "CallsignIntelligenceService.h"
#include "Radio.hpp"
#include <algorithm>

using namespace decodium::adif_document;
void decodiumBridgeLogbookLog(const QString&);
void rebuildWorkedCallsFromDocument(QSet<QString>&, QList<ParsedAdifRecord> const&);

void DecodiumBridge::mirrorLegacyLoggedAdif(QByteArray const& adif)
{
    if (m_shuttingDown || adif.trimmed().isEmpty()) {
        return;
    }

    QList<ParsedAdifRecord> const sourceRecords = parseAdifRecordsBytes(adif);
    if (sourceRecords.isEmpty()) {
        decodiumBridgeLogbookLog(QStringLiteral("legacy ADIF mirror skipped: no parsable record"));
        return;
    }

    QString const activePath = ensureAdifLogPath();
    ParsedAdifDocument dest = loadAdifDocument(activePath);
    QSet<QString> existingKeys;
    for (ParsedAdifRecord const& record : dest.records) {
        QString const key = adifRecordDedupeKey(record.fields);
        if (!key.isEmpty()) {
            existingKeys.insert(key);
        }
    }

    int imported = 0;
    QList<QPair<QByteArray, QString>> clubLogRecords;
    QStringList importedCalls;
    QStringList const activeQsoCalls {
        m_dxCall,
        m_promptLogCall,
        m_pendingAutoLogCall,
        m_lateAutoLogCall,
    };
    QString const activeMode = m_mode.trimmed().toUpper();
    bool importedActiveQso = false;
    for (ParsedAdifRecord const& record : sourceRecords) {
        ParsedAdifRecord normalizedRecord = normalizeImportedAdifRecord(record);
        QString const key = adifRecordDedupeKey(normalizedRecord.fields);
        if (key.isEmpty() || existingKeys.contains(key)) {
            continue;
        }

        dest.records.append(normalizedRecord);
        QByteArray clubLogAdif;
        for (auto field = normalizedRecord.fields.cbegin(); field != normalizedRecord.fields.cend(); ++field) {
            const QByteArray value = field.value().toUtf8();
            clubLogAdif += '<' + field.key().toUtf8() + ':' + QByteArray::number(value.size()) + '>' + value + ' ';
        }
        clubLogAdif += "<EOR>";
        const QString station = normalizedRecord.fields.value(QStringLiteral("STATION_CALLSIGN"), m_callsign).trimmed();
        clubLogRecords.append({clubLogAdif, station.isEmpty() ? m_callsign : station});
        existingKeys.insert(key);
        ++imported;

        QString const call = normalizedRecord.fields.value(QStringLiteral("CALL")).trimmed().toUpper();
        if (!call.isEmpty() && !importedCalls.contains(call)) {
            importedCalls.append(call);
        }

        QString const recordMode = normalizedRecord.fields
                                       .value(QStringLiteral("SUBMODE"),
                                              normalizedRecord.fields.value(QStringLiteral("MODE")))
                                       .trimmed().toUpper();
        QString const baseCall = Radio::base_callsign(call).trimmed().toUpper();
        bool const activeCallMatch = std::any_of(activeQsoCalls.cbegin(), activeQsoCalls.cend(),
                                                  [&baseCall](QString const& activeCall) {
            return !baseCall.isEmpty()
                && Radio::base_callsign(activeCall).trimmed().toUpper() == baseCall;
        });
        if (activeCallMatch && (activeMode.isEmpty() || recordMode.isEmpty() || recordMode == activeMode)) {
            importedActiveQso = true;
        }
    }

    if (imported <= 0) {
        decodiumBridgeLogbookLog(QStringLiteral("legacy ADIF mirror skipped: duplicate or incomplete record"));
        return;
    }

    if (!writeAdifDocument(activePath, dest)) {
        emit errorMessage(QStringLiteral("Impossibile aggiornare il log ADIF attivo dal backend FT2: %1")
                              .arg(activePath));
        return;
    }
    if (m_callsignIntelligence)
        for (const auto& record : std::as_const(clubLogRecords))
            m_callsignIntelligence->enqueueClubLogQso(record.first, record.second);

    rebuildWorkedCallsFromDocument(m_workedCalls, dest.records);
    rebuildWorkedSetsFromAdifRecords(dest.records);
    m_qsoCountCache = dest.records.size();
    invalidateQsoSearchCache();
    warmLogCacheAsync();
    emit qsoCountChanged();
    emit workedCountChanged();
    emit qsoLogCacheChanged();
    if (m_mapIntelligenceService) {
        m_mapIntelligenceService->reloadFromAdif(activePath);
    }
    if (importedActiveQso && !m_qsoLogged) {
        // The embedded legacy state machine has already committed this QSO.
        // Mark it before the QML auto-sequence reaches its own logging branch.
        m_qsoLogged = true;
        decodiumBridgeLogbookLog(QStringLiteral("legacy ADIF matched active QSO; bridge duplicate log suppressed"));
    }
    decodiumBridgeLogbookLog(QStringLiteral("legacy ADIF mirrored to active logbook: imported=%1 total=%2 calls=%3 path=%4")
                  .arg(imported)
                  .arg(m_qsoCountCache)
                  .arg(importedCalls.join(QLatin1Char(',')))
                  .arg(activePath));
}

