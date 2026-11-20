#include "CallsignIntelligenceService.h"
#include "ClubLogRestGuard.hpp"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTimeZone>
#include <QUrl>

namespace {
QHash<QByteArray, QByteArray> adifFields(const QByteArray& data)
{
    QHash<QByteArray, QByteArray> fields;
    qsizetype pos = 0;
    while ((pos = data.indexOf('<', pos)) >= 0) {
        const qsizetype end = data.indexOf('>', pos + 1);
        if (end < 0) break;
        const auto parts = data.mid(pos + 1, end - pos - 1).split(':');
        if (parts.size() < 2) { pos = end + 1; continue; }
        bool ok = false;
        const int length = parts.at(1).toInt(&ok);
        if (!ok || length < 0 || end + 1 + length > data.size()) break;
        fields.insert(parts.first().trimmed().toUpper(), data.mid(end + 1, length));
        pos = end + 1 + length;
    }
    return fields;
}

QJsonObject identityFilter(const QByteArray& adif)
{
    const auto fields = adifFields(adif);
    const QString dx = QString::fromUtf8(fields.value("CALL")).trimmed().toUpper();
    const QString date = QString::fromLatin1(fields.value("QSO_DATE"));
    const QString time = QString::fromLatin1(fields.value("TIME_ON"));
    QString band = QString::fromLatin1(fields.value("BAND")).trimmed().toLower();
    QString mode = QString::fromLatin1(fields.value("MODE")).trimmed().toUpper();
    const QString submode = QString::fromLatin1(fields.value("SUBMODE")).trimmed().toUpper();
    if (mode == QLatin1String("MFSK") && !submode.isEmpty()) mode = submode;
    if (band.endsWith(QLatin1String("m")) && !band.endsWith(QLatin1String("cm"))) band.chop(1);
    const QDate day = QDate::fromString(date, QStringLiteral("yyyyMMdd"));
    const QTime clock = QTime::fromString(time, time.size() == 4 ? QStringLiteral("HHmm") : QStringLiteral("HHmmss"));
    if (dx.isEmpty() || band.isEmpty() || mode.isEmpty() || !day.isValid() || !clock.isValid()
        || (time.size() != 4 && time.size() != 6)) return {};
    const QDateTime from(day, clock, QTimeZone::UTC);
    return {{QStringLiteral("dxcall"), dx}, {QStringLiteral("band"), band}, {QStringLiteral("mode"), mode},
            {QStringLiteral("from"), from.toString(Qt::ISODate)},
            {QStringLiteral("to"), from.addSecs(time.size() == 4 ? 59 : 0).toString(Qt::ISODate)}};
}
}

void CallsignIntelligenceService::setClubLogUploadEnabled(bool enabled)
{
    if (m_clubLogUploadEnabled == enabled) return;
    m_clubLogUploadEnabled = enabled;
    saveSetting(QStringLiteral("ClubLogUploadEnabled"), enabled);
    if (enabled) m_clubLogUploadTimer.start(1000);
    else m_clubLogUploadTimer.stop();
    refreshClubLogUploadStatus();
    emit settingsChanged();
}

void CallsignIntelligenceService::retryClubLogRejected()
{
    if (!m_database || !m_database->isOpen() || m_clubLogUploadReply) return;
    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("UPDATE clublog_outbox SET state='pending',last_error=NULL WHERE state='rejected' AND station=?"));
    query.addBindValue(m_operatorCallsign.trimmed().toUpper());
    if (!query.exec()) { refreshClubLogUploadStatus(tr("Could not update the Club Log queue")); return; }
    refreshClubLogUploadStatus();
    if (m_clubLogUploadEnabled) m_clubLogUploadTimer.start(1000);
}

void CallsignIntelligenceService::refreshClubLogUploadStatus(const QString& message)
{
    if (!m_database || !m_database->isOpen()) return;
    QSqlQuery query(*m_database);
    query.exec(QStringLiteral("SELECT state, COUNT(*) FROM clublog_outbox GROUP BY state"));
    int pending = 0, uncertain = 0, rejected = 0;
    while (query.next()) {
        const QString state = query.value(0).toString();
        if (state == QLatin1String("pending")) pending = query.value(1).toInt();
        else if (state == QLatin1String("uncertain")) uncertain = query.value(1).toInt();
        else if (state == QLatin1String("rejected")) rejected = query.value(1).toInt();
    }
    m_clubLogUploadStatus = tr("Club Log: %1 queued, %2 awaiting verification, %3 rejected").arg(pending).arg(uncertain).arg(rejected);
    if (rejected > 0) {
        QSqlQuery last(*m_database);
        if (last.exec(QStringLiteral("SELECT last_error FROM clublog_outbox WHERE state='rejected' AND last_error IS NOT NULL ORDER BY id DESC LIMIT 1")) && last.next())
            m_clubLogUploadStatus += QStringLiteral(" — ") + last.value(0).toString();
    }
    if (!message.isEmpty()) m_clubLogUploadStatus += QStringLiteral(" — ") + message;
    emit clubLogUploadStatusChanged();
}

void CallsignIntelligenceService::enqueueClubLogQso(const QByteArray& record, const QString& station)
{
    if (!m_clubLogUploadEnabled || !m_database || !m_database->isOpen()) return;
    const QString call = station.trimmed().toUpper();
    const QRegularExpression safeCall(QStringLiteral("^[A-Z0-9]+(?:/[A-Z0-9]+)*$"));
    if (!safeCall.match(call).hasMatch() || record.trimmed().isEmpty()) {
        refreshClubLogUploadStatus(tr("QSO not queued: station or contact data incomplete")); return;
    }
    QByteArray adif = record.trimmed();
    if (!adif.toUpper().endsWith("<EOR>")) adif += " <EOR>";
    const QString digest = QString::fromLatin1(QCryptographicHash::hash(adif, QCryptographicHash::Sha256).toHex());
    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO clublog_outbox(station,adif,digest,state) VALUES(?,?,?,'pending')"));
    query.addBindValue(call); query.addBindValue(adif); query.addBindValue(digest);
    if (!query.exec()) { refreshClubLogUploadStatus(tr("Could not save the QSO to the Club Log queue")); return; }
    refreshClubLogUploadStatus();
    if (!m_clubLogUploadTimer.isActive()) m_clubLogUploadTimer.start(20000);
}

void CallsignIntelligenceService::processClubLogOutbox()
{
    if (!m_clubLogUploadEnabled || m_offlineMode || m_clubLogUploadReply || !m_database || !m_database->isOpen()) return;
    if (!m_clubLogApiKey.startsWith(QLatin1String("cl_"))) { refreshClubLogUploadStatus(tr("Personal token required")); return; }
    loadClubLogRestRestriction();
    if (m_clubLogRestAuthRejected) { refreshClubLogUploadStatus(tr("Token refused; check Club Log settings")); return; }
    if (QDateTime::currentMSecsSinceEpoch() < m_clubLogRetryNotBeforeMs) { m_clubLogUploadTimer.start(300000); return; }
    const QString activeStation = m_operatorCallsign.trimmed().toUpper();
    if (activeStation.isEmpty()) { refreshClubLogUploadStatus(tr("Station callsign required")); return; }
    QSqlQuery first(*m_database);
    first.prepare(QStringLiteral("SELECT station FROM clublog_outbox WHERE state='pending' AND station=? ORDER BY id LIMIT 1"));
    first.addBindValue(activeStation);
    if (!first.exec() || !first.next()) {
        QSqlQuery uncertain(*m_database);
        uncertain.prepare(QStringLiteral("SELECT station FROM clublog_outbox WHERE state='uncertain' AND station=? ORDER BY id LIMIT 1"));
        uncertain.addBindValue(activeStation);
        if (uncertain.exec() && uncertain.next())
            verifyClubLogOutbox(uncertain.value(0).toString());
        return;
    }
    const QString station = first.value(0).toString();
    QSqlQuery selection(*m_database);
    selection.prepare(QStringLiteral("SELECT id,adif FROM clublog_outbox WHERE state='pending' AND station=? ORDER BY id LIMIT 20"));
    selection.addBindValue(station);
    if (!selection.exec()) return;
    QList<qint64> ids; QJsonArray qsos;
    while (selection.next()) {
        ids.append(selection.value(0).toLongLong());
        qsos.append(QJsonObject{{QStringLiteral("call"), station}, {QStringLiteral("adif"), QString::fromUtf8(selection.value(1).toByteArray())}});
    }
    if (ids.isEmpty() || !m_database->transaction()) return;
    bool claimed = true;
    for (qint64 id : ids) {
        QSqlQuery update(*m_database);
        update.prepare(QStringLiteral("UPDATE clublog_outbox SET state='uncertain' WHERE id=? AND state='pending'"));
        update.addBindValue(id);
        if (!update.exec() || update.numRowsAffected() != 1) { claimed = false; break; }
    }
    if (!claimed || !m_database->commit()) { m_database->rollback(); return; }
    const bool batch = ids.size() > 1;
    QNetworkRequest request(QUrl(batch ? QStringLiteral("https://clublog.org/api/v1/qso/batch") : QStringLiteral("https://clublog.org/api/v1/qso")));
    const QString token = m_clubLogApiKey;
    request.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(30000);
    const QJsonObject body = batch ? QJsonObject{{QStringLiteral("qsos"), qsos}} : qsos.first().toObject();
    m_clubLogUploadReply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(m_clubLogUploadReply, &QNetworkReply::finished, this, [this, ids, token, batch, station, reply = m_clubLogUploadReply] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto json = QJsonDocument::fromJson(reply->readAll()).object();
        QString errorMessage = json.value(QStringLiteral("message")).toString();
        if (errorMessage.isEmpty()) errorMessage = json.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
        if (errorMessage.isEmpty() && status >= 400) errorMessage = tr("HTTP %1").arg(status);
        errorMessage.replace(token, QStringLiteral("[redacted]"), Qt::CaseInsensitive);
        errorMessage = errorMessage.left(250);
        if ((status == 401 || status == 403) && token == m_clubLogApiKey) {
            m_clubLogRestAuthRejected = true;
            QSettings guard(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
            secure_settings::recordClubLogRestriction(guard, token, true, 0);
        }
        if (status == 429) noteClubLogRateLimit(reply);
        reply->deleteLater(); m_clubLogUploadReply = nullptr;
        QString state = QStringLiteral("uncertain"), remote;
        if (status == 401 || status == 403 || status == 429) state = QStringLiteral("pending");
        else if (status >= 400 && status < 500) state = QStringLiteral("rejected");
        else if (!batch && (status == 200 || status == 201)) {
            const QString outcome = json.value(QStringLiteral("status")).toString();
            remote = json.value(QStringLiteral("qsoid")).toVariant().toString();
            if (remote.toLongLong() > 0 && (outcome == QLatin1String("created") || outcome == QLatin1String("updated") || outcome == QLatin1String("duplicate")))
                state = QStringLiteral("sent");
            else if (outcome == QLatin1String("rejected")) state = QStringLiteral("rejected");
        }
        for (qint64 id : ids) {
            QSqlQuery update(*m_database);
            update.prepare(QStringLiteral("UPDATE clublog_outbox SET state=?,remote_id=?,last_error=? WHERE id=? AND state='uncertain'"));
            update.addBindValue(state); update.addBindValue(remote); update.addBindValue(errorMessage); update.addBindValue(id); update.exec();
        }
        refreshClubLogUploadStatus(status >= 400 ? tr("Club Log HTTP %1").arg(status) : QString());
        if (!m_clubLogUploadEnabled) return;
        if (m_clubLogApiKey != token || m_operatorCallsign.trimmed().toUpper() != station) {
            m_clubLogUploadTimer.start(1000); return;
        }
        if (status == 401 || status == 403) return;
        if (status == 429) { m_clubLogUploadTimer.start(300000); return; }
        if (state == QLatin1String("uncertain")) verifyClubLogOutbox(station);
        else m_clubLogUploadTimer.start(1000);
    });
    refreshClubLogUploadStatus(tr("Sending"));
}

void CallsignIntelligenceService::verifyClubLogOutbox(const QString& station)
{
    if (m_clubLogUploadReply || m_offlineMode || !m_clubLogUploadEnabled) return;
    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("SELECT id,adif FROM clublog_outbox WHERE station=? AND state='uncertain' ORDER BY id LIMIT 100"));
    query.addBindValue(station);
    if (!query.exec()) return;
    QJsonArray filters;
    QHash<QByteArray, qint64> positions;
    QHash<QByteArray, QJsonObject> filterObjects;
    QSet<QByteArray> ambiguous;
    while (query.next()) {
        const QJsonObject filter = identityFilter(query.value(1).toByteArray());
        const QByteArray key = QJsonDocument(filter).toJson(QJsonDocument::Compact);
        if (filter.isEmpty() || ambiguous.contains(key)) continue;
        if (positions.contains(key)) { positions.remove(key); filterObjects.remove(key); ambiguous.insert(key); continue; }
        positions.insert(key, query.value(0).toLongLong()); filterObjects.insert(key, filter);
    }
    for (auto it = filterObjects.cbegin(); it != filterObjects.cend(); ++it) filters.append(it.value());
    if (filters.isEmpty()) { refreshClubLogUploadStatus(tr("Contact identity is ambiguous")); return; }
    const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(station, "/"));
    QNetworkRequest request(QUrl(QStringLiteral("https://clublog.org/api/v1/logs/") + encoded + QStringLiteral("/qsos/query")));
    const QString token = m_clubLogApiKey;
    request.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(30000);
    m_clubLogUploadReply = m_network->post(request, QJsonDocument(QJsonObject{{QStringLiteral("filters"), filters}}).toJson(QJsonDocument::Compact));
    connect(m_clubLogUploadReply, &QNetworkReply::finished, this, [this, reply = m_clubLogUploadReply, positions, token, station] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
        if ((status == 401 || status == 403) && token == m_clubLogApiKey) {
            m_clubLogRestAuthRejected = true;
            QSettings guard(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
            secure_settings::recordClubLogRestriction(guard, token, true, 0);
        }
        if (status == 429) noteClubLogRateLimit(reply);
        reply->deleteLater(); m_clubLogUploadReply = nullptr;
        if (m_clubLogApiKey != token || m_operatorCallsign.trimmed().toUpper() != station) {
            if (m_clubLogUploadEnabled) m_clubLogUploadTimer.start(1000);
            return;
        }
        const QJsonValue resultsValue = document.object().value(QStringLiteral("results"));
        if (status == 200 && resultsValue.isArray() && resultsValue.toArray().size() == positions.size()) {
            QSet<QByteArray> seen; QSet<qint64> used; QList<QPair<qint64, qint64>> confirmed;
            bool valid = true;
            for (const auto& value : resultsValue.toArray()) {
                const auto row = value.toObject();
                const QByteArray key = QJsonDocument(row.value(QStringLiteral("filter")).toObject()).toJson(QJsonDocument::Compact);
                const auto matches = row.value(QStringLiteral("qsos"));
                if (!positions.contains(key) || seen.contains(key) || !matches.isArray() || matches.toArray().size() >= 100) { valid = false; break; }
                seen.insert(key);
                const auto rows = matches.toArray();
                if (rows.size() != 1) continue;
                bool ok = false; const qint64 remote = rows.first().toObject().value(QStringLiteral("qsoid")).toVariant().toLongLong(&ok);
                if (!ok || remote <= 0 || used.contains(remote)) { valid = false; break; }
                used.insert(remote); confirmed.append({positions.value(key), remote});
            }
            if (valid && seen.size() == positions.size()) {
                for (const auto& pair : confirmed) {
                    QSqlQuery update(*m_database);
                    update.prepare(QStringLiteral("UPDATE clublog_outbox SET state='sent',remote_id=? WHERE id=? AND state='uncertain'"));
                    update.addBindValue(QString::number(pair.second)); update.addBindValue(pair.first); update.exec();
                }
            }
        }
        refreshClubLogUploadStatus(status >= 400 ? tr("Club Log HTTP %1").arg(status) : QString());
        if (m_clubLogUploadEnabled && status != 401 && status != 403) m_clubLogUploadTimer.start(300000);
    });
}
