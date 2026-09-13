#include "CallsignIntelligenceService.h"

#include "DxccLookup.h"
#include "DecodiumProfileSettings.h"
#include "EqslInboxDownload.h"
#include "LotwReportResponse.h"
#include "SecureSettings.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTextStream>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QProcess>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>
#include <utility>

namespace {

QString normHeader(QString value)
{
    value = value.trimmed().toLower();
    value.remove(QRegularExpression(QStringLiteral("[^a-z0-9]+")));
    return value;
}

QString fieldFrom(const QStringList& headers,
                  const QStringList& row,
                  const QStringList& aliases)
{
    for (int i = 0; i < headers.size() && i < row.size(); ++i) {
        if (aliases.contains(normHeader(headers.at(i)))) {
            return row.at(i).trimmed();
        }
    }
    return {};
}

QStringList splitLine(const QString& line, QChar delimiter)
{
    QStringList result;
    QString current;
    bool quoted = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QLatin1Char('"')) {
            if (quoted && i + 1 < line.size() && line.at(i + 1) == ch) {
                current += ch;
                ++i;
            } else {
                quoted = !quoted;
            }
        } else if (ch == delimiter && !quoted) {
            result.append(current.trimmed());
            current.clear();
        } else {
            current += ch;
        }
    }
    result.append(current.trimmed());
    return result;
}

QChar delimiterFor(const QString& line)
{
    const QList<QPair<QChar, int>> candidates {
        {QLatin1Char('|'), line.count(QLatin1Char('|'))},
        {QLatin1Char(','), line.count(QLatin1Char(','))},
        {QLatin1Char(';'), line.count(QLatin1Char(';'))},
        {QLatin1Char('\t'), line.count(QLatin1Char('\t'))}
    };
    QChar best = QLatin1Char(',');
    int count = 0;
    for (const auto& candidate : candidates) {
        if (candidate.second > count) {
            best = candidate.first;
            count = candidate.second;
        }
    }
    return best;
}

bool truthy(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    return normalized == QStringLiteral("1")
        || normalized == QStringLiteral("y")
        || normalized == QStringLiteral("yes")
        || normalized == QStringLiteral("true")
        || normalized == QStringLiteral("ag")
        || normalized == QStringLiteral("ok");
}

QString jsonString(const QVariantMap& map)
{
    return QString::fromUtf8(QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact));
}

QString firstNonEmpty(const QVariantMap& value, const QString& key)
{
    return value.value(key).toString().trimmed();
}

QString qrzFormValue(const QByteArray& payload, const QString& key)
{
    const QList<QByteArray> fields = payload.split('&');
    const QByteArray wanted = key.toUtf8();
    for (const QByteArray& field : fields) {
        const int separator = field.indexOf('=');
        const QByteArray name = separator >= 0 ? field.left(separator) : field;
        if (name != wanted) continue;
        QByteArray encoded = separator >= 0 ? field.mid(separator + 1) : QByteArray();
        encoded.replace('+', ' ');
        return QUrl::fromPercentEncoding(encoded);
    }
    return {};
}

qint64 highestQrzLogId(const QString& adif)
{
    qint64 highest = -1;
    const QRegularExpression tag(QStringLiteral("(?i)<APP_QRZLOG_LOGID:\\d+>([^<]*)"));
    auto iterator = tag.globalMatch(adif);
    while (iterator.hasNext()) {
        const qint64 value = iterator.next().captured(1).trimmed().toLongLong();
        if (value > highest) highest = value;
    }
    return highest;
}

QString adifHeaderValue(const QByteArray& payload, const QString& field)
{
    const QByteArray lower = payload.toLower();
    const int eoh = lower.indexOf("<eoh>");
    const QByteArray header = eoh >= 0 ? payload.left(eoh + 5) : payload.left(8192);
    const QRegularExpression expression(QStringLiteral("(?i)<%1:\\d+>([^<]*)")
                                             .arg(QRegularExpression::escape(field)));
    const QRegularExpressionMatch match = expression.match(QString::fromUtf8(header));
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QByteArray extractFccEnDatPayload(const QByteArray& data)
{
    if (!data.startsWith("PK")) return data;

    // Feed unzip a real temporary archive.  Passing the ZIP bytes to stdin
    // with "unzip -p -" is not portable across the BSD/GNU unzip variants
    // shipped on the supported platforms.  This helper is also used by the
    // worker-thread import, so the potentially slow decompression never runs
    // on the GUI thread.
    QTemporaryFile archive;
    if (!archive.open()) return {};
    if (archive.write(data) != data.size() || !archive.flush()) return {};
    QProcess process;
    process.start(QStringLiteral("unzip"), {QStringLiteral("-p"), archive.fileName(), QStringLiteral("EN.dat")});
    if (!process.waitForStarted(3000)) return {};
    if (!process.waitForFinished(30000) || process.exitCode() != 0) return {};
    return process.readAllStandardOutput();
}

QVariantMap importProviderPayloadInBackground(const QString& provider,
                                              const QString& databasePath,
                                              const QByteArray& data)
{
    QVariantMap result;
    QByteArray payload = data;
    if (provider == QStringLiteral("fcc_uls")) {
        payload = extractFccEnDatPayload(data);
        if (payload.isEmpty()) {
            result.insert(QStringLiteral("error"), QStringLiteral("FCC ULS: EN.dat non disponibile nell'archivio"));
            return result;
        }
    }

    const QString connectionName = QStringLiteral("decodium_database_import_%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    database.setDatabaseName(databasePath);
    if (!database.open()) {
        result.insert(QStringLiteral("error"), database.lastError().text());
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase(connectionName);
        return result;
    }

    int imported = 0;
    bool committed = false;
    QString error;
    {
        QSqlQuery query(database);
        if (!database.transaction()) {
            error = database.lastError().text();
        } else if (!query.prepare(QStringLiteral("INSERT OR REPLACE INTO callsign_records(provider,callsign,grid,name,qth,country,dxcc,continent,state,county,last_upload,lotw,eqsl,oqrs,confirmed,metadata_json,updated_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"))) {
            error = query.lastError().text();
            database.rollback();
        } else {
            const qint64 updatedAt = QDateTime::currentMSecsSinceEpoch();
            const QRegularExpression invalidCall(QStringLiteral("[^A-Z0-9/ -]"));
            const auto importRecord = [&](QVariantMap record) {
                QString call = record.value(QStringLiteral("call")).toString().trimmed().toUpper();
                if (call.startsWith(QStringLiteral("CQ "))) call = call.mid(3).trimmed();
                if (call.isEmpty() || call.size() > 32 || invalidCall.match(call).hasMatch()) return;

                query.bindValue(0, provider);
                query.bindValue(1, call);
                query.bindValue(2, record.value(QStringLiteral("grid")).toString().trimmed().toUpper());
                query.bindValue(3, record.value(QStringLiteral("name")).toString().trimmed());
                query.bindValue(4, record.value(QStringLiteral("qth")).toString().trimmed());
                query.bindValue(5, record.value(QStringLiteral("country")).toString().trimmed());
                query.bindValue(6, record.value(QStringLiteral("dxcc")).toString().trimmed());
                query.bindValue(7, record.value(QStringLiteral("continent")).toString().trimmed());
                query.bindValue(8, record.value(QStringLiteral("state")).toString().trimmed());
                query.bindValue(9, record.value(QStringLiteral("county")).toString().trimmed());
                query.bindValue(10, record.value(QStringLiteral("lastUpload")).toString().trimmed());
                query.bindValue(11, record.value(QStringLiteral("lotw")).toBool() ? 1 : 0);
                query.bindValue(12, record.value(QStringLiteral("eqsl")).toBool() ? 1 : 0);
                query.bindValue(13, record.value(QStringLiteral("oqrs")).toBool() ? 1 : 0);
                query.bindValue(14, record.value(QStringLiteral("confirmed")).toBool() ? 1 : 0);
                query.bindValue(15, jsonString(record));
                query.bindValue(16, updatedAt);
                if (query.exec()) ++imported;
            };

            if (provider == QStringLiteral("fcc_uls")) {
                const QRegularExpression callPattern(QStringLiteral("^[A-Z0-9]{1,3}[0-9][A-Z0-9]{1,5}(/[A-Z0-9]+)?$"));
                QString text = QString::fromUtf8(payload);
                QTextStream stream(&text, QIODevice::ReadOnly);
                stream.setEncoding(QStringConverter::Utf8);
                while (!stream.atEnd()) {
                    const QString line = stream.readLine().trimmed();
                    const QStringList fields = line.split(QLatin1Char('|'));
                    if (fields.isEmpty() || fields.first().trimmed().toUpper() != QStringLiteral("EN")) continue;

                    QString call;
                    for (const QString& field : fields) {
                        const QString candidate = field.trimmed().toUpper();
                        if (candidate.size() >= 4 && callPattern.match(candidate).hasMatch()) {
                            call = candidate;
                            break;
                        }
                    }
                    if (call.isEmpty()) continue;
                    QVariantMap record;
                    record.insert(QStringLiteral("call"), call);
                    record.insert(QStringLiteral("name"), fields.value(7).trimmed());
                    record.insert(QStringLiteral("qth"), fields.value(10).trimmed());
                    record.insert(QStringLiteral("state"), fields.value(11).trimmed());
                    record.insert(QStringLiteral("metadata"), line);
                    importRecord(record);
                }
            } else if (provider == QStringLiteral("clublog_oqrs")) {
                const QJsonDocument document = QJsonDocument::fromJson(payload);
                if (!document.isArray()) {
                    error = QStringLiteral("Formato Club Log OQRS non riconosciuto");
                } else {
                    for (const QJsonValue& value : document.array()) {
                        const QJsonArray row = value.toArray();
                        if (row.size() < 4) continue;
                        QVariantMap record;
                        record.insert(QStringLiteral("call"), row.at(0).toString());
                        record.insert(QStringLiteral("lastUpload"), row.at(1).toString());
                        record.insert(QStringLiteral("oqrs"), true);
                        record.insert(QStringLiteral("confirmed"), true);
                        record.insert(QStringLiteral("metadata"), QStringLiteral("band=%1 mode=%2").arg(row.at(2).toString(), row.at(3).toString()));
                        importRecord(record);
                    }
                }
            } else {
                QString text = QString::fromUtf8(payload);
                QTextStream stream(&text, QIODevice::ReadOnly);
                stream.setEncoding(QStringConverter::Utf8);
                const QString first = stream.readLine();
                if (first.isNull()) {
                    error = QStringLiteral("File provider vuoto");
                } else {
                    const QChar delimiter = delimiterFor(first);
                    QStringList headers = splitLine(first, delimiter);
                    bool hasHeader = false;
                    for (const QString& header : headers) {
                        const QString normalized = normHeader(header);
                        if (normalized == QStringLiteral("call") || normalized == QStringLiteral("callsign")
                            || normalized == QStringLiteral("gridsquare") || normalized == QStringLiteral("licenseename")) {
                            hasHeader = true;
                            break;
                        }
                    }
                    const auto importRow = [&](const QStringList& row, const QStringList& effectiveHeaders) {
                        QString call = fieldFrom(effectiveHeaders, row, {QStringLiteral("call"), QStringLiteral("callsign"), QStringLiteral("matchedcallsign"), QStringLiteral("username")});
                        if (call.isEmpty() && !row.isEmpty()) call = row.first();
                        QVariantMap record;
                        record.insert(QStringLiteral("call"), call);
                        record.insert(QStringLiteral("grid"), fieldFrom(effectiveHeaders, row, {QStringLiteral("grid"), QStringLiteral("gridsquare"), QStringLiteral("locator"), QStringLiteral("qra")}));
                        record.insert(QStringLiteral("name"), fieldFrom(effectiveHeaders, row, {QStringLiteral("name"), QStringLiteral("fullname"), QStringLiteral("licensename"), QStringLiteral("operator")}));
                        record.insert(QStringLiteral("qth"), fieldFrom(effectiveHeaders, row, {QStringLiteral("qth"), QStringLiteral("city"), QStringLiteral("address"), QStringLiteral("location")}));
                        record.insert(QStringLiteral("state"), fieldFrom(effectiveHeaders, row, {QStringLiteral("state"), QStringLiteral("st"), QStringLiteral("usstate")}));
                        record.insert(QStringLiteral("county"), fieldFrom(effectiveHeaders, row, {QStringLiteral("county"), QStringLiteral("countrycounty")}));
                        if (provider == QStringLiteral("lotw")) {
                            record.insert(QStringLiteral("lotw"), true);
                            if (row.size() > 1) record.insert(QStringLiteral("lastUpload"), row.at(1));
                        } else if (provider == QStringLiteral("eqsl")) {
                            record.insert(QStringLiteral("eqsl"), true);
                        }
                        importRecord(record);
                    };

                    if (hasHeader) {
                        while (!stream.atEnd()) importRow(splitLine(stream.readLine(), delimiter), headers);
                    } else {
                        headers = {QStringLiteral("CALLSIGN"), QStringLiteral("LAST_UPLOAD")};
                        importRow(splitLine(first, delimiter), headers);
                        while (!stream.atEnd()) importRow(splitLine(stream.readLine(), delimiter), headers);
                    }
                }
            }
            if (!database.commit()) {
                committed = false;
                if (error.isEmpty()) error = database.lastError().text();
            } else {
                committed = true;
            }
        }
    }

    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);

    result.insert(QStringLiteral("ok"), committed && imported > 0);
    result.insert(QStringLiteral("imported"), imported);
    if (!error.isEmpty()) result.insert(QStringLiteral("error"), error);
    return result;
}

} // namespace

CallsignIntelligenceService::CallsignIntelligenceService(QObject* parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_database(new QSqlDatabase)
{
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    m_databasePath = QDir(appData).absoluteFilePath(QStringLiteral("callsign-intelligence.sqlite"));
    m_connectionName = QStringLiteral("decodium_callsign_intelligence_%1")
                           .arg(reinterpret_cast<quintptr>(this));
    *m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database->setDatabaseName(m_databasePath);

    m_specs.insert(QStringLiteral("fcc_uls"), {QStringLiteral("fcc_uls"), QStringLiteral("FCC ULS"),
                                                QStringLiteral("https://data.fcc.gov/download/pub/uls/complete/l_amat.zip"), true, true});
    m_specs.insert(QStringLiteral("lotw"), {QStringLiteral("lotw"), QStringLiteral("LoTW - User activity"),
                                             QStringLiteral("https://lotw.arrl.org/lotw-user-activity.csv"), true, true});
    m_specs.insert(QStringLiteral("lotw_confirmed"), {QStringLiteral("lotw_confirmed"), QStringLiteral("LoTW - Confirmations received"),
                                             QStringLiteral("https://lotw.arrl.org/lotwuser/lotwreport.adi"), true, true});
    m_specs.insert(QStringLiteral("eqsl"), {QStringLiteral("eqsl"), QStringLiteral("eQSL AG"),
                                             QStringLiteral("https://eqsl.cc/DownloadedFiles/eQSLMemberList.csv"), true, true});
    m_specs.insert(QStringLiteral("eqsl_inbox"), {QStringLiteral("eqsl_inbox"), QStringLiteral("eQSL InBox - Confirmations received"),
                                             QStringLiteral("https://www.eqsl.cc/qslcard/DownloadInbox.cfm"), true, true});
    m_specs.insert(QStringLiteral("qrz_confirmed"), {QStringLiteral("qrz_confirmed"), QStringLiteral("QRZ.com - Confirmations received"),
                                             QStringLiteral("https://logbook.qrz.com/api"), true, true});
    m_specs.insert(QStringLiteral("clublog_oqrs"), {QStringLiteral("clublog_oqrs"), QStringLiteral("Club Log OQRS"),
                                                     QStringLiteral("https://clublog.org/getoqrsmatches.php"), true, true});
    m_specs.insert(QStringLiteral("dxcc"), {QStringLiteral("dxcc"), QStringLiteral("DXCC cty.dat"), {}, true, false});

    openDatabase();
    loadSettings();
    setStatus(tr("Pronto: database locale callsign disponibile"));
}

CallsignIntelligenceService::~CallsignIntelligenceService()
{
    if (m_databaseImportWatcher) {
        m_databaseImportWatcher->disconnect(this);
        m_databaseImportWatcher->waitForFinished();
        delete m_databaseImportWatcher;
        m_databaseImportWatcher = nullptr;
    }
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
    if (m_database && m_database->isOpen()) {
        m_database->close();
    }
    if (!m_connectionName.isEmpty()) {
        // Drop the last QSqlDatabase handle before removing the named
        // connection; this avoids Qt's "connection is still in use" warning
        // during application shutdown.
        if (m_database)
            *m_database = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    delete m_database;
}

bool CallsignIntelligenceService::openDatabase()
{
    if (!m_database || !m_database->open()) {
        setStatus(tr("Database callsign non disponibile: %1").arg(m_database ? m_database->lastError().text() : QString()));
        return false;
    }
    createSchema();
    return true;
}

void CallsignIntelligenceService::createSchema()
{
    if (!m_database || !m_database->isOpen()) return;
    QSqlQuery query(*m_database);
    query.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS callsign_records ("
                             "provider TEXT NOT NULL, callsign TEXT NOT NULL, grid TEXT, name TEXT, qth TEXT, "
                             "country TEXT, dxcc TEXT, continent TEXT, state TEXT, county TEXT, "
                             "last_upload TEXT, lotw INTEGER NOT NULL DEFAULT 0, eqsl INTEGER NOT NULL DEFAULT 0, "
                             "oqrs INTEGER NOT NULL DEFAULT 0, confirmed INTEGER NOT NULL DEFAULT 0, "
                             "metadata_json TEXT, updated_at INTEGER NOT NULL, PRIMARY KEY(provider,callsign))"));
    query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS callsign_records_call ON callsign_records(callsign)"));
    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS callsign_cache ("
                             "callsign TEXT PRIMARY KEY, provider TEXT, payload_json TEXT NOT NULL, "
                             "updated_at INTEGER NOT NULL, expires_at INTEGER NOT NULL)"));
    query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS callsign_provider_state ("
                             "provider TEXT PRIMARY KEY, source_url TEXT, local_path TEXT, row_count INTEGER NOT NULL DEFAULT 0, "
                             "updated_at INTEGER NOT NULL DEFAULT 0, status TEXT, error TEXT)"));
    for (const ProviderSpec& spec : std::as_const(m_specs)) {
        QSqlQuery insert(*m_database);
        insert.prepare(QStringLiteral("INSERT OR IGNORE INTO callsign_provider_state(provider,source_url,status) VALUES(?,?,?)"));
        insert.addBindValue(spec.id);
        insert.addBindValue(spec.url);
        insert.addBindValue(spec.updateable ? tr("Never updated") : tr("Local"));
        insert.exec();
    }
}

void CallsignIntelligenceService::loadSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.beginGroup(QStringLiteral("CallsignIntelligence"));
    m_autoOpenOnQsoStart = settings.value(QStringLiteral("AutoOpenOnQsoStart"), false).toBool();
    m_autoCloseAfterLogging = settings.value(QStringLiteral("AutoCloseAfterLogging"), false).toBool();
    m_enrichMissingFields = settings.value(QStringLiteral("EnrichMissingFields"), false).toBool();
    m_cacheTtlMinutes = qBound(5, settings.value(QStringLiteral("CacheTtlMinutes"), 1440).toInt(), 10080);
    m_eqslUsername = settings.value(QStringLiteral("EqslUsername")).toString().trimmed().toUpper();
    // LoTW documents this as a username, not necessarily a callsign.  Do not
    // alter its case while loading a saved account name.
    m_lotwUsername = settings.value(QStringLiteral("LotwUsername")).toString().trimmed();
    m_lotwLastQsl = settings.value(QStringLiteral("LotwLastQsl")).toString().trimmed();
    m_clubLogEmail = settings.value(QStringLiteral("ClubLogEmail")).toString();
    const QString secureService = secure_settings::service(QStringLiteral("CALLSIGN_INTELLIGENCE"));
    m_eqslPassword = secure_settings::load_or_import(&settings, secureService,
                                                      QStringLiteral("EqslPassword"),
                                                      settings.value(QStringLiteral("EqslPassword")).toString());
    m_clubLogApiKey = secure_settings::load_or_import(&settings, secureService,
                                                       QStringLiteral("ClubLogApiKey"),
                                                       settings.value(QStringLiteral("ClubLogApiKey")).toString()).trimmed();
    m_clubLogApplicationPassword = secure_settings::load_or_import(&settings, secureService,
                                                                     QStringLiteral("ClubLogApplicationPassword"),
                                                                     settings.value(QStringLiteral("ClubLogApplicationPassword")).toString()).trimmed();
    settings.endGroup();
}

void CallsignIntelligenceService::saveSetting(const QString& key, const QVariant& value)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.beginGroup(QStringLiteral("CallsignIntelligence"));
    if (key == QStringLiteral("EqslPassword")
        || key == QStringLiteral("ClubLogApiKey")
        || key == QStringLiteral("ClubLogApplicationPassword")) {
        settings.setValue(key, secure_settings::value_for_write(secure_settings::service(QStringLiteral("CALLSIGN_INTELLIGENCE")), key, value.toString()));
    } else {
        settings.setValue(key, value);
    }
    settings.sync();
    settings.endGroup();
}

void CallsignIntelligenceService::setAutoOpenOnQsoStart(bool value)
{
    if (m_autoOpenOnQsoStart == value) return;
    m_autoOpenOnQsoStart = value;
    saveSetting(QStringLiteral("AutoOpenOnQsoStart"), value);
    emit settingsChanged();
}

void CallsignIntelligenceService::setAutoCloseAfterLogging(bool value)
{
    if (m_autoCloseAfterLogging == value) return;
    m_autoCloseAfterLogging = value;
    saveSetting(QStringLiteral("AutoCloseAfterLogging"), value);
    emit settingsChanged();
}

void CallsignIntelligenceService::setEnrichMissingFields(bool value)
{
    if (m_enrichMissingFields == value) return;
    m_enrichMissingFields = value;
    saveSetting(QStringLiteral("EnrichMissingFields"), value);
    emit settingsChanged();
}

void CallsignIntelligenceService::setCacheTtlMinutes(int value)
{
    const int bounded = qBound(5, value, 10080);
    if (m_cacheTtlMinutes == bounded) return;
    m_cacheTtlMinutes = bounded;
    saveSetting(QStringLiteral("CacheTtlMinutes"), bounded);
    emit settingsChanged();
}

void CallsignIntelligenceService::setOperatorCallsign(const QString& value)
{
    const QString clean = value.trimmed().toUpper();
    if (m_operatorCallsign == clean) return;
    m_operatorCallsign = clean;
    emit settingsChanged();
}

void CallsignIntelligenceService::setEqslUsername(const QString& value)
{
    const QString clean = value.trimmed().toUpper();
    if (m_eqslUsername == clean) return;
    m_eqslUsername = clean;
    saveSetting(QStringLiteral("EqslUsername"), clean);
    emit settingsChanged();
}

void CallsignIntelligenceService::setEqslPassword(const QString& value)
{
    if (m_eqslPassword == value) return;
    m_eqslPassword = value;
    saveSetting(QStringLiteral("EqslPassword"), value);
    emit settingsChanged();
}

void CallsignIntelligenceService::setLotwUsername(const QString& value)
{
    const QString clean = value.trimmed();
    if (m_lotwUsername == clean) return;
    m_lotwUsername = clean;
    saveSetting(QStringLiteral("LotwUsername"), clean);
    emit settingsChanged();
}

void CallsignIntelligenceService::setLotwPassword(const QString& value)
{
    // Passwords are opaque values: trimming would make a valid password with
    // a leading or trailing space impossible to use.
    m_lotwPassword = value;
}

void CallsignIntelligenceService::setQrzApiKey(const QString& value)
{
    m_qrzApiKey = value.trimmed();
}

void CallsignIntelligenceService::setClubLogApiKey(const QString& value)
{
    const QString clean = value.trimmed();
    if (m_clubLogApiKey == clean) return;
    m_clubLogApiKey = clean;
    saveSetting(QStringLiteral("ClubLogApiKey"), clean);
    emit settingsChanged();
}

void CallsignIntelligenceService::setClubLogEmail(const QString& value)
{
    const QString clean = value.trimmed();
    if (m_clubLogEmail == clean) return;
    m_clubLogEmail = clean;
    saveSetting(QStringLiteral("ClubLogEmail"), clean);
    emit settingsChanged();
}

void CallsignIntelligenceService::setClubLogApplicationPassword(const QString& value)
{
    const QString clean = value.trimmed();
    if (m_clubLogApplicationPassword == clean) return;
    m_clubLogApplicationPassword = clean;
    saveSetting(QStringLiteral("ClubLogApplicationPassword"), clean);
    emit settingsChanged();
}

void CallsignIntelligenceService::setDxccLookup(DxccLookup* lookup)
{
    m_dxccLookup = lookup;
}

void CallsignIntelligenceService::notifyDxccDataChanged()
{
    emit databasesChanged();
}

QString CallsignIntelligenceService::normalizeCall(const QString& value) const
{
    QString result = value.trimmed().toUpper();
    if (result.startsWith(QStringLiteral("CQ "))) result = result.mid(3).trimmed();
    if (result.size() > 32 || result.contains(QRegularExpression(QStringLiteral("[^A-Z0-9/ -]")))) return {};
    return result;
}

void CallsignIntelligenceService::setStatus(const QString& value)
{
    if (m_status == value) return;
    m_status = value;
    emit statusChanged();
    if (m_databaseUpdatePending && !m_updateProvider.isEmpty()) {
        emit databasesChanged();
    }
}

void CallsignIntelligenceService::setPending(bool value)
{
    if (m_lookupPending == value) return;
    m_lookupPending = value;
    emit lookupPendingChanged();
}

void CallsignIntelligenceService::setDatabaseUpdatePending(bool value)
{
    if (m_databaseUpdatePending == value) return;
    m_databaseUpdatePending = value;
    emit databaseUpdatePendingChanged();
}

QVariantMap CallsignIntelligenceService::mergeRecord(QVariantMap target, const QVariantMap& source) const
{
    static const QStringList fields {
        QStringLiteral("grid"), QStringLiteral("name"), QStringLiteral("qth"), QStringLiteral("country"),
        QStringLiteral("dxcc"), QStringLiteral("continent"), QStringLiteral("state"), QStringLiteral("county"),
        QStringLiteral("lastUpload")
    };
    for (const QString& key : fields) {
        if (firstNonEmpty(target, key).isEmpty() && !firstNonEmpty(source, key).isEmpty()) {
            target.insert(key, source.value(key));
        }
    }
    for (const QString& key : {QStringLiteral("lotw"), QStringLiteral("eqsl"), QStringLiteral("oqrs"), QStringLiteral("confirmed")}) {
        if (source.value(key).toBool()) target.insert(key, true);
    }
    QStringList providers = target.value(QStringLiteral("providers")).toStringList();
    const QString provider = source.value(QStringLiteral("provider")).toString();
    if (!provider.isEmpty() && !providers.contains(provider)) providers.append(provider);
    target.insert(QStringLiteral("providers"), providers);
    return target;
}

QVariantMap CallsignIntelligenceService::localLookup(const QString& callsign) const
{
    QVariantMap result;
    if (!m_database || !m_database->isOpen()) return result;
    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("SELECT provider,callsign,grid,name,qth,country,dxcc,continent,state,county,last_upload,lotw,eqsl,oqrs,confirmed,metadata_json "
                                 "FROM callsign_records WHERE callsign=? ORDER BY CASE provider WHEN 'fcc_uls' THEN 1 WHEN 'eqsl' THEN 2 WHEN 'lotw' THEN 3 WHEN 'clublog_oqrs' THEN 4 ELSE 9 END"));
    query.addBindValue(callsign);
    if (!query.exec()) return result;
    while (query.next()) {
        QVariantMap row;
        row.insert(QStringLiteral("provider"), query.value(0).toString());
        row.insert(QStringLiteral("call"), query.value(1).toString());
        row.insert(QStringLiteral("grid"), query.value(2).toString());
        row.insert(QStringLiteral("name"), query.value(3).toString());
        row.insert(QStringLiteral("qth"), query.value(4).toString());
        row.insert(QStringLiteral("country"), query.value(5).toString());
        row.insert(QStringLiteral("dxcc"), query.value(6).toString());
        row.insert(QStringLiteral("continent"), query.value(7).toString());
        row.insert(QStringLiteral("state"), query.value(8).toString());
        row.insert(QStringLiteral("county"), query.value(9).toString());
        row.insert(QStringLiteral("lastUpload"), query.value(10).toString());
        row.insert(QStringLiteral("lotw"), query.value(11).toBool());
        row.insert(QStringLiteral("eqsl"), query.value(12).toBool());
        row.insert(QStringLiteral("oqrs"), query.value(13).toBool());
        row.insert(QStringLiteral("confirmed"), query.value(14).toBool());
        result = mergeRecord(result, row);
    }
    if (!result.isEmpty()) {
        result.insert(QStringLiteral("call"), callsign);
        result.insert(QStringLiteral("provider"), result.value(QStringLiteral("providers")).toStringList().join(QStringLiteral(", ")));
    }
    if (m_dxccLookup && m_dxccLookup->isLoaded()) {
        const DxccEntity entity = m_dxccLookup->lookup(callsign);
        if (entity.isValid()) {
            if (firstNonEmpty(result, QStringLiteral("dxcc")).isEmpty()) result.insert(QStringLiteral("dxcc"), entity.name);
            if (firstNonEmpty(result, QStringLiteral("country")).isEmpty()) result.insert(QStringLiteral("country"), entity.name);
            if (firstNonEmpty(result, QStringLiteral("continent")).isEmpty()) result.insert(QStringLiteral("continent"), entity.continent);
            result.insert(QStringLiteral("cqZone"), entity.cqZone);
            result.insert(QStringLiteral("ituZone"), entity.ituZone);
        }
    }
    return result;
}

QVariantMap CallsignIntelligenceService::cachedLookup(const QString& callsign) const
{
    QVariantMap empty;
    if (!m_database || !m_database->isOpen()) return empty;
    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("SELECT payload_json,expires_at FROM callsign_cache WHERE callsign=?"));
    query.addBindValue(callsign);
    if (!query.exec() || !query.next()) return empty;
    if (query.value(1).toLongLong() < QDateTime::currentMSecsSinceEpoch()) return empty;
    const QJsonDocument document = QJsonDocument::fromJson(query.value(0).toString().toUtf8());
    return document.isObject() ? document.object().toVariantMap() : empty;
}

void CallsignIntelligenceService::cacheResult(const QVariantMap& value)
{
    if (!m_database || !m_database->isOpen() || value.value(QStringLiteral("call")).toString().isEmpty()) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO callsign_cache(callsign,provider,payload_json,updated_at,expires_at) VALUES(?,?,?,?,?)"));
    query.addBindValue(value.value(QStringLiteral("call")).toString());
    query.addBindValue(value.value(QStringLiteral("provider")).toString());
    query.addBindValue(jsonString(value));
    query.addBindValue(now);
    query.addBindValue(now + static_cast<qint64>(m_cacheTtlMinutes) * 60000);
    query.exec();
}

void CallsignIntelligenceService::finishLookup(const QVariantMap& value, bool fromCache, const QString& status)
{
    m_result = value;
    m_result.insert(QStringLiteral("cacheHit"), fromCache);
    m_result.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    m_currentCall = value.value(QStringLiteral("call"), m_currentCall).toString();
    setPending(false);
    setStatus(status);
    emit currentCallChanged();
    emit resultChanged();
    if (m_enrichMissingFields && !m_result.isEmpty()) {
        emit enrichmentReady(m_currentCall, m_result);
    }
}

void CallsignIntelligenceService::setOfflineMode(bool offline)
{
    if (m_offlineMode == offline) {
        return;
    }
    m_offlineMode = offline;
    if (offline) {
        if (m_activeReply) {
            m_activeReply->abort();
        }
        setDatabaseUpdatePending(false);
        setPending(false);
        setStatus(tr("Offline: solo cache e database callsign locali"));
    } else {
        setStatus(tr("Online: provider remoto callsign abilitati"));
    }
    emit offlineModeChanged();
}

void CallsignIntelligenceService::lookup(const QString& callsign, bool forceRefresh)
{
    const QString call = normalizeCall(callsign);
    if (call.isEmpty()) {
        setStatus(tr("Callsign non valido"));
        return;
    }
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
    m_currentCall = call;
    emit currentCallChanged();
    if (!forceRefresh) {
        const QVariantMap cached = cachedLookup(call);
        if (!cached.isEmpty()) {
            finishLookup(cached, true, tr("Risultato da cache locale"));
            return;
        }
    }
    QVariantMap local = localLookup(call);
    if (!local.isEmpty()) {
        cacheResult(local);
        finishLookup(local, false, tr("Risultato da database locali"));
        return;
    }
    if (!m_offlineMode) {
        setPending(true);
        setStatus(tr("Nessun record locale: provo i provider remoti..."));
        if (!m_clubLogApiKey.isEmpty()) {
            lookupRemoteClubLog(call);
            return;
        }
    } else {
        setStatus(tr("Offline: nessun record remoto richiesto"));
    }
    QVariantMap fallback;
    fallback.insert(QStringLiteral("call"), call);
    if (m_dxccLookup && m_dxccLookup->isLoaded()) {
        const DxccEntity entity = m_dxccLookup->lookup(call);
        if (entity.isValid()) {
            fallback.insert(QStringLiteral("country"), entity.name);
            fallback.insert(QStringLiteral("dxcc"), entity.name);
            fallback.insert(QStringLiteral("continent"), entity.continent);
            fallback.insert(QStringLiteral("cqZone"), entity.cqZone);
            fallback.insert(QStringLiteral("ituZone"), entity.ituZone);
            fallback.insert(QStringLiteral("provider"), QStringLiteral("dxcc"));
        }
    }
    finishLookup(fallback, false, fallback.size() > 1
                 ? tr("Fallback DXCC: nessun profilo provider disponibile")
                 : tr("Nessun provider ha trovato il callsign"));
}

void CallsignIntelligenceService::lookupRemoteClubLog(const QString& callsign)
{
    if (m_offlineMode) {
        return;
    }
    QUrl url(QStringLiteral("https://clublog.org/watch.php"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("call"), callsign);
    query.addQueryItem(QStringLiteral("api"), m_clubLogApiKey);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium/4.0 Callsign Intelligence"));
    request.setTransferTimeout(15000);
    m_activeReply = m_network->get(request);
    connect(m_activeReply, &QNetworkReply::finished, this, [this, reply = m_activeReply, callsign]() {
        handleRemoteLookupFinished(reply, callsign);
    });
}

void CallsignIntelligenceService::handleRemoteLookupFinished(QNetworkReply* reply, const QString& callsign)
{
    if (!reply) return;
    const QByteArray payload = reply->readAll();
    const QNetworkReply::NetworkError error = reply->error();
    const QString errorText = reply->errorString();
    reply->deleteLater();
    m_activeReply = nullptr;
    if (m_offlineMode) {
        return;
    }
    QVariantMap local = localLookup(callsign);
    if (error == QNetworkReply::NoError) {
        const QJsonDocument doc = QJsonDocument::fromJson(payload);
        if (doc.isObject()) {
            const QJsonObject object = doc.object();
            QVariantMap remote;
            remote.insert(QStringLiteral("call"), callsign);
            remote.insert(QStringLiteral("provider"), QStringLiteral("clublog"));
            remote.insert(QStringLiteral("grid"), object.value(QStringLiteral("qra")).toString());
            remote.insert(QStringLiteral("oqrs"), object.value(QStringLiteral("has_oqrs")).toBool());
            remote.insert(QStringLiteral("clubLogUser"), object.value(QStringLiteral("clublog_user")).toBool());
            remote.insert(QStringLiteral("isExpedition"), object.value(QStringLiteral("is_expedition")).toBool());
            const QJsonObject info = object.value(QStringLiteral("clublog_info")).toObject();
            remote.insert(QStringLiteral("lastClubLogUpload"), info.value(QStringLiteral("last_clublog_upload")).toString());
            remote.insert(QStringLiteral("lastLotwConfirmation"), info.value(QStringLiteral("last_lotw_confirmation")).toString());
            remote = mergeRecord(remote, local);
            cacheResult(remote);
            finishLookup(remote, false, tr("Risultato da Club Log con fallback locale"));
            return;
        }
    }
    if (!local.isEmpty()) {
        cacheResult(local);
        finishLookup(local, false, tr("Club Log non disponibile: usato fallback locale (%1)").arg(errorText));
    } else {
        finishLookup(QVariantMap{{QStringLiteral("call"), callsign}}, false,
                     tr("Provider remoti non disponibile: %1").arg(errorText));
    }
}

bool CallsignIntelligenceService::upsertRecord(const QString& provider, const QVariantMap& record)
{
    if (!m_database || !m_database->isOpen()) return false;
    const QString call = normalizeCall(record.value(QStringLiteral("call")).toString());
    if (call.isEmpty()) return false;
    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO callsign_records(provider,callsign,grid,name,qth,country,dxcc,continent,state,county,last_upload,lotw,eqsl,oqrs,confirmed,metadata_json,updated_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    query.addBindValue(provider);
    query.addBindValue(call);
    query.addBindValue(record.value(QStringLiteral("grid")).toString().trimmed().toUpper());
    query.addBindValue(record.value(QStringLiteral("name")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("qth")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("country")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("dxcc")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("continent")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("state")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("county")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("lastUpload")).toString().trimmed());
    query.addBindValue(record.value(QStringLiteral("lotw")).toBool() ? 1 : 0);
    query.addBindValue(record.value(QStringLiteral("eqsl")).toBool() ? 1 : 0);
    query.addBindValue(record.value(QStringLiteral("oqrs")).toBool() ? 1 : 0);
    query.addBindValue(record.value(QStringLiteral("confirmed")).toBool() ? 1 : 0);
    query.addBindValue(jsonString(record));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    return query.exec();
}

bool CallsignIntelligenceService::importDelimited(const QString& provider, const QByteArray& data)
{
    QString text = QString::fromUtf8(data);
    QTextStream stream(&text, QIODevice::ReadOnly);
    stream.setEncoding(QStringConverter::Utf8);
    const QString first = stream.readLine();
    if (first.isNull()) return false;
    const QChar delimiter = delimiterFor(first);
    QStringList headers = splitLine(first, delimiter);
    bool hasHeader = false;
    for (const QString& header : headers) {
        const QString normalized = normHeader(header);
        if (normalized == QStringLiteral("call") || normalized == QStringLiteral("callsign")
            || normalized == QStringLiteral("gridsquare") || normalized == QStringLiteral("licenseename")) {
            hasHeader = true;
            break;
        }
    }
    int imported = 0;
    QSqlDatabase db = *m_database;
    db.transaction();
    auto importRow = [this, &imported, &provider](const QStringList& row, const QStringList& effectiveHeaders) {
        QString call = fieldFrom(effectiveHeaders, row, {QStringLiteral("call"), QStringLiteral("callsign"), QStringLiteral("matchedcallsign"), QStringLiteral("username")});
        if (call.isEmpty() && !row.isEmpty()) call = row.first();
        QVariantMap record;
        record.insert(QStringLiteral("call"), call);
        record.insert(QStringLiteral("grid"), fieldFrom(effectiveHeaders, row, {QStringLiteral("grid"), QStringLiteral("gridsquare"), QStringLiteral("locator"), QStringLiteral("qra")}));
        record.insert(QStringLiteral("name"), fieldFrom(effectiveHeaders, row, {QStringLiteral("name"), QStringLiteral("fullname"), QStringLiteral("licensename"), QStringLiteral("operator")}));
        record.insert(QStringLiteral("qth"), fieldFrom(effectiveHeaders, row, {QStringLiteral("qth"), QStringLiteral("city"), QStringLiteral("address"), QStringLiteral("location")}));
        record.insert(QStringLiteral("state"), fieldFrom(effectiveHeaders, row, {QStringLiteral("state"), QStringLiteral("st"), QStringLiteral("usstate")}));
        record.insert(QStringLiteral("county"), fieldFrom(effectiveHeaders, row, {QStringLiteral("county"), QStringLiteral("countrycounty")}));
        if (provider == QStringLiteral("lotw")) {
            record.insert(QStringLiteral("lotw"), true);
            if (row.size() > 1) record.insert(QStringLiteral("lastUpload"), row.at(1));
        } else if (provider == QStringLiteral("eqsl")) {
            record.insert(QStringLiteral("eqsl"), true);
        }
        if (upsertRecord(provider, record)) ++imported;
    };
    if (hasHeader) {
        while (!stream.atEnd()) {
            const QStringList row = splitLine(stream.readLine(), delimiter);
            if (!row.isEmpty()) importRow(row, headers);
        }
    } else {
        // LoTW AG activity and eQSL AGMemberListDated are intentionally
        // headerless: first column is the callsign, second is activity date.
        headers = {QStringLiteral("CALLSIGN"), QStringLiteral("LAST_UPLOAD")};
        importRow(splitLine(first, delimiter), headers);
        while (!stream.atEnd()) importRow(splitLine(stream.readLine(), delimiter), headers);
    }
    const bool committed = db.commit();
    if (!committed) db.rollback();
    if (committed) refreshDatabaseState(provider, QDateTime::currentMSecsSinceEpoch(), imported, tr("Updated"));
    return committed && imported > 0;
}

bool CallsignIntelligenceService::importAdif(const QString& provider, const QByteArray& data)
{
    const QString text = QString::fromUtf8(data);
    const QRegularExpression recordExpression(QStringLiteral("(?is)(.*?)(?:<EOR>|$)"));
    auto iterator = recordExpression.globalMatch(text);
    int imported = 0;
    QSqlDatabase db = *m_database;
    db.transaction();
    while (iterator.hasNext()) {
        const QString recordText = iterator.next().captured(1);
        QVariantMap record;
        const QRegularExpression tag(QStringLiteral("(?i)<([A-Z0-9_]+)(?::\\d+)?>([^<]*)"));
        auto tags = tag.globalMatch(recordText);
        while (tags.hasNext()) {
            const auto match = tags.next();
            const QString key = match.captured(1).toUpper();
            const QString value = match.captured(2).trimmed();
            if (key == QStringLiteral("CALL")) record.insert(QStringLiteral("call"), value);
            else if (key == QStringLiteral("GRIDSQUARE")) record.insert(QStringLiteral("grid"), value);
            else if (key == QStringLiteral("NAME")) record.insert(QStringLiteral("name"), value);
            else if (key == QStringLiteral("QTH")) record.insert(QStringLiteral("qth"), value);
            else if (key == QStringLiteral("STATE")) record.insert(QStringLiteral("state"), value);
            else if (key == QStringLiteral("COUNTY")) record.insert(QStringLiteral("county"), value);
            else if (key == QStringLiteral("LOTW_QSL_RCVD")) record.insert(QStringLiteral("lotw"), truthy(value));
            else if (key == QStringLiteral("EQSL_QSL_RCVD")) record.insert(QStringLiteral("eqsl"), truthy(value));
        }
        if (provider == QStringLiteral("lotw")) record.insert(QStringLiteral("lotw"), true);
        if (provider == QStringLiteral("eqsl")) record.insert(QStringLiteral("eqsl"), true);
        if (upsertRecord(provider, record)) ++imported;
    }
    const bool committed = db.commit();
    if (committed) refreshDatabaseState(provider, QDateTime::currentMSecsSinceEpoch(), imported, tr("Updated"));
    return committed && imported > 0;
}

QByteArray CallsignIntelligenceService::extractFccEnDat(const QByteArray& data) const
{
    if (!data.startsWith("PK")) return data;
    // Feed unzip a real temporary archive.  Passing the ZIP bytes to stdin
    // with "unzip -p -" is not portable across the BSD/GNU unzip variants
    // shipped on the supported platforms.
    QTemporaryFile archive;
    if (!archive.open()) return {};
    if (archive.write(data) != data.size() || !archive.flush()) return {};
    QProcess process;
    process.start(QStringLiteral("unzip"), {QStringLiteral("-p"), archive.fileName(), QStringLiteral("EN.dat")});
    if (!process.waitForStarted(3000)) return {};
    if (!process.waitForFinished(30000) || process.exitCode() != 0) return {};
    return process.readAllStandardOutput();
}

bool CallsignIntelligenceService::importFcc(const QByteArray& data)
{
    const QByteArray extracted = extractFccEnDat(data);
    if (extracted.isEmpty()) return false;
    QString text = QString::fromUtf8(extracted);
    QTextStream stream(&text, QIODevice::ReadOnly);
    stream.setEncoding(QStringConverter::Utf8);
    QSqlDatabase db = *m_database;
    db.transaction();
    int imported = 0;
    const QRegularExpression callPattern(QStringLiteral("^[A-Z0-9]{1,3}[0-9][A-Z0-9]{1,5}(/[A-Z0-9]+)?$"));
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        const QStringList fields = line.split(QLatin1Char('|'));
        if (fields.isEmpty() || fields.first().trimmed().toUpper() != QStringLiteral("EN")) continue;
        QString call;
        for (const QString& field : fields) {
            const QString candidate = field.trimmed().toUpper();
            if (candidate.size() >= 4 && callPattern.match(candidate).hasMatch()) {
                call = candidate;
                break;
            }
        }
        if (call.isEmpty()) continue;
        QVariantMap record;
        record.insert(QStringLiteral("call"), call);
        // EN.dat is the FCC entity file.  The exact field positions have
        // changed over ULS revisions, therefore use stable semantic hints and
        // retain the raw row for diagnostics rather than hard-coding columns.
        record.insert(QStringLiteral("name"), fields.value(7).trimmed());
        record.insert(QStringLiteral("qth"), fields.value(10).trimmed());
        record.insert(QStringLiteral("state"), fields.value(11).trimmed());
        record.insert(QStringLiteral("metadata"), line);
        if (upsertRecord(QStringLiteral("fcc_uls"), record)) ++imported;
    }
    const bool committed = db.commit();
    if (committed) refreshDatabaseState(QStringLiteral("fcc_uls"), QDateTime::currentMSecsSinceEpoch(), imported, tr("Updated"));
    return committed && imported > 0;
}

bool CallsignIntelligenceService::importClubLogOqrs(const QByteArray& data)
{
    const QJsonDocument document = QJsonDocument::fromJson(data);
    if (!document.isArray()) return false;
    QSqlDatabase db = *m_database;
    db.transaction();
    int imported = 0;
    for (const QJsonValue& value : document.array()) {
        const QJsonArray row = value.toArray();
        if (row.size() < 4) continue;
        QVariantMap record;
        record.insert(QStringLiteral("call"), row.at(0).toString());
        record.insert(QStringLiteral("lastUpload"), row.at(1).toString());
        record.insert(QStringLiteral("oqrs"), true);
        record.insert(QStringLiteral("confirmed"), true);
        record.insert(QStringLiteral("metadata"), QStringLiteral("band=%1 mode=%2").arg(row.at(2).toString(), row.at(3).toString()));
        if (upsertRecord(QStringLiteral("clublog_oqrs"), record)) ++imported;
    }
    const bool committed = db.commit();
    if (committed) refreshDatabaseState(QStringLiteral("clublog_oqrs"), QDateTime::currentMSecsSinceEpoch(), imported, tr("Updated"));
    return committed && imported > 0;
}

bool CallsignIntelligenceService::importBytes(const QString& provider, const QByteArray& data, const QString& sourcePath)
{
    Q_UNUSED(sourcePath)
    if (provider == QStringLiteral("fcc_uls")) return importFcc(data);
    if (provider == QStringLiteral("clublog_oqrs") && data.trimmed().startsWith('[')) return importClubLogOqrs(data);
    const QString text = QString::fromUtf8(data);
    if (text.contains(QStringLiteral("<EOR>"), Qt::CaseInsensitive) || text.contains(QStringLiteral("<CALL:"), Qt::CaseInsensitive)) return importAdif(provider, data);
    return importDelimited(provider, data);
}

bool CallsignIntelligenceService::importDatabase(const QString& provider, const QString& path)
{
    const QString cleanProvider = provider.trimmed().toLower();
    if (!m_specs.contains(cleanProvider) || cleanProvider == QStringLiteral("dxcc")) return false;
    QString localPath = path;
    if (localPath.startsWith(QStringLiteral("file://"))) localPath = QUrl(localPath).toLocalFile();
    if (cleanProvider == QStringLiteral("lotw_confirmed")
        || cleanProvider == QStringLiteral("qrz_confirmed")) {
        if (m_databaseUpdatePending || m_databaseImportWatcher) {
            setStatus(tr("Aggiornamento già in corso"));
            return false;
        }
        setDatabaseUpdatePending(true);
        m_updateProvider = cleanProvider;
        setStatus(tr("Lettura ADI %1 in background...").arg(providerLabel(cleanProvider)));
        startConfirmedAdifFileImport(cleanProvider, QFileInfo(localPath).absoluteFilePath());
        return true;
    }
    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        refreshDatabaseState(cleanProvider, 0, 0, tr("Error"), file.errorString());
        return false;
    }
    const bool ok = importBytes(cleanProvider, file.readAll(), localPath);
    if (ok) {
        QSqlQuery query(*m_database);
        query.prepare(QStringLiteral("UPDATE callsign_provider_state SET local_path=?,status=?,error='' WHERE provider=?"));
        query.addBindValue(QFileInfo(localPath).absoluteFilePath());
        query.addBindValue(tr("Updated"));
        query.addBindValue(cleanProvider);
        query.exec();
        emit databasesChanged();
        if (!m_currentCall.isEmpty()) lookup(m_currentCall, true);
    }
    return ok;
}

void CallsignIntelligenceService::refreshDatabase(const QString& provider)
{
    const QString cleanProvider = provider.trimmed().toLower();
    if (!m_specs.contains(cleanProvider) || cleanProvider == QStringLiteral("dxcc")) return;
    if (m_offlineMode) {
        setStatus(tr("Offline: aggiornamenti remoti callsign disabilitati"));
        return;
    }
    if (m_activeReply || m_databaseUpdatePending) {
        setStatus(tr("Aggiornamento già in corso"));
        return;
    }
    if (cleanProvider == QStringLiteral("lotw_confirmed")) {
        const QString username = lotwUsername().trimmed();
        if (username.isEmpty() || m_lotwPassword.isEmpty()) {
            refreshDatabaseState(cleanProvider, 0, 0, tr("Error"),
                                 tr("Username e password LoTW richiesti; la password è nella sezione Reporting → LoTW"));
            setStatus(tr("LoTW conferme: username e password richiesti"));
            emit databasesChanged();
            return;
        }

        QUrl url(QStringLiteral("https://lotw.arrl.org/lotwuser/lotwreport.adi"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("login"), username);
        query.addQueryItem(QStringLiteral("password"), m_lotwPassword);
        query.addQueryItem(QStringLiteral("qso_query"), QStringLiteral("1"));
        query.addQueryItem(QStringLiteral("qso_qsl"), QStringLiteral("yes"));
        query.addQueryItem(QStringLiteral("qso_qsldetail"), QStringLiteral("yes"));
        query.addQueryItem(QStringLiteral("qso_withown"), QStringLiteral("yes"));
        if (!m_lotwLastQsl.isEmpty()) {
            query.addQueryItem(QStringLiteral("qso_qslsince"), m_lotwLastQsl);
        }
        url.setQuery(query);
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium/4.0 LoTW Confirmations"));
        request.setTransferTimeout(120000);
        setDatabaseUpdatePending(true);
        m_updateProvider = cleanProvider;
        setStatus(m_lotwLastQsl.isEmpty()
                      ? tr("LoTW: download iniziale delle conferme in corso...")
                      : tr("LoTW: cerco nuove conferme dal %1...").arg(m_lotwLastQsl));
        m_activeReply = m_network->get(request);
        connect(m_activeReply, &QNetworkReply::finished, this, [this, reply = m_activeReply, cleanProvider]() {
            handleDatabaseReply(reply, cleanProvider);
        });
        return;
    }
    if (cleanProvider == QStringLiteral("qrz_confirmed")) {
        if (m_qrzApiKey.trimmed().isEmpty()) {
            refreshDatabaseState(cleanProvider, 0, 0, tr("Error"), tr("Chiave API QRZ mancante: configurarla nella sezione QRZ Logbook"));
            setStatus(tr("QRZ.com: chiave API mancante"));
            emit databasesChanged();
            return;
        }
        m_qrzAfterLogId = QStringLiteral("0");
        m_qrzPageCount = 0;
        m_qrzAdifPayload.clear();
        setDatabaseUpdatePending(true);
        m_updateProvider = cleanProvider;
        setStatus(tr("QRZ.com: pagina 1 delle conferme in download..."));
        requestQrzConfirmedPage();
        return;
    }
    if (cleanProvider == QStringLiteral("eqsl_inbox")) {
        const QString username = eqslUsername().trimmed();
        if (username.isEmpty() || m_eqslPassword.isEmpty()) {
            refreshDatabaseState(cleanProvider, 0, 0, tr("Error"), tr("Username e password eQSL richiesti"));
            setStatus(tr("eQSL InBox: username e password eQSL richiesti"));
            emit databasesChanged();
            return;
        }

        QUrl url(QStringLiteral("https://www.eqsl.cc/qslcard/DownloadInbox.cfm"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("Username"), username);
        query.addQueryItem(QStringLiteral("Password"), m_eqslPassword);
        url.setQuery(query);
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium/4.0 eQSL InBox"));
        request.setTransferTimeout(120000);
        setDatabaseUpdatePending(true);
        m_updateProvider = cleanProvider;
        m_activeReply = m_network->get(request);
        connect(m_activeReply, &QNetworkReply::finished, this, [this, reply = m_activeReply, cleanProvider]() {
            handleDatabaseReply(reply, cleanProvider);
        });
        setStatus(tr("Download eQSL InBox in corso..."));
        return;
    }
    if (cleanProvider == QStringLiteral("clublog_oqrs")) {
        if (m_clubLogApiKey.isEmpty() || m_clubLogEmail.isEmpty() || m_clubLogApplicationPassword.isEmpty() || m_operatorCallsign.isEmpty()) {
            setStatus(tr("Club Log OQRS: API key, email, application password e callsign operatore richiesti"));
            return;
        }
        QUrl url(QStringLiteral("https://clublog.org/getoqrsmatches.php"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("api"), m_clubLogApiKey);
        query.addQueryItem(QStringLiteral("email"), m_clubLogEmail);
        query.addQueryItem(QStringLiteral("password"), m_clubLogApplicationPassword);
        query.addQueryItem(QStringLiteral("callsign"), m_operatorCallsign);
        url.setQuery(query);
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium/4.0 Callsign Intelligence"));
        request.setTransferTimeout(30000);
        setDatabaseUpdatePending(true);
        m_updateProvider = cleanProvider;
        m_activeReply = m_network->get(request);
        connect(m_activeReply, &QNetworkReply::finished, this, [this, reply = m_activeReply, cleanProvider]() {
            handleDatabaseReply(reply, cleanProvider);
        });
        setStatus(tr("Aggiornamento Club Log OQRS in corso..."));
        return;
    }
    const QUrl url(providerUrl(cleanProvider));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium/4.0 Callsign Intelligence"));
    request.setTransferTimeout(60000);
    setDatabaseUpdatePending(true);
    m_updateProvider = cleanProvider;
    m_activeReply = m_network->get(request);
    connect(m_activeReply, &QNetworkReply::finished, this, [this, reply = m_activeReply, cleanProvider]() {
        handleDatabaseReply(reply, cleanProvider);
    });
    setStatus(tr("Aggiornamento %1 in corso...").arg(providerLabel(cleanProvider)));
}

void CallsignIntelligenceService::handleDatabaseReply(QNetworkReply* reply,
                                                      const QString& provider,
                                                      bool eqslInboxAdif)
{
    if (!reply) return;
    const QByteArray payload = reply->readAll();
    const QUrl responseUrl = reply->url();
    const auto error = reply->error();
    const QString errorText = reply->errorString();
    reply->deleteLater();
    m_activeReply = nullptr;
    if (m_offlineMode) {
        setDatabaseUpdatePending(false);
        return;
    }
    if (error != QNetworkReply::NoError) {
        refreshDatabaseState(provider, 0, 0, tr("Error"), errorText);
        setStatus(tr("Aggiornamento %1 fallito: %2").arg(providerLabel(provider), errorText));
        m_updateProvider.clear();
        setDatabaseUpdatePending(false);
        emit databasesChanged();
        return;
    }
    if (provider == QStringLiteral("lotw_confirmed")) {
        const decodium::lotw::ReportResponse response = decodium::lotw::parseReportResponse(payload);
        if (response.kind != decodium::lotw::ReportResponseKind::Adif) {
            const QString detail = response.error.isEmpty()
                ? tr("la risposta LoTW non contiene un ADIF valido")
                : response.error;
            refreshDatabaseState(provider, 0, 0, tr("Error"), detail);
            m_updateProvider.clear();
            setDatabaseUpdatePending(false);
            setStatus(tr("LoTW fallito: %1").arg(detail));
            emit databasesChanged();
            return;
        }
        m_pendingLotwLastQsl = adifHeaderValue(payload, QStringLiteral("APP_LoTW_LASTQSL"));
        startConfirmedAdifSave(provider, payload);
        return;
    }
    if (provider == QStringLiteral("qrz_confirmed")) {
        const QString result = qrzFormValue(payload, QStringLiteral("RESULT")).toUpper();
        if (result != QStringLiteral("OK")) {
            QString reason = qrzFormValue(payload, QStringLiteral("REASON"));
            if (reason.isEmpty()) reason = qrzFormValue(payload, QStringLiteral("DATA"));
            const QString detail = reason.isEmpty() ? tr("risposta API non valida") : reason;
            refreshDatabaseState(provider, 0, 0, tr("Error"), detail);
            m_updateProvider.clear();
            setDatabaseUpdatePending(false);
            setStatus(tr("QRZ.com fallito: %1").arg(detail));
            emit databasesChanged();
            return;
        }

        QString pageAdif = qrzFormValue(payload, QStringLiteral("ADIF"));
        const QRegularExpression eohExpression(QStringLiteral("(?i)<EOH>"));
        const QRegularExpressionMatch eohMatch = eohExpression.match(pageAdif);
        if (eohMatch.hasMatch()) {
            // FETCH pages may carry their own ADIF header.  Keep one clean
            // stream of records so a later page cannot become part of the
            // previous record when the pages are merged.
            pageAdif = pageAdif.mid(eohMatch.capturedEnd());
        }
        if (pageAdif.contains(QRegularExpression(QStringLiteral("(?i)<CALL:")))) {
            m_qrzAdifPayload.append(pageAdif.toUtf8());
            const QByteArray lowerAdif = m_qrzAdifPayload.toLower();
            if (!lowerAdif.trimmed().endsWith("<eor>")) {
                m_qrzAdifPayload.append("\n<EOR>\n");
            }
        }
        const int count = qMax(0, qrzFormValue(payload, QStringLiteral("COUNT")).toInt());
        qint64 highestLogId = -1;
        const QStringList logIds = qrzFormValue(payload, QStringLiteral("LOGIDS")).split(',', Qt::SkipEmptyParts);
        for (const QString& logId : logIds) {
            highestLogId = qMax(highestLogId, logId.trimmed().toLongLong());
        }
        highestLogId = qMax(highestLogId, highestQrzLogId(pageAdif));
        ++m_qrzPageCount;

        // QRZ recommends requesting 250 records at a time and continuing
        // after the highest returned log id.  The guard also protects the UI
        // from a malformed server response that never advances the cursor.
        if (count >= 250 && highestLogId >= m_qrzAfterLogId.toLongLong() && m_qrzPageCount < 10000) {
            m_qrzAfterLogId = QString::number(highestLogId + 1);
            setStatus(tr("QRZ.com: pagina %1 ricevuta (%2 conferme), continuo...")
                          .arg(m_qrzPageCount)
                          .arg(count));
            requestQrzConfirmedPage();
            return;
        }

        int downloadedRecords = 0;
        int eorCursor = 0;
        const QByteArray lowerPayload = m_qrzAdifPayload.toLower();
        while ((eorCursor = lowerPayload.indexOf("<eor", eorCursor)) >= 0) {
            ++downloadedRecords;
            eorCursor += 4;
        }
        setStatus(tr("QRZ.com: %1 conferme scaricate; preparazione import...")
                      .arg(downloadedRecords));
        startConfirmedAdifSave(provider, m_qrzAdifPayload);
        return;
    }
    if (provider == QStringLiteral("eqsl_inbox")) {
        if (!eqslInboxAdif) {
            const decodium::eqsl::InboxPageResult page = decodium::eqsl::parseInboxPage(payload, responseUrl);
            if (page.kind == decodium::eqsl::InboxPageKind::DownloadReady) {
                QNetworkRequest request(page.adifUrl);
                request.setHeader(QNetworkRequest::UserAgentHeader,
                                  QStringLiteral("Decodium/4.0 eQSL InBox"));
                request.setTransferTimeout(120000);
                m_activeReply = m_network->get(request);
                connect(m_activeReply, &QNetworkReply::finished, this,
                        [this, reply = m_activeReply, provider]() {
                    handleDatabaseReply(reply, provider, true);
                });
                setStatus(tr("eQSL InBox: download del file ADI in corso..."));
                return;
            }
            if (page.kind == decodium::eqsl::InboxPageKind::NoRecords) {
                // eQSL can legitimately answer without a generated download
                // when the Inbox is empty.  Store an empty ADIF rather than
                // presenting this as a credentials failure.
                startConfirmedAdifSave(provider, {});
                return;
            }
            if (page.kind != decodium::eqsl::InboxPageKind::DirectAdif) {
                const QString detail = page.error.isEmpty()
                    ? tr("risposta eQSL non valida")
                    : page.error;
                refreshDatabaseState(provider, 0, 0, tr("Error"), detail);
                m_updateProvider.clear();
                setDatabaseUpdatePending(false);
                setStatus(tr("eQSL InBox fallito: %1").arg(detail));
                emit databasesChanged();
                return;
            }
        }
        startConfirmedAdifSave(provider, payload);
        return;
    }
    startDatabaseImport(provider, payload);
}

void CallsignIntelligenceService::requestQrzConfirmedPage()
{
    if (m_offlineMode || m_qrzApiKey.trimmed().isEmpty() || m_updateProvider != QStringLiteral("qrz_confirmed")) {
        return;
    }

    QNetworkRequest request(QUrl(QStringLiteral("https://logbook.qrz.com/api")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Decodium/4.0 QRZLogbook"));
    request.setTransferTimeout(120000);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("KEY"), m_qrzApiKey);
    query.addQueryItem(QStringLiteral("ACTION"), QStringLiteral("FETCH"));
    query.addQueryItem(QStringLiteral("OPTION"), QStringLiteral("TYPE:ADIF,STATUS:CONFIRMED,MAX:250,AFTERLOGID:%1").arg(m_qrzAfterLogId));
    m_activeReply = m_network->post(request, query.toString(QUrl::FullyEncoded).toUtf8());
    connect(m_activeReply, &QNetworkReply::finished, this, [this, reply = m_activeReply]() {
        handleDatabaseReply(reply, QStringLiteral("qrz_confirmed"));
    });
}

void CallsignIntelligenceService::startConfirmedAdifSave(const QString& provider, const QByteArray& data)
{
    if (m_databaseImportWatcher) {
        setStatus(tr("Aggiornamento già in corso"));
        return;
    }

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString fileName = provider == QStringLiteral("eqsl_inbox")
        ? QStringLiteral("eqsl-inbox.adi")
        : provider == QStringLiteral("lotw_confirmed")
          ? QStringLiteral("lotw-confirmed.adi")
          : QStringLiteral("qrz-confirmed.adi");
    const QString path = QDir(appData).absoluteFilePath(fileName);
    setStatus(tr("Salvataggio conferme %1 in background...").arg(providerLabel(provider)));

    auto* watcher = new QFutureWatcher<QVariantMap>(this);
    m_databaseImportWatcher = watcher;
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, path, provider]() {
        const QVariantMap result = watcher->result();
        if (m_databaseImportWatcher == watcher)
            m_databaseImportWatcher = nullptr;
        watcher->deleteLater();

        if (!result.value(QStringLiteral("ok")).toBool()) {
            const QString error = result.value(QStringLiteral("error"), tr("File ADI non valido")).toString();
            refreshDatabaseState(provider, 0, 0, tr("Error"), error);
            m_updateProvider.clear();
            setDatabaseUpdatePending(false);
            if (provider == QStringLiteral("lotw_confirmed")) {
                m_pendingLotwLastQsl.clear();
            }
            setStatus(tr("%1 fallito: %2").arg(providerLabel(provider), error));
            emit databasesChanged();
            return;
        }

        QSqlQuery query(*m_database);
        query.prepare(QStringLiteral("UPDATE callsign_provider_state SET local_path=?,status=?,error='' WHERE provider=?"));
        query.addBindValue(path);
        query.addBindValue(tr("Scaricato; sincronizzazione logbook..."));
        query.addBindValue(provider);
        query.exec();
        refreshDatabaseState(provider, QDateTime::currentMSecsSinceEpoch(),
                             result.value(QStringLiteral("rowCount")).toInt(),
                             tr("Scaricato; sincronizzazione logbook..."));
        setStatus(tr("%1 scaricato: sincronizzazione logbook in corso...").arg(providerLabel(provider)));
        emit databasesChanged();
        emit confirmedAdifDownloaded(provider, path);
    });
    watcher->setFuture(QtConcurrent::run([path, data, provider]() {
        QVariantMap result;
        const QByteArray lower = data.left(8192).toLower();
        const QByteArray lowerAll = data.toLower();
        const bool confirmedAdifHeader = (provider == QStringLiteral("lotw_confirmed")
                                          || provider == QStringLiteral("eqsl_inbox"))
            && lowerAll.contains("<eoh>");
        const bool hasRecords = lowerAll.contains("<call:");
        const bool hasRecordEnd = lowerAll.contains("<eor");
        if (!data.trimmed().isEmpty()
            && ((!hasRecords && !confirmedAdifHeader) || (!hasRecordEnd && !confirmedAdifHeader))) {
            result.insert(QStringLiteral("error"),
                          lower.contains("<html") || lower.contains("error")
                              ? QStringLiteral("il provider ha restituito una pagina di errore: verificare le credenziali")
                              : QStringLiteral("la risposta non contiene un ADI valido"));
            return result;
        }

        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            result.insert(QStringLiteral("error"), file.errorString());
            return result;
        }
        QByteArray output = data.trimmed().isEmpty()
            ? QByteArray("Decodium4 confirmed contacts\n<EOH>\n")
            : data;
        QByteArray lowerOutput = output.toLower();
        if (provider == QStringLiteral("lotw_confirmed")
            && lowerOutput.contains("<call:")
            && !lowerOutput.contains("<eor")) {
            output.append("\n<EOR>\n");
            lowerOutput = output.toLower();
        }
        if (file.write(output) != output.size() || !file.commit()) {
            result.insert(QStringLiteral("error"), file.errorString());
            return result;
        }

        int rowCount = 0;
        int cursor = 0;
        while ((cursor = lowerOutput.indexOf("<eor", cursor)) >= 0) {
            ++rowCount;
            cursor += 4;
        }
        result.insert(QStringLiteral("ok"), true);
        result.insert(QStringLiteral("rowCount"), rowCount);
        return result;
    }));
}

void CallsignIntelligenceService::startConfirmedAdifFileImport(const QString& provider, const QString& path)
{
    if (m_databaseImportWatcher) {
        setStatus(tr("Aggiornamento già in corso"));
        return;
    }

    auto* watcher = new QFutureWatcher<QVariantMap>(this);
    m_databaseImportWatcher = watcher;
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, provider]() {
        const QVariantMap result = watcher->result();
        if (m_databaseImportWatcher == watcher)
            m_databaseImportWatcher = nullptr;
        watcher->deleteLater();

        if (!result.value(QStringLiteral("ok")).toBool()) {
            const QString error = result.value(QStringLiteral("error"), tr("impossibile leggere il file ADI")).toString();
            refreshDatabaseState(provider, 0, 0, tr("Error"), error);
            m_updateProvider.clear();
            setDatabaseUpdatePending(false);
            if (provider == QStringLiteral("lotw_confirmed")) {
                m_pendingLotwLastQsl.clear();
            }
            setStatus(tr("Importazione %1 fallita: %2").arg(providerLabel(provider), error));
            emit databasesChanged();
            return;
        }

        // A manually chosen LoTW export is a recovery/import operation. It
        // must not move the incremental online cursor backwards when an older
        // archive is selected. Only a successful online LoTW query advances
        // APP_LoTW_LASTQSL in completeConfirmedAdifImport().
        if (provider == QStringLiteral("lotw_confirmed")) {
            m_pendingLotwLastQsl.clear();
        }
        setStatus(tr("File ADI %1 letto; importazione in background...").arg(providerLabel(provider)));
        startConfirmedAdifSave(provider, result.value(QStringLiteral("payload")).toByteArray());
    });
    watcher->setFuture(QtConcurrent::run([path]() {
        QVariantMap result;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            result.insert(QStringLiteral("error"), file.errorString());
            return result;
        }
        const QByteArray payload = file.readAll();
        if (payload.isEmpty() && file.error() != QFileDevice::NoError) {
            result.insert(QStringLiteral("error"), file.errorString());
            return result;
        }
        result.insert(QStringLiteral("ok"), true);
        result.insert(QStringLiteral("payload"), payload);
        return result;
    }));
}

void CallsignIntelligenceService::startDatabaseImport(const QString& provider, const QByteArray& data)
{
    if (m_databaseImportWatcher) {
        setStatus(tr("Aggiornamento già in corso"));
        return;
    }

    setStatus(tr("Importazione %1 in background...").arg(providerLabel(provider)));
    auto* watcher = new QFutureWatcher<QVariantMap>(this);
    m_databaseImportWatcher = watcher;
    const QString databasePath = m_databasePath;
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, provider]() {
        const QVariantMap result = watcher->result();
        if (m_databaseImportWatcher == watcher)
            m_databaseImportWatcher = nullptr;
        watcher->deleteLater();
        m_updateProvider.clear();
        setDatabaseUpdatePending(false);

        const bool ok = result.value(QStringLiteral("ok")).toBool();
        const int imported = result.value(QStringLiteral("imported")).toInt();
        if (!ok) {
            const QString error = result.value(QStringLiteral("error"), tr("Formato dati non riconosciuto o nessun record")).toString();
            refreshDatabaseState(provider, 0, 0, tr("Error"), error);
            setStatus(tr("Aggiornamento %1 fallito: %2").arg(providerLabel(provider), error));
        } else {
            refreshDatabaseState(provider, QDateTime::currentMSecsSinceEpoch(), imported, tr("Updated"));
            setStatus(tr("%1 aggiornato").arg(providerLabel(provider)));
        }
        emit databasesChanged();
    });
    watcher->setFuture(QtConcurrent::run([provider, databasePath, data]() {
        return importProviderPayloadInBackground(provider, databasePath, data);
    }));
}

void CallsignIntelligenceService::refreshDatabaseState(const QString& provider, qint64 updatedAt, int rowCount, const QString& status, const QString& error)
{
    if (!m_database || !m_database->isOpen()) return;

    // The importer returns the number of rows seen in the last payload.  The
    // UI, however, needs the number of records currently available locally;
    // an update can replace existing callsigns and therefore the two values
    // are not necessarily the same.  On errors keep the last known count.
    int effectiveRowCount = rowCount;
    QSqlQuery countQuery(*m_database);
    if (updatedAt > 0 && provider != QStringLiteral("eqsl_inbox")
        && provider != QStringLiteral("lotw_confirmed")
        && provider != QStringLiteral("qrz_confirmed")) {
        countQuery.prepare(QStringLiteral("SELECT COUNT(*) FROM callsign_records WHERE provider=?"));
        countQuery.addBindValue(provider);
        if (countQuery.exec() && countQuery.next())
            effectiveRowCount = countQuery.value(0).toInt();
    } else if (updatedAt <= 0) {
        // Preserve the last successful count when reporting an error. For
        // confirmed providers a successful sync uses rowCount directly: its
        // source records live in the active logbook, not callsign_records.
        countQuery.prepare(QStringLiteral("SELECT row_count FROM callsign_provider_state WHERE provider=?"));
        countQuery.addBindValue(provider);
        if (countQuery.exec() && countQuery.next())
            effectiveRowCount = countQuery.value(0).toInt();
    }

    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("UPDATE callsign_provider_state SET updated_at=?,row_count=?,status=?,error=? WHERE provider=?"));
    query.addBindValue(updatedAt);
    query.addBindValue(effectiveRowCount);
    query.addBindValue(status);
    query.addBindValue(error);
    query.addBindValue(provider);
    query.exec();
    emit databasesChanged();
}

QVariantMap CallsignIntelligenceService::providerState(const QString& provider) const
{
    QVariantMap state;
    const ProviderSpec spec = m_specs.value(provider);
    state.insert(QStringLiteral("id"), provider);
    state.insert(QStringLiteral("label"), spec.label);
    state.insert(QStringLiteral("url"), spec.url);
    state.insert(QStringLiteral("updateable"), spec.updateable);
    if (provider == QStringLiteral("eqsl_inbox"))
        state.insert(QStringLiteral("managedFile"), true);
    if (!m_database || !m_database->isOpen()) return state;
    QSqlQuery query(*m_database);
    query.prepare(QStringLiteral("SELECT source_url,local_path,row_count,updated_at,status,error FROM callsign_provider_state WHERE provider=?"));
    query.addBindValue(provider);
    if (query.exec() && query.next()) {
        state.insert(QStringLiteral("url"), query.value(0).toString());
        state.insert(QStringLiteral("localPath"), query.value(1).toString());
        state.insert(QStringLiteral("rowCount"), query.value(2).toInt());
        state.insert(QStringLiteral("updatedAt"), query.value(3).toLongLong());
        QString storedStatus = query.value(4).toString();
        if (storedStatus == QStringLiteral("Mai aggiornato"))
            storedStatus = tr("Never updated");
        else if (storedStatus == QStringLiteral("Locale"))
            storedStatus = tr("Local");
        else if (storedStatus == QStringLiteral("Aggiornato"))
            storedStatus = tr("Updated");
        else if (storedStatus == QStringLiteral("Errore"))
            storedStatus = tr("Error");
        state.insert(QStringLiteral("status"), storedStatus);
        state.insert(QStringLiteral("error"), query.value(5).toString());
    }
    if (m_databaseUpdatePending && m_updateProvider == provider) {
        state.insert(QStringLiteral("status"), m_status);
        state.insert(QStringLiteral("error"), QString());
    }

    // cty.dat is not imported into callsign_records. Its real record count
    // is the number of entities parsed by DxccLookup, otherwise the UI would
    // incorrectly report "nessun record" after a successful DXCC download.
    if (provider == QStringLiteral("dxcc") && m_dxccLookup) {
        const int entityCount = m_dxccLookup->entityCount();
        state.insert(QStringLiteral("rowCount"), entityCount);
        state.insert(QStringLiteral("status"), entityCount > 0
                     ? tr("Local")
                     : tr("File not loaded"));
        state.insert(QStringLiteral("error"), QString());
    }
    return state;
}

void CallsignIntelligenceService::completeConfirmedAdifImport(const QString& provider,
                                                               bool ok,
                                                               int imported,
                                                               int updated,
                                                               int sourceCount,
                                                               const QString& error)
{
    if (!m_database || !m_database->isOpen()) {
        m_updateProvider.clear();
        setDatabaseUpdatePending(false);
        return;
    }

    if (!ok) {
        refreshDatabaseState(provider, 0, 0, tr("Error"), error);
        setStatus(tr("Sincronizzazione %1 fallita: %2").arg(providerLabel(provider), error));
    } else {
        const QString status = tr("Updated: %1 new, %2 confirmations updated")
                                   .arg(imported)
                                   .arg(updated);
        if (provider == QStringLiteral("lotw_confirmed") && !m_pendingLotwLastQsl.isEmpty()) {
            // Advance the incremental cursor only after DecodiumBridge has
            // successfully merged the downloaded confirmations in the active
            // logbook.  A failed merge must be safely retried next time.
            m_lotwLastQsl = m_pendingLotwLastQsl;
            saveSetting(QStringLiteral("LotwLastQsl"), m_lotwLastQsl);
            m_pendingLotwLastQsl.clear();
        }
        refreshDatabaseState(provider, QDateTime::currentMSecsSinceEpoch(), sourceCount, status);
        setStatus(tr("%1 synchronized: %2 new, %3 confirmations updated")
                      .arg(providerLabel(provider))
                      .arg(imported)
                      .arg(updated));
    }
    m_updateProvider.clear();
    setDatabaseUpdatePending(false);
    emit databasesChanged();
}

QVariantList CallsignIntelligenceService::databases() const
{
    QVariantList result;
    for (const QString& provider : {QStringLiteral("fcc_uls"), QStringLiteral("lotw"),
                                    QStringLiteral("lotw_confirmed"), QStringLiteral("eqsl"),
                                    QStringLiteral("eqsl_inbox"), QStringLiteral("qrz_confirmed"),
                                    QStringLiteral("clublog_oqrs")}) {
        result.append(providerState(provider));
    }
    return result;
}

QString CallsignIntelligenceService::providerUrl(const QString& provider) const
{
    return m_specs.value(provider).url;
}

QString CallsignIntelligenceService::providerLabel(const QString& provider) const
{
    if (provider == QStringLiteral("fcc_uls")) return tr("FCC ULS");
    if (provider == QStringLiteral("lotw")) return tr("LoTW - User activity");
    if (provider == QStringLiteral("lotw_confirmed")) return tr("LoTW - Confirmations received");
    if (provider == QStringLiteral("eqsl")) return tr("eQSL AG");
    if (provider == QStringLiteral("eqsl_inbox")) return tr("eQSL InBox - Confirmations received");
    if (provider == QStringLiteral("qrz_confirmed")) return tr("QRZ.com - Confirmations received");
    if (provider == QStringLiteral("clublog_oqrs")) return tr("Club Log OQRS");
    if (provider == QStringLiteral("dxcc")) return tr("DXCC cty.dat");
    return m_specs.value(provider).label.isEmpty() ? provider : m_specs.value(provider).label;
}

QString CallsignIntelligenceService::externalUrl(const QString& provider, const QString& callsign) const
{
    const QString call = QUrl::toPercentEncoding(callsign);
    if (provider == QStringLiteral("qrz")) return QStringLiteral("https://www.qrz.com/db/%1").arg(call);
    if (provider == QStringLiteral("fcc_uls")) return QStringLiteral("https://wireless2.fcc.gov/UlsApp/UlsSearch/searchLicense.jsp?callSign=%1").arg(call);
    if (provider == QStringLiteral("eqsl")) return QStringLiteral("https://www.eqsl.cc/Member.cfm?%1").arg(call);
    if (provider == QStringLiteral("clublog")) return QStringLiteral("https://clublog.org/logsearch/%1").arg(call);
    return QStringLiteral("https://www.google.com/search?q=%1+amateur+radio").arg(call);
}

bool CallsignIntelligenceService::openProviderLookup(const QString& provider,
                                                     const QString& callsign)
{
    const QString cleanProvider = provider.trimmed().toLower().isEmpty()
        ? QStringLiteral("qrz") : provider.trimmed().toLower();
    const QString call = normalizeCall(callsign.trimmed().isEmpty()
                                           ? m_currentCall : callsign);
    if (call.isEmpty()) {
        setStatus(tr("Impossibile aprire il lookup esterno: callsign non valido"));
        qWarning().noquote()
            << "[CALLLOOKUP] external lookup rejected: invalid callsign"
            << "provider=" << cleanProvider;
        return false;
    }

    const QString url = externalUrl(cleanProvider, call);
    if (url.isEmpty()) {
        setStatus(tr("Impossibile creare l'URL del provider esterno"));
        qWarning().noquote()
            << "[CALLLOOKUP] external lookup rejected: empty URL"
            << "provider=" << cleanProvider
            << "call=" << call;
        return false;
    }

    // Offline mode controls Decodium's own network requests. An explicit user
    // click is still allowed to hand the URL to the system browser.
    setStatus(tr("Apertura %1 per %2 nel browser di sistema...")
                  .arg(cleanProvider.toUpper(), call));
    qInfo().noquote()
        << "[CALLLOOKUP] external lookup requested"
        << "provider=" << cleanProvider
        << "call=" << call;
    emit externalLookupRequested(url);
    return true;
}

void CallsignIntelligenceService::clearCache(const QString& callsign)
{
    if (!m_database || !m_database->isOpen()) return;
    QSqlQuery query(*m_database);
    if (callsign.trimmed().isEmpty()) {
        query.exec(QStringLiteral("DELETE FROM callsign_cache"));
    } else {
        query.prepare(QStringLiteral("DELETE FROM callsign_cache WHERE callsign=?"));
        query.addBindValue(normalizeCall(callsign));
        query.exec();
    }
    setStatus(tr("Cache callsign svuotata"));
}

QVariantMap CallsignIntelligenceService::lookupForFields(const QString& callsign) const
{
    const QString call = normalizeCall(callsign);
    if (call.isEmpty()) return {};
    QVariantMap value = cachedLookup(call);
    if (value.isEmpty()) value = localLookup(call);
    return value;
}

void CallsignIntelligenceService::notifyQsoStarted(const QString& callsign)
{
    const QString call = normalizeCall(callsign);
    if (call.isEmpty()) return;
    lookup(call);
    if (m_autoOpenOnQsoStart) emit lookupWindowRequested();
}

void CallsignIntelligenceService::notifyQsoLogged(const QString& callsign)
{
    if (m_autoCloseAfterLogging && normalizeCall(callsign) == m_currentCall) {
        emit lookupWindowCloseRequested();
    }
}
