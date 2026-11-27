#include "src/services/CallsignIntelligenceService.h"
#include "src/services/EqslInboxDownload.h"
#include "src/services/LotwReportResponse.h"
#include "src/security/SecureSettings.hpp"

#include <QFile>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QSignalSpy>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QtTest>
#include <cstring>
#include <utility>

class FakeClubLogReply final : public QNetworkReply
{
public:
    FakeClubLogReply(const QNetworkRequest& request, QByteArray payload, int status, QObject* parent)
        : QNetworkReply(parent), m_payload(std::move(payload))
    {
        setRequest(request); setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if (status >= 400) setError(QNetworkReply::ContentAccessDenied, QStringLiteral("HTTP error"));
        if (status == 429) setRawHeader("Retry-After", "300");
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        QTimer::singleShot(0, this, [this] { emit readyRead(); emit finished(); });
    }
    void abort() override {}
    qint64 bytesAvailable() const override { return m_payload.size() - m_offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char* data, qint64 maximum) override
    {
        const qint64 count = qMin(maximum, qint64(m_payload.size()) - m_offset);
        if (count <= 0) return -1;
        std::memcpy(data, m_payload.constData() + m_offset, size_t(count));
        m_offset += count;
        return count;
    }
private:
    QByteArray m_payload;
    qint64 m_offset {0};
};

class FakeClubLogNetwork final : public QNetworkAccessManager
{
public:
    QList<QNetworkRequest> requests;
    QList<QByteArray> payloads;
    QList<int> statuses;
    QList<QByteArray> bodies;
protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice* outgoing) override
    {
        requests.append(request);
        bodies.append(outgoing ? outgoing->readAll() : QByteArray());
        const QByteArray payload = payloads.isEmpty() ? QByteArray("{}") : payloads.takeFirst();
        const int status = statuses.isEmpty() ? 200 : statuses.takeFirst();
        return new FakeClubLogReply(request, payload, status, this);
    }
};

class CallsignIntelligenceTest final : public QObject
{
    Q_OBJECT

private slots:
    void decoDxLogBatchLiveOptIn()
    {
        const QString fixture = qEnvironmentVariable("DECODIUM_CLUBLOG_LIVE_FIXTURE");
        const QString executable = qEnvironmentVariable("DECODXLOG_CLUBLOG_LIVE_TEST_EXECUTABLE");
        const QString audit = qEnvironmentVariable("DECODXLOG_CLUBLOG_LIVE_AUDIT");
        if (fixture.isEmpty() || executable.isEmpty() || audit.isEmpty()) QSKIP("DecoDXLog live test not requested");
        const auto key = secure_settings::default_backend().lookup(QStringLiteral("DecoDXLog"), QStringLiteral("clublog"));
        if (!key.found || !key.value.startsWith(QLatin1String("cl_"))) QSKIP("Club Log token unavailable in system keystore");
        QProcess child;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("DECODXLOG_CLUBLOG_LIVE_FIXTURE"), fixture);
        env.insert(QStringLiteral("DECODXLOG_CLUBLOG_LIVE_AUDIT"), audit);
        child.setProcessEnvironment(env);
        child.start(executable, {QStringLiteral("clubLogBatchLiveOptIn"), QStringLiteral("-silent")});
        QVERIFY2(child.waitForStarted(10000), qPrintable(child.errorString()));
        child.write(key.value.toUtf8() + '\n'); child.closeWriteChannel();
        QVERIFY2(child.waitForFinished(60000), qPrintable(child.errorString()));
        QString output = QString::fromUtf8(child.readAllStandardOutput() + child.readAllStandardError());
        output.replace(key.value, QStringLiteral("[redacted]"), Qt::CaseInsensitive);
        QVERIFY2(child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0, qPrintable(output));
        QVERIFY2(!output.contains(QLatin1String("SKIP")), qPrintable(output));
    }

    void clubLogDirectLiveOptIn()
    {
        const QString path = qEnvironmentVariable("DECODIUM_CLUBLOG_LIVE_FIXTURE");
        if (path.isEmpty()) QSKIP("Live Club Log test not requested");
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject fixture = QJsonDocument::fromJson(file.readAll()).object();
        const QString station = fixture.value("station").toString();
        const QJsonArray records = fixture.value("records").toArray();
        QVERIFY(!station.isEmpty()); QCOMPARE(records.size(), 2);
        const auto key = secure_settings::default_backend().lookup(QStringLiteral("DecoDXLog"), QStringLiteral("clublog"));
        if (!key.found || !key.value.startsWith(QLatin1String("cl_"))) QSKIP("Club Log token is unavailable in the system keystore");
        CallsignIntelligenceService service;
        service.m_clubLogApiKey = key.value;
        service.m_clubLogUploadEnabled = true;
        service.m_operatorCallsign = station;
        QSqlQuery existing(*service.m_database);
        QVERIFY(existing.exec("SELECT COUNT(*) FROM clublog_outbox WHERE state IN ('pending','uncertain')"));
        QVERIFY(existing.next());
        if (existing.value(0).toInt() > 0) QSKIP("The isolated test outbox contains unresolved contacts");
        QStringList digests;
        for (const auto& value : records) {
            QByteArray adif = value.toString().toUtf8().trimmed();
            if (!adif.toUpper().endsWith("<EOR>")) adif += " <EOR>";
            const QString digest = QString::fromLatin1(QCryptographicHash::hash(adif, QCryptographicHash::Sha256).toHex());
            digests.append(digest);
            existing.prepare("SELECT COUNT(*) FROM clublog_outbox WHERE station=? AND digest=?");
            existing.addBindValue(station); existing.addBindValue(digest);
            QVERIFY(existing.exec()); QVERIFY(existing.next());
            if (existing.value(0).toInt() > 0) QSKIP("Live fixture was already submitted or is awaiting verification");
        }
        for (const auto& value : records) service.enqueueClubLogQso(value.toString().toUtf8(), station);
        service.m_clubLogUploadTimer.stop();
        service.processClubLogOutbox();
        QTRY_VERIFY_WITH_TIMEOUT(service.m_clubLogUploadReply == nullptr, 45000);
        QSqlQuery result(*service.m_database);
        result.prepare("SELECT COUNT(*) FROM clublog_outbox WHERE station=? AND state='sent' AND remote_id IS NOT NULL AND digest IN (?,?)");
        result.addBindValue(station); result.addBindValue(digests.at(0)); result.addBindValue(digests.at(1));
        QVERIFY(result.exec()); QVERIFY(result.next());
        QCOMPARE(result.value(0).toInt(), 2);
        service.m_clubLogUploadTimer.stop();
    }
    void clubLogDirectBatchVerifiesEachQso()
    {
        CallsignIntelligenceService service;
        QSqlQuery cleanup(*service.m_database); QVERIFY(cleanup.exec("DELETE FROM clublog_outbox"));
        auto* network = new FakeClubLogNetwork; network->setParent(&service); service.m_network = network;
        service.m_clubLogApiKey = "cl_direct_batch_test"; service.m_clubLogUploadEnabled = true; service.m_operatorCallsign = "K1TEST";
        const QByteArray one = "<CALL:5>K1AAA<QSO_DATE:8>20260101<TIME_ON:6>100000<BAND:3>20M<MODE:2>CW<EOR>";
        const QByteArray two = "<CALL:5>K1BBB<QSO_DATE:8>20260101<TIME_ON:6>110000<BAND:3>20M<MODE:2>CW<EOR>";
        service.enqueueClubLogQso(one, "K1TEST"); service.enqueueClubLogQso(two, "K1TEST");
        service.m_clubLogUploadTimer.stop();
        QJsonArray results;
        for (const auto& value : {qMakePair(QStringLiteral("K1AAA"), 101), qMakePair(QStringLiteral("K1BBB"), 102)}) {
            const QString stamp = value.first == QLatin1String("K1AAA") ? "2026-01-01T10:00:00Z" : "2026-01-01T11:00:00Z";
            const QJsonObject filter{{"dxcall", value.first}, {"band", "20"}, {"mode", "CW"}, {"from", stamp}, {"to", stamp}};
            results.append(QJsonObject{{"filter", filter}, {"qsos", QJsonArray{QJsonObject{{"qsoid", value.second}}}}});
        }
        network->payloads = {R"({"results":[{"status":"created","qsoid":101},{"status":"updated","qsoid":102}]})",
                             QJsonDocument(QJsonObject{{"results", results}}).toJson(QJsonDocument::Compact)};
        network->statuses = {200, 200};
        service.processClubLogOutbox();
        QTRY_COMPARE(network->requests.size(), 2);
        QTRY_VERIFY(service.m_clubLogUploadReply == nullptr);
        QCOMPARE(network->requests.at(0).url().path(), QStringLiteral("/api/v1/qso/batch"));
        QCOMPARE(network->requests.at(1).url().path(), QStringLiteral("/api/v1/logs/K1TEST/qsos/query"));
        QCOMPARE(QJsonDocument::fromJson(network->bodies.first()).object().value("qsos").toArray().size(), 2);
        QSqlQuery result(*service.m_database);
        QVERIFY(result.exec("SELECT COUNT(*) FROM clublog_outbox WHERE state='sent' AND remote_id IN ('101','102')"));
        QVERIFY(result.next()); QCOMPARE(result.value(0).toInt(), 2);
        service.m_clubLogUploadTimer.stop();
        QVERIFY(cleanup.exec("DELETE FROM clublog_outbox"));
    }

    void clubLogDirectFailureDoesNotResendUncertainQso()
    {
        CallsignIntelligenceService service;
        QSqlQuery cleanup(*service.m_database); QVERIFY(cleanup.exec("DELETE FROM clublog_outbox"));
        auto* network = new FakeClubLogNetwork; network->setParent(&service); service.m_network = network;
        service.m_clubLogApiKey = "cl_direct_failure_test"; service.m_clubLogUploadEnabled = true; service.m_operatorCallsign = "K1TEST";
        const QByteArray one = "<CALL:5>K1AAA<QSO_DATE:8>20260101<TIME_ON:6>100000<BAND:3>20M<MODE:2>CW<EOR>";
        service.enqueueClubLogQso(one, "K1TEST"); service.m_clubLogUploadTimer.stop();
        network->statuses = {500, 500}; network->payloads = {"{}", "{}"};
        service.processClubLogOutbox();
        QTRY_COMPARE(network->requests.size(), 2);
        QTRY_VERIFY(service.m_clubLogUploadReply == nullptr);
        QCOMPARE(network->requests.at(0).url().path(), QStringLiteral("/api/v1/qso"));
        QCOMPARE(network->requests.at(1).url().path(), QStringLiteral("/api/v1/logs/K1TEST/qsos/query"));
        service.m_clubLogUploadTimer.stop();
        QSqlQuery result(*service.m_database);
        QVERIFY(result.exec("SELECT state FROM clublog_outbox")); QVERIFY(result.next());
        QCOMPARE(result.value(0).toString(), QStringLiteral("uncertain"));
        const QByteArray two = "<CALL:5>K1BBB<QSO_DATE:8>20260101<TIME_ON:6>110000<BAND:3>20M<MODE:2>CW<EOR>";
        service.enqueueClubLogQso(two, "K1TEST"); service.m_clubLogUploadTimer.stop();
        network->statuses = {201}; network->payloads = {R"({"status":"created","qsoid":102})"};
        service.processClubLogOutbox();
        QTRY_COMPARE(network->requests.size(), 3);
        QTRY_VERIFY(service.m_clubLogUploadReply == nullptr);
        QVERIFY(network->bodies.last().contains("K1BBB"));
        QVERIFY(!network->bodies.last().contains("K1AAA"));
        QVERIFY(result.exec("SELECT COUNT(*) FROM clublog_outbox WHERE state='sent' AND remote_id='102'"));
        QVERIFY(result.next()); QCOMPARE(result.value(0).toInt(), 1);
        QVERIFY(result.exec("SELECT COUNT(*) FROM clublog_outbox WHERE state='uncertain'"));
        QVERIFY(result.next()); QCOMPARE(result.value(0).toInt(), 1);
        service.m_clubLogUploadTimer.stop();
        QVERIFY(cleanup.exec("DELETE FROM clublog_outbox"));
    }

    void clubLogDirect403StopsAndKeepsTheQso()
    {
        CallsignIntelligenceService service;
        QSqlQuery cleanup(*service.m_database); QVERIFY(cleanup.exec("DELETE FROM clublog_outbox"));
        auto* network = new FakeClubLogNetwork; network->setParent(&service); service.m_network = network;
        service.m_clubLogApiKey = "cl_direct_refused_test"; service.m_clubLogUploadEnabled = true; service.m_operatorCallsign = "K1TEST";
        service.enqueueClubLogQso("<CALL:5>K1AAA<QSO_DATE:8>20260101<TIME_ON:6>100000<BAND:3>20M<MODE:2>CW<EOR>", "K1TEST");
        service.m_clubLogUploadTimer.stop();
        network->statuses = {403}; network->payloads = {"{}"};
        service.processClubLogOutbox();
        QTRY_COMPARE(network->requests.size(), 1);
        QTRY_VERIFY(service.m_clubLogUploadReply == nullptr);
        QVERIFY(service.m_clubLogRestAuthRejected);
        service.processClubLogOutbox();
        QCOMPARE(network->requests.size(), 1);
        QSqlQuery result(*service.m_database);
        QVERIFY(result.exec("SELECT state FROM clublog_outbox")); QVERIFY(result.next());
        QCOMPARE(result.value(0).toString(), QStringLiteral("pending"));
        QVERIFY(cleanup.exec("DELETE FROM clublog_outbox"));
    }

    void clubLogDirectPartialPresenceDoesNotInventAResult()
    {
        CallsignIntelligenceService service;
        QSqlQuery cleanup(*service.m_database); QVERIFY(cleanup.exec("DELETE FROM clublog_outbox"));
        auto* network = new FakeClubLogNetwork; network->setParent(&service); service.m_network = network;
        service.m_clubLogApiKey = "cl_direct_partial_test"; service.m_clubLogUploadEnabled = true; service.m_operatorCallsign = "K1TEST";
        service.enqueueClubLogQso("<CALL:5>K1AAA<QSO_DATE:8>20260101<TIME_ON:6>100000<BAND:3>20M<MODE:2>CW<EOR>", "K1TEST");
        service.enqueueClubLogQso("<CALL:5>K1BBB<QSO_DATE:8>20260101<TIME_ON:6>110000<BAND:3>20M<MODE:2>CW<EOR>", "K1TEST");
        service.m_clubLogUploadTimer.stop();
        QJsonArray results;
        for (const auto& call : {QStringLiteral("K1AAA"), QStringLiteral("K1BBB")}) {
            const bool first = call == QLatin1String("K1AAA");
            const QString stamp = first ? "2026-01-01T10:00:00Z" : "2026-01-01T11:00:00Z";
            const QJsonObject filter{{"dxcall", call}, {"band", "20"}, {"mode", "CW"}, {"from", stamp}, {"to", stamp}};
            results.append(QJsonObject{{"filter", filter}, {"qsos", first ? QJsonArray{QJsonObject{{"qsoid", 101}}} : QJsonArray{}}});
        }
        network->statuses = {200, 200};
        network->payloads = {R"({"results":[{"status":"created","qsoid":101},{"status":"rejected","qsoid":null}]})",
                             QJsonDocument(QJsonObject{{"results", results}}).toJson(QJsonDocument::Compact)};
        service.processClubLogOutbox();
        QTRY_COMPARE(network->requests.size(), 2);
        QTRY_VERIFY(service.m_clubLogUploadReply == nullptr);
        QSqlQuery result(*service.m_database);
        QVERIFY(result.exec("SELECT state,remote_id FROM clublog_outbox ORDER BY id"));
        QVERIFY(result.next()); QCOMPARE(result.value(0).toString(), QStringLiteral("sent")); QCOMPARE(result.value(1).toString(), QStringLiteral("101"));
        QVERIFY(result.next()); QCOMPARE(result.value(0).toString(), QStringLiteral("uncertain"));
        service.m_clubLogUploadTimer.stop(); QVERIFY(cleanup.exec("DELETE FROM clublog_outbox"));
    }

    void clubLogDirectOnlySendsTheActiveStation()
    {
        CallsignIntelligenceService service;
        QSqlQuery cleanup(*service.m_database); QVERIFY(cleanup.exec("DELETE FROM clublog_outbox"));
        auto* network = new FakeClubLogNetwork; network->setParent(&service); service.m_network = network;
        service.m_clubLogApiKey = "cl_direct_station_test"; service.m_clubLogUploadEnabled = true; service.m_operatorCallsign = "K1OTHER";
        service.enqueueClubLogQso("<CALL:5>K1AAA<QSO_DATE:8>20260101<TIME_ON:6>100000<BAND:3>20M<MODE:2>CW<EOR>", "K1TEST");
        service.m_clubLogUploadTimer.stop(); service.processClubLogOutbox();
        QVERIFY(network->requests.isEmpty());
        QVERIFY(cleanup.exec("DELETE FROM clublog_outbox"));
    }

    void clubLogDirectRejectedCanBeRetriedWithoutLeakingToken()
    {
        CallsignIntelligenceService service;
        QSqlQuery cleanup(*service.m_database); QVERIFY(cleanup.exec("DELETE FROM clublog_outbox"));
        auto* network = new FakeClubLogNetwork; network->setParent(&service); service.m_network = network;
        service.m_clubLogApiKey = "cl_reject_secret"; service.m_clubLogUploadEnabled = true; service.m_operatorCallsign = "K1TEST";
        service.enqueueClubLogQso("<CALL:5>K1AAA<QSO_DATE:8>20260101<TIME_ON:6>100000<BAND:3>20M<MODE:2>CW<EOR>", "K1TEST");
        service.m_clubLogUploadTimer.stop();
        network->statuses = {422}; network->payloads = {R"({"message":"invalid cl_reject_secret"})"};
        service.processClubLogOutbox();
        QTRY_VERIFY(service.m_clubLogUploadReply == nullptr);
        QSqlQuery result(*service.m_database);
        QVERIFY(result.exec("SELECT state,last_error FROM clublog_outbox")); QVERIFY(result.next());
        QCOMPARE(result.value(0).toString(), QStringLiteral("rejected"));
        QVERIFY(!result.value(1).toString().contains("cl_reject_secret"));
        service.retryClubLogRejected(); service.m_clubLogUploadTimer.stop();
        QVERIFY(result.exec("SELECT state FROM clublog_outbox")); QVERIFY(result.next());
        QCOMPARE(result.value(0).toString(), QStringLiteral("pending"));
        QVERIFY(cleanup.exec("DELETE FROM clublog_outbox"));
    }

    void clubLogDirectQueueSurvivesRestart()
    {
        {
            CallsignIntelligenceService service;
            QSqlQuery cleanup(*service.m_database); QVERIFY(cleanup.exec("DELETE FROM clublog_outbox"));
            service.m_clubLogUploadEnabled = true;
            service.enqueueClubLogQso("<CALL:5>K1AAA<QSO_DATE:8>20260101<TIME_ON:6>100000<BAND:3>20M<MODE:2>CW<EOR>", "K1TEST");
            service.m_clubLogUploadTimer.stop();
        }
        {
            CallsignIntelligenceService service;
            auto* network = new FakeClubLogNetwork; network->setParent(&service); service.m_network = network;
            service.m_clubLogApiKey = "cl_restart_test"; service.m_clubLogUploadEnabled = true; service.m_operatorCallsign = "K1TEST";
            network->statuses = {201}; network->payloads = {R"({"status":"created","qsoid":333})"};
            service.processClubLogOutbox();
            QTRY_COMPARE(network->requests.size(), 1);
            QTRY_VERIFY(service.m_clubLogUploadReply == nullptr);
            QSqlQuery query(*service.m_database);
            QVERIFY(query.exec("SELECT state,remote_id FROM clublog_outbox")); QVERIFY(query.next());
            QCOMPARE(query.value(0).toString(), QStringLiteral("sent")); QCOMPARE(query.value(1).toString(), QStringLiteral("333"));
            service.m_clubLogUploadTimer.stop();
            QVERIFY(query.exec("DELETE FROM clublog_outbox"));
        }
    }

    void initTestCase()
    {
        // Keep the SQLite cache and profile settings outside the user's live
        // Decodium configuration when the test suite runs.
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                           QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
        settings.remove(QStringLiteral("ClubLogRestAccess"));
        settings.sync();
    }

    void importsProviderDataAndUsesCache()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("eqsl.csv"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("Callsign,Grid Square,Name,QTH,State\nIU8LMC,JN70ES,Salvatore,Malta,MT\n");
        file.close();

        CallsignIntelligenceService service;
        const bool originalAutoOpen = service.autoOpenOnQsoStart();
        service.setAutoOpenOnQsoStart(false);
        QVERIFY(!service.autoOpenOnQsoStart());
        QVERIFY(service.importDatabase(QStringLiteral("eqsl"), path));
        service.clearCache(QStringLiteral("IU8LMC"));

        service.lookup(QStringLiteral("iu8lmc"));
        QCOMPARE(service.result().value(QStringLiteral("call")).toString(), QStringLiteral("IU8LMC"));
        QCOMPARE(service.result().value(QStringLiteral("grid")).toString(), QStringLiteral("JN70ES"));
        QCOMPARE(service.result().value(QStringLiteral("name")).toString(), QStringLiteral("Salvatore"));
        QVERIFY(service.result().value(QStringLiteral("eqsl")).toBool());
        QVERIFY(!service.cacheHit());

        service.lookup(QStringLiteral("IU8LMC"));
        QVERIFY(service.cacheHit());
        QVERIFY(service.databases().size() >= 5);
        service.setAutoOpenOnQsoStart(originalAutoOpen);
    }

    void importsLotwHeaderlessActivity()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("lotw.csv"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("K1ABC,2026-07-31,12:00:00\n");
        file.close();

        CallsignIntelligenceService service;
        QVERIFY(service.importDatabase(QStringLiteral("lotw"), path));
        service.clearCache(QStringLiteral("K1ABC"));
        service.lookup(QStringLiteral("K1ABC"));
        QVERIFY(service.result().value(QStringLiteral("lotw")).toBool());
        QCOMPARE(service.result().value(QStringLiteral("lastUpload")).toString(), QStringLiteral("2026-07-31"));
    }

    void parsesEqslInboxDownloadPage()
    {
        const QUrl inboxUrl(QStringLiteral("https://www.eqsl.cc/qslcard/DownloadInbox.cfm"));
        const QByteArray page = QByteArrayLiteral(
            "<html><a href=\"../DownloadedFiles/Inbox-123.ADI\">.ADI file</a></html>");
        const auto result = decodium::eqsl::parseInboxPage(page, inboxUrl);
        QCOMPARE(result.kind, decodium::eqsl::InboxPageKind::DownloadReady);
        QCOMPARE(result.adifUrl,
                 QUrl(QStringLiteral("https://www.eqsl.cc/DownloadedFiles/Inbox-123.ADI")));
    }

    void classifiesEqslInboxNoRecordsAndLoginFailure()
    {
        const QUrl inboxUrl(QStringLiteral("https://www.eqsl.cc/qslcard/DownloadInbox.cfm"));
        QCOMPARE(decodium::eqsl::parseInboxPage("<html>No records</html>", inboxUrl).kind,
                 decodium::eqsl::InboxPageKind::NoRecords);
        QCOMPARE(decodium::eqsl::parseInboxPage("<html>Error: No such Callsign found</html>", inboxUrl).kind,
                 decodium::eqsl::InboxPageKind::AuthenticationError);
        QCOMPARE(decodium::eqsl::parseInboxPage("Decodium\n<EOH>\n", inboxUrl).kind,
                 decodium::eqsl::InboxPageKind::DirectAdif);
    }

    void classifiesLotwReportResponses()
    {
        QCOMPARE(decodium::lotw::parseReportResponse("Logbook of the World\n<EOH>\n").kind,
                 decodium::lotw::ReportResponseKind::Adif);
        QCOMPARE(decodium::lotw::parseReportResponse(
                     "<html><i>Username/password incorrect</i></html>").kind,
                 decodium::lotw::ReportResponseKind::AuthenticationError);
        QCOMPARE(decodium::lotw::parseReportResponse(
                     "<html>Service temporarily unavailable; try again later</html>").kind,
                 decodium::lotw::ReportResponseKind::ServiceError);
    }

    void manuallyImportsLotwConfirmationAdiThroughLogbookPipeline()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString path = directory.filePath(QStringLiteral("lotw-confirmed.adi"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write("Generated by LoTW\n<APP_LoTW_LASTQSL:19>2026-08-06 21:00:00\n<EOH>\n"
                   "<CALL:5>K1ABC<QSO_DATE:8>20260806<TIME_ON:6>210000<BAND:3>20M<MODE:3>FT8<EOR>\n");
        file.close();

        CallsignIntelligenceService service;
        QSignalSpy downloadedSpy(&service, &CallsignIntelligenceService::confirmedAdifDownloaded);
        QVERIFY(service.importDatabase(QStringLiteral("lotw_confirmed"), path));
        QTRY_COMPARE(downloadedSpy.count(), 1);
        QCOMPARE(downloadedSpy.at(0).at(0).toString(), QStringLiteral("lotw_confirmed"));
        QVERIFY(QFileInfo(downloadedSpy.at(0).at(1).toString()).exists());
    }

    void showsConfirmedSourceCountInsteadOfAStaleZero()
    {
        CallsignIntelligenceService service;
        service.completeConfirmedAdifImport(QStringLiteral("lotw_confirmed"), true, 27, 14, 41);
        const QVariantList providers = service.databases();
        for (const QVariant& value : providers) {
            const QVariantMap provider = value.toMap();
            if (provider.value(QStringLiteral("id")).toString() == QStringLiteral("lotw_confirmed")) {
                QCOMPARE(provider.value(QStringLiteral("rowCount")).toInt(), 41);
                return;
            }
        }
        QFAIL("LoTW confirmations provider is missing");
    }

    void qsoHooksHonorDefaultsAndSettings()
    {
        CallsignIntelligenceService service;
        const bool originalAutoOpen = service.autoOpenOnQsoStart();
        const bool originalAutoClose = service.autoCloseAfterLogging();
        service.setAutoOpenOnQsoStart(false);
        service.setAutoCloseAfterLogging(false);
        QSignalSpy openSpy(&service, &CallsignIntelligenceService::lookupWindowRequested);
        QSignalSpy closeSpy(&service, &CallsignIntelligenceService::lookupWindowCloseRequested);

        service.notifyQsoStarted(QStringLiteral("K1ABC"));
        QCOMPARE(openSpy.count(), 0);
        service.setAutoOpenOnQsoStart(true);
        service.setAutoCloseAfterLogging(true);
        service.notifyQsoStarted(QStringLiteral("K1ABC"));
        QCOMPARE(openSpy.count(), 1);
        service.notifyQsoLogged(QStringLiteral("K1ABC"));
        QCOMPARE(closeSpy.count(), 1);
        service.setAutoOpenOnQsoStart(originalAutoOpen);
        service.setAutoCloseAfterLogging(originalAutoClose);
    }

    void externalLookupsEmitCanonicalUrlsEvenInOfflineMode()
    {
        CallsignIntelligenceService service;
        service.setOfflineMode(true);
        QSignalSpy lookupSpy(
            &service,
            &CallsignIntelligenceService::externalLookupRequested);

        const QList<QPair<QString, QString>> cases {
            {QStringLiteral("qrz"),
             QStringLiteral("https://www.qrz.com/db/IU3VGK")},
            {QStringLiteral("fcc_uls"),
             QStringLiteral("https://wireless2.fcc.gov/UlsApp/UlsSearch/searchLicense.jsp?callSign=IU3VGK")},
            {QStringLiteral("eqsl"),
             QStringLiteral("https://www.eqsl.cc/Member.cfm?IU3VGK")},
            {QStringLiteral("clublog"),
             QStringLiteral("https://clublog.org/logsearch/IU3VGK")}
        };

        for (const auto& testCase : cases) {
            QVERIFY(service.openProviderLookup(testCase.first,
                                               QStringLiteral("iu3vgk")));
            QCOMPARE(lookupSpy.count(), 1);
            QCOMPARE(lookupSpy.takeFirst().at(0).toString(), testCase.second);
        }

        QVERIFY(!service.openProviderLookup(QStringLiteral("qrz"),
                                            QStringLiteral("invalid!")));
        QCOMPARE(lookupSpy.count(), 0);
    }

    void clubLogLegacyAndRestLookupsUseDifferentAuthentication()
    {
        CallsignIntelligenceService service;
        auto* network = new FakeClubLogNetwork;
        network->setParent(&service);
        service.m_network = network;
        network->payloads = {R"({"qra":"JM75"})", R"({"entity":"Malta","continent":"EU","cq_zone":15})"};
        service.m_clubLogApiKey = QStringLiteral("legacy_fixture");
        service.lookupRemoteClubLog(QStringLiteral("Z7TEST"));
        QTRY_COMPARE(network->requests.size(), 1);
        QTRY_VERIFY(service.m_activeReply == nullptr);
        const QNetworkRequest legacy = network->requests.at(0);
        QCOMPARE(legacy.url().path(), QStringLiteral("/watch.php"));
        QCOMPARE(QUrlQuery(legacy.url()).queryItemValue(QStringLiteral("api")), QStringLiteral("legacy_fixture"));
        QVERIFY(legacy.rawHeader("Authorization").isEmpty());
        service.m_clubLogApiKey = QStringLiteral("cl_fixture");
        service.lookupRemoteClubLog(QStringLiteral("Z8TEST"));
        QTRY_COMPARE(network->requests.size(), 2);
        QTRY_VERIFY(service.m_activeReply == nullptr);
        const QNetworkRequest modern = network->requests.at(1);
        QCOMPARE(modern.url().path(), QStringLiteral("/api/v1/dxcc/lookup"));
        QCOMPARE(modern.rawHeader("Authorization"), QByteArray("Bearer cl_fixture"));
        QVERIFY(!modern.url().toString().contains(QStringLiteral("cl_fixture")));
        QCOMPARE(service.result().value(QStringLiteral("country")).toString(), QStringLiteral("Malta"));
    }

    void clubLogLegacyOqrsPreservesItsAuthenticationPath()
    {
        CallsignIntelligenceService service;
        auto* network = new FakeClubLogNetwork;
        network->setParent(&service); service.m_network = network;
        service.m_offlineMode = false;
        service.m_clubLogApiKey = QStringLiteral("legacy_fixture");
        service.m_clubLogEmail = QStringLiteral("test@example.org");
        service.m_clubLogApplicationPassword = QStringLiteral("legacy_password");
        service.m_operatorCallsign = QStringLiteral("9H1SR");
        network->payloads = {R"([["Z5TEST","2026-01-01","20m","FT8"]])"};
        service.refreshDatabase(QStringLiteral("clublog_oqrs"));
        QTRY_COMPARE_WITH_TIMEOUT(network->requests.size(), 1, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!service.databaseUpdatePending(), 10000);
        const QNetworkRequest request = network->requests.first();
        QCOMPARE(request.url().path(), QStringLiteral("/getoqrsmatches.php"));
        const QUrlQuery query(request.url());
        QCOMPARE(query.queryItemValue(QStringLiteral("api")), QStringLiteral("legacy_fixture"));
        QCOMPARE(query.queryItemValue(QStringLiteral("email")), QStringLiteral("test@example.org"));
        QVERIFY(request.rawHeader("Authorization").isEmpty());
    }

    void clubLogRestStopsAfterRefusalOrRateLimit()
    {
        CallsignIntelligenceService service;
        auto* network = new FakeClubLogNetwork;
        network->setParent(&service); service.m_network = network;
        service.m_offlineMode = false;
        service.m_clubLogApiKey = QStringLiteral("cl_fixture");
        network->statuses = {401}; network->payloads = {R"({"message":"invalid token"})"};
        service.lookupRemoteClubLog(QStringLiteral("Z6TEST"));
        QTRY_VERIFY(service.m_activeReply == nullptr);
        QVERIFY(service.m_clubLogRestAuthRejected);
        service.lookup(QStringLiteral("Z1NOCACHE"), true);
        QCOMPARE(network->requests.size(), 1);
        service.m_clubLogRestAuthRejected = false;
        network->statuses = {429}; network->payloads = {R"({"message":"rate limit"})"};
        service.lookupRemoteClubLog(QStringLiteral("Z6TEST"));
        QTRY_VERIFY(service.m_activeReply == nullptr);
        QVERIFY(service.m_clubLogRetryNotBeforeMs >= QDateTime::currentMSecsSinceEpoch() + 299000);
        service.lookup(QStringLiteral("Z2NOCACHE"), true);
        QCOMPARE(network->requests.size(), 2);
    }

    void clubLogRestOqrsFollowsCursorWithoutMarkingConfirmation()
    {
        CallsignIntelligenceService service;
        auto* network = new FakeClubLogNetwork;
        network->setParent(&service); service.m_network = network;
        service.m_clubLogApiKey = QStringLiteral("cl_fixture");
        service.m_offlineMode = false;
        service.m_operatorCallsign = QStringLiteral("9H1SR");
        network->payloads = {R"({"matches":[{"dxcall":"Z7TEST","band":"20","mode":"FT8"}],"next_cursor":"next-page"})",
                             R"({"matches":[{"dxcall":"Z8TEST","band":"40","mode":"CW"}],"next_cursor":null})"};
        service.refreshDatabase(QStringLiteral("clublog_oqrs"));
        QTRY_COMPARE_WITH_TIMEOUT(network->requests.size(), 2, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!service.databaseUpdatePending(), 10000);
        QCOMPARE(QUrlQuery(network->requests.at(1).url()).queryItemValue(QStringLiteral("cursor")), QStringLiteral("next-page"));
        for (const QNetworkRequest& request : std::as_const(network->requests)) {
            QCOMPARE(request.rawHeader("Authorization"), QByteArray("Bearer cl_fixture"));
            QVERIFY(!request.url().toString().contains(QStringLiteral("cl_fixture")));
        }
        service.clearCache(QStringLiteral("Z8TEST"));
        service.lookup(QStringLiteral("Z8TEST"));
        QVERIFY(service.result().value(QStringLiteral("oqrs")).toBool());
        QVERIFY(!service.result().value(QStringLiteral("confirmed")).toBool());
    }

    void clubLogOqrs403StopsFurtherRequests()
    {
        CallsignIntelligenceService service;
        auto* network = new FakeClubLogNetwork;
        network->setParent(&service); service.m_network = network;
        service.m_clubLogApiKey = QStringLiteral("cl_fixture");
        service.m_offlineMode = false;
        service.m_operatorCallsign = QStringLiteral("9H1SR");
        network->statuses = {403};
        network->payloads = {R"({"message":"access denied"})"};
        service.refreshDatabase(QStringLiteral("clublog_oqrs"));
        QTRY_COMPARE(network->requests.size(), 1);
        QTRY_VERIFY(!service.databaseUpdatePending());
        QVERIFY(service.m_clubLogRestAuthRejected);
        service.refreshDatabase(QStringLiteral("clublog_oqrs"));
        QCOMPARE(network->requests.size(), 1);
    }

    void clubLogOqrsSnapshotReplacesStaleRowsAndInvalidatesCache()
    {
        CallsignIntelligenceService service;
        auto* network = new FakeClubLogNetwork;
        network->setParent(&service); service.m_network = network;
        service.m_clubLogApiKey = QStringLiteral("cl_fixture");
        service.m_offlineMode = false;
        service.m_operatorCallsign = QStringLiteral("9H1SR");
        network->payloads = {R"({"matches":[{"dxcall":"Z1OLD"}],"next_cursor":null})"};
        service.refreshDatabase(QStringLiteral("clublog_oqrs"));
        QTRY_VERIFY_WITH_TIMEOUT(!service.databaseUpdatePending(), 10000);
        QVERIFY(service.localLookup(QStringLiteral("Z1OLD")).value(QStringLiteral("oqrs")).toBool());
        service.cacheResult(service.localLookup(QStringLiteral("Z1OLD")));
        QVERIFY(service.cachedLookup(QStringLiteral("Z1OLD")).value(QStringLiteral("oqrs")).toBool());

        network->payloads = {R"({"matches":[{"dxcall":"Z2NEW"}],"next_cursor":null})"};
        service.refreshDatabase(QStringLiteral("clublog_oqrs"));
        QTRY_VERIFY_WITH_TIMEOUT(!service.databaseUpdatePending(), 10000);
        QVERIFY(!service.localLookup(QStringLiteral("Z1OLD")).value(QStringLiteral("oqrs")).toBool());
        QVERIFY(service.cachedLookup(QStringLiteral("Z1OLD")).isEmpty());
        QVERIFY(service.localLookup(QStringLiteral("Z2NEW")).value(QStringLiteral("oqrs")).toBool());

        network->payloads = {R"({"matches":[{"dxcall":"Z3PART"}],"next_cursor":"cl_fixture"})"};
        const int requestsBefore = network->requests.size();
        service.refreshDatabase(QStringLiteral("clublog_oqrs"));
        QTRY_VERIFY_WITH_TIMEOUT(!service.databaseUpdatePending(), 10000);
        QCOMPARE(network->requests.size(), requestsBefore + 1);
        QVERIFY(service.localLookup(QStringLiteral("Z2NEW")).value(QStringLiteral("oqrs")).toBool());

        network->payloads = {R"({"matches":[{"dxcall":"Z3PART"}],"next_cursor":42})"};
        service.refreshDatabase(QStringLiteral("clublog_oqrs"));
        QTRY_VERIFY_WITH_TIMEOUT(!service.databaseUpdatePending(), 10000);
        QVERIFY(service.localLookup(QStringLiteral("Z2NEW")).value(QStringLiteral("oqrs")).toBool());
        QVERIFY(!service.localLookup(QStringLiteral("Z3PART")).value(QStringLiteral("oqrs")).toBool());

        network->payloads = {R"({"matches":[{"dxcall":"BAD$"}],"next_cursor":null})"};
        service.refreshDatabase(QStringLiteral("clublog_oqrs"));
        QTRY_VERIFY_WITH_TIMEOUT(!service.databaseUpdatePending(), 10000);
        QVERIFY(service.localLookup(QStringLiteral("Z2NEW")).value(QStringLiteral("oqrs")).toBool());

        network->payloads = {R"({"matches":[],"next_cursor":null})"};
        service.refreshDatabase(QStringLiteral("clublog_oqrs"));
        QTRY_VERIFY_WITH_TIMEOUT(!service.databaseUpdatePending(), 10000);
        QVERIFY(!service.localLookup(QStringLiteral("Z2NEW")).value(QStringLiteral("oqrs")).toBool());
    }

    void clubLogOldTokenRefusalDoesNotBlockNewToken()
    {
        CallsignIntelligenceService service;
        auto* network = new FakeClubLogNetwork;
        network->setParent(&service); service.m_network = network;
        service.m_clubLogApiKey = QStringLiteral("cl_old");
        service.m_offlineMode = false;
        network->statuses = {401};
        network->payloads = {R"({"message":"invalid token"})"};
        service.lookupRemoteClubLog(QStringLiteral("Z6TEST"));
        service.m_clubLogApiKey = QStringLiteral("cl_new");
        QTRY_VERIFY(service.m_activeReply == nullptr);
        QVERIFY(!service.m_clubLogRestAuthRejected);
    }

    void importsRestOqrsAsRequestNotConfirmation()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile file(directory.filePath(QStringLiteral("rest-oqrs.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"([{"call":"9H1SR","dxcall":"Z8TEST","qsodate":"2026-11-01T12:00:00Z","band":"20","mode":"FT8"}])");
        file.close();
        CallsignIntelligenceService service;
        service.clearCache(QStringLiteral("Z8TEST"));
        QVERIFY(service.importDatabase(QStringLiteral("clublog_oqrs"), file.fileName()));
        service.lookup(QStringLiteral("Z8TEST"));
        QVERIFY(service.result().value(QStringLiteral("oqrs")).toBool());
        QVERIFY(!service.result().value(QStringLiteral("confirmed")).toBool());
    }

    void importsClubLogOqrsAndEmitsEnrichment()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString oqrsPath = directory.filePath(QStringLiteral("oqrs.json"));
        QFile oqrs(oqrsPath);
        QVERIFY(oqrs.open(QIODevice::WriteOnly | QIODevice::Text));
        oqrs.write("[[\"Z9TEST\",\"2026-08-01\",\"20m\",\"FT8\"]]");
        oqrs.close();

        CallsignIntelligenceService service;
        service.clearCache(QStringLiteral("Z9TEST"));
        QVERIFY(service.importDatabase(QStringLiteral("clublog_oqrs"), oqrsPath));
        service.lookup(QStringLiteral("Z9TEST"));
        QVERIFY(service.result().value(QStringLiteral("oqrs")).toBool());
        QVERIFY(service.result().value(QStringLiteral("confirmed")).toBool());

        const QString profilePath = directory.filePath(QStringLiteral("profile.csv"));
        QFile profile(profilePath);
        QVERIFY(profile.open(QIODevice::WriteOnly | QIODevice::Text));
        QStringList fccFields(12);
        fccFields[0] = QStringLiteral("EN");
        fccFields[1] = QStringLiteral("Z9TEST");
        fccFields[7] = QStringLiteral("Test Operator");
        fccFields[10] = QStringLiteral("Malta");
        fccFields[11] = QStringLiteral("MT");
        profile.write((fccFields.join(QLatin1Char('|')) + QLatin1Char('\n')).toUtf8());
        profile.close();
        QVERIFY(service.importDatabase(QStringLiteral("fcc_uls"), profilePath));

        const QString enrichmentPath = directory.filePath(QStringLiteral("enrichment.csv"));
        QFile enrichment(enrichmentPath);
        QVERIFY(enrichment.open(QIODevice::WriteOnly | QIODevice::Text));
        enrichment.write("CALL,GRID\nZ9TEST,JN70ES\n");
        enrichment.close();
        QVERIFY(service.importDatabase(QStringLiteral("eqsl"), enrichmentPath));

        QSignalSpy enrichmentSpy(&service, &CallsignIntelligenceService::enrichmentReady);
        const bool originalEnrichment = service.enrichMissingFields();
        service.setEnrichMissingFields(true);
        service.lookup(QStringLiteral("Z9TEST"), true);
        QCOMPARE(enrichmentSpy.count(), 1);
        const QList<QVariant> arguments = enrichmentSpy.at(0);
        QCOMPARE(arguments.at(0).toString(), QStringLiteral("Z9TEST"));
        QCOMPARE(arguments.at(1).toMap().value(QStringLiteral("grid")).toString(), QStringLiteral("JN70ES"));
        QCOMPARE(service.result().value(QStringLiteral("provider")).toString(), QStringLiteral("fcc_uls, eqsl, clublog_oqrs"));
        service.setEnrichMissingFields(originalEnrichment);
    }
};

QTEST_MAIN(CallsignIntelligenceTest)
#include "test_callsign_intelligence.moc"
