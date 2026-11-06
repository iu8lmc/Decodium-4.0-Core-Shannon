// SPDX-License-Identifier: GPL-3.0-or-later
#include "src/sstv/digital/HamDrmHybridFtp.h"
#include "src/sstv/digital/HamDrmController.h"
#include <QBuffer>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

namespace hamdrm = decodium::sstv::hamdrm;

class RecordingTx final : public hamdrm::HamDrmWaveformTxBackend {
public:
    int starts = 0;
    hamdrm::HamDrmEncodedObject object;
    hamdrm::HamDrmWaveformCapability capability() const override { return {true, "test", "test"}; }
    hamdrm::HamDrmStatus start(const hamdrm::HamDrmProfile&, hamdrm::HamDrmEncodedObject input,
                               std::uint64_t, hamdrm::HamDrmWaveformTxSink&) override {
        ++starts; object = std::move(input); return hamdrm::HamDrmStatus::success();
    }
    void cancel(std::uint64_t) noexcept override {}
};

class LocalFtp final : public QObject {
    Q_OBJECT
public:
    QTcpServer controlServer, dataServer;
    QTcpSocket* control = nullptr;
    QTcpSocket* data = nullptr;
    QByteArray pending, payload, uploaded, retrievedName;
    QStringList commands;
    bool pasv = false, rejectLogin = false, uploading = false, rejectUpload = false, stallLogin = false, omitUploadConfirmation = false;
    int slowChunkMs = 0, stallAfterChunks = 0, sentChunks = 0, slowUploadChunkMs = 0;
    char epsvDelimiter = '|';
    QByteArray epsvOverride;
    QByteArray greetingOverride;
    explicit LocalFtp(QObject* parent = nullptr) : QObject(parent) {
        connect(&controlServer, &QTcpServer::newConnection, this, [this] {
            control = controlServer.nextPendingConnection();
            control->write(greetingOverride.isEmpty() ? QByteArray("220-Welcome\r\n220 Ready\r\n") : greetingOverride);
            connect(control, &QTcpSocket::readyRead, this, [this] {
                pending += control->readAll();
                while (pending.contains("\r\n")) {
                    const int end = pending.indexOf("\r\n");
                    const QByteArray command = pending.left(end);
                    pending.remove(0, end + 2);
                    // Record command verbs only, never test credentials.
                    commands.append(QString::fromLatin1(command.left(command.indexOf(' ') < 0 ? command.size() : command.indexOf(' '))));
                    if (command.startsWith("USER ")) { if (!stallLogin) control->write("331 Password\r\n"); }
                    else if (command.startsWith("PASS ")) control->write(rejectLogin ? "530 Denied\r\n" : "230 Logged in\r\n");
                    else if (command == "TYPE I") control->write("200 Binary\r\n");
                    else if (command.startsWith("CWD ")) control->write("250 Directory\r\n");
                    else if (command == "EPSV" && pasv) control->write("500 Unsupported\r\n");
                    else if (command == "EPSV" || command == "PASV") {
                        if (!dataServer.isListening()) dataServer.listen(QHostAddress::LocalHost, 0);
                        const auto port = dataServer.serverPort();
                        if (command == "EPSV" && !epsvOverride.isEmpty()) control->write(epsvOverride);
                        else if (command == "EPSV") {
                            const QByteArray delimiter(1, epsvDelimiter);
                            control->write("229 Entering (" + delimiter + delimiter + delimiter
                                           + QByteArray::number(port) + delimiter + ")\r\n");
                        }
                        else control->write("227 Entering (203,0,113,42," + QByteArray::number(port / 256) + "," + QByteArray::number(port % 256) + ")\r\n");
                    } else if (command.startsWith("RETR ")) {
                        retrievedName = command.mid(5);
                        control->write("150 Opening\r\n");
                        if (slowChunkMs > 0 && data) {
                            auto* timer = new QTimer(this);
                            timer->setInterval(slowChunkMs);
                            connect(timer, &QTimer::timeout, this, [this, timer] {
                                if (stallAfterChunks > 0 && sentChunks >= stallAfterChunks) {
                                    timer->stop(); timer->deleteLater(); return;
                                }
                                const int offset = sentChunks * 4;
                                const QByteArray part = payload.mid(offset, 4);
                                if (!part.isEmpty()) { data->write(part); ++sentChunks; }
                                if (offset + part.size() >= payload.size()) {
                                    timer->stop(); timer->deleteLater();
                                    data->disconnectFromHost(); control->write("226 Complete\r\n");
                                }
                            });
                            timer->start();
                        } else {
                            if (data) { data->write(payload); data->disconnectFromHost(); }
                            control->write("226 Complete\r\n");
                        }
                    } else if (command.startsWith("STOR ")) {
                        if (rejectUpload) control->write("550 Denied\r\n");
                        else { uploading = true; control->write("150 Opening\r\n"); }
                    }
                }
            });
        });
        connect(&dataServer, &QTcpServer::newConnection, this, [this] {
            data = dataServer.nextPendingConnection();
            if (slowUploadChunkMs > 0) {
                data->setReadBufferSize(65536);
                auto* timer = new QTimer(data);
                timer->setInterval(slowUploadChunkMs);
                connect(timer, &QTimer::timeout, this, [this] { uploaded += data->read(65536); });
                timer->start();
            } else {
                connect(data, &QTcpSocket::readyRead, this, [this] { uploaded += data->readAll(); });
            }
            connect(data, &QTcpSocket::disconnected, this, [this] {
                uploaded += data->readAll();
                if (uploading) {
                    if (!omitUploadConfirmation) control->write("226 Complete\r\n");
                    uploading = false;
                }
            });
        });
    }
};

class TestHybridFtp : public QObject {
    Q_OBJECT
private slots:
    void oversizedControlReplyAB_data() {
        QTest::addColumn<int>("length");
        QTest::newRow("A-one-over") << 16385;
        QTest::newRow("B-large") << (2 * 1024 * 1024);
    }
    void oversizedControlReplyAB() {
        QFETCH(int, length);
        LocalFtp server;
        server.greetingOverride = QByteArray("220 ") + QByteArray(length - 4, 'x');
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        hamdrm::HamDrmHybridFtp client;
        QSignalSpy result(&client, &hamdrm::HamDrmHybridFtp::finished);
        QVERIFY(client.download({"127.0.0.1", "user", "dummy", "radio"},
                                "de_TEST-1-image.jpg", server.controlServer.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 5000);
        QVERIFY(!result[0][0].toBool());
        QVERIFY(result[0][2].toString().contains("too large"));
        QVERIFY(result[0][1].toByteArray().isEmpty());
        QVERIFY(!server.commands.contains("USER"));
    }
    void alternateEpsvDelimiterAB_data() {
        QTest::addColumn<char>("delimiter");
        QTest::newRow("A-exclamation") << '!';
        QTest::newRow("B-hash") << '#';
        QTest::newRow("opening-parenthesis") << '(';
        QTest::newRow("closing-parenthesis") << ')';
        QTest::newRow("digit") << '1';
        QTest::newRow("highest-ascii") << '~';
    }
    void alternateEpsvDelimiterAB() {
        QFETCH(char, delimiter);
        LocalFtp server;
        server.epsvDelimiter = delimiter;
        server.payload = delimiter == '!' ? QByteArray("alternative delimiter A") : QByteArray("alternative delimiter B");
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        hamdrm::HamDrmHybridFtp client;
        QSignalSpy result(&client, &hamdrm::HamDrmHybridFtp::finished);
        QVERIFY(client.download({"127.0.0.1", "user", "dummy", "radio"},
                                "de_TEST-1-image.jpg", server.controlServer.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 5000);
        QVERIFY2(result[0][0].toBool(), qPrintable(result[0][2].toString()));
        QCOMPARE(result[0][1].toByteArray(), server.payload);
        QVERIFY(!server.commands.contains("PASV"));
        const QByteArray upload = server.payload + " upload";
        QVERIFY(client.upload({"127.0.0.1", "user", "dummy", "radio"},
                              "de_TEST-1-image.jpg", upload, server.controlServer.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 2, 5000);
        QVERIFY2(result[1][0].toBool(), qPrintable(result[1][2].toString()));
        QCOMPARE(server.uploaded, upload);
    }
    void malformedEpsvRejectedAB_data() {
        QTest::addColumn<QByteArray>("response");
        QTest::newRow("A-mismatched-delimiter") << QByteArray("229 Entering (!!!1234#)\r\n");
        QTest::newRow("B-zero-port") << QByteArray("229 Entering (###0#)\r\n");
        QTest::newRow("out-of-range-port") << QByteArray("229 Entering (!!!65536!)\r\n");
        QTest::newRow("space-delimiter") << QByteArray("229 Entering (   1234 )\r\n");
    }
    void malformedEpsvRejectedAB() {
        QFETCH(QByteArray, response);
        LocalFtp server;
        server.epsvOverride = response;
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        hamdrm::HamDrmHybridFtp client;
        QSignalSpy result(&client, &hamdrm::HamDrmHybridFtp::finished);
        QVERIFY(client.download({"127.0.0.1", "user", "dummy", "radio"},
                                "de_TEST-1-image.jpg", server.controlServer.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 5000);
        QVERIFY(!result[0][0].toBool());
        QVERIFY(result[0][1].toByteArray().isEmpty());
        QVERIFY(!server.commands.contains("RETR"));
    }
    void slowDownloadUsesIdleTimeoutAB_data() { downloadAB_data(); }
    void slowDownloadUsesIdleTimeoutAB() {
        QFETCH(bool, pasv);
        LocalFtp server;
        server.pasv = pasv;
        server.payload = pasv ? QByteArray("slow-image-B-with-progress") : QByteArray("slow-image-A-with-progress");
        server.slowChunkMs = 100;
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        hamdrm::HamDrmHybridFtp client(nullptr, 350);
        QSignalSpy result(&client, &hamdrm::HamDrmHybridFtp::finished);
        QElapsedTimer elapsed; elapsed.start();
        QVERIFY(client.download({"127.0.0.1", "user", "dummy", "radio"},
                                "de_TEST-1-image.jpg", server.controlServer.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 5000);
        QVERIFY2(result[0][0].toBool(), qPrintable(result[0][2].toString()));
        QCOMPARE(result[0][1].toByteArray(), server.payload);
        QVERIFY(elapsed.elapsed() > 350);
        QVERIFY(server.sentChunks > 3);
    }
    void stalledDownloadTimesOutAB_data() { downloadAB_data(); }
    void stalledDownloadTimesOutAB() {
        QFETCH(bool, pasv);
        LocalFtp server;
        server.pasv = pasv;
        server.payload = QByteArray("unfinished-file-with-no-further-progress");
        server.slowChunkMs = 100;
        server.stallAfterChunks = 1;
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        hamdrm::HamDrmHybridFtp client(nullptr, 350);
        QSignalSpy result(&client, &hamdrm::HamDrmHybridFtp::finished);
        QVERIFY(client.download({"127.0.0.1", "user", "dummy", "radio"},
                                "de_TEST-1-image.jpg", server.controlServer.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 5000);
        QVERIFY(!result[0][0].toBool());
        QVERIFY(result[0][2].toString().contains("timed out"));
        QVERIFY(result[0][1].toByteArray().isEmpty());
        QCOMPARE(server.sentChunks, 1);
    }
    void slowUploadUsesIdleTimeoutAB_data() { downloadAB_data(); }
    void slowUploadUsesIdleTimeoutAB() {
        QFETCH(bool, pasv);
        LocalFtp server;
        server.pasv = pasv;
        server.slowUploadChunkMs = 50;
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        const QByteArray original(4 * 1024 * 1024, pasv ? 'b' : 'a');
        hamdrm::HamDrmHybridFtp client(nullptr, 1200);
        QSignalSpy result(&client, &hamdrm::HamDrmHybridFtp::finished);
        QElapsedTimer elapsed; elapsed.start();
        QVERIFY(client.upload({"127.0.0.1", "user", "dummy", "radio"},
                              "de_TEST-1-image.jpg", original, server.controlServer.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 15000);
        QVERIFY2(result[0][0].toBool(), qPrintable(result[0][2].toString()));
        QCOMPARE(server.uploaded, original);
        QVERIFY(elapsed.elapsed() > 1200);
    }
    void missingUploadConfirmationTimesOutAB_data() { downloadAB_data(); }
    void missingUploadConfirmationTimesOutAB() {
        QFETCH(bool, pasv);
        LocalFtp server;
        server.pasv = pasv;
        server.omitUploadConfirmation = true;
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        hamdrm::HamDrmHybridFtp client(nullptr, 120);
        QSignalSpy result(&client, &hamdrm::HamDrmHybridFtp::finished);
        QElapsedTimer elapsed; elapsed.start();
        QVERIFY(client.upload({"127.0.0.1", "user", "dummy", "radio"},
                              "de_TEST-1-image.jpg", QByteArray("test-payload"),
                              server.controlServer.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 5000);
        QVERIFY(!result[0][0].toBool());
        QVERIFY(result[0][2].toString().contains("timed out"));
        QVERIFY(elapsed.elapsed() >= 800);
        QCOMPARE(server.uploaded, QByteArray("test-payload"));
    }
    void downloadAB_data() {
        QTest::addColumn<bool>("pasv");
        QTest::newRow("A-EPSV") << false;
        QTest::newRow("B-PASV") << true;
    }
    void downloadAB() {
        QFETCH(bool, pasv);
        LocalFtp server;
        server.pasv = pasv;
        server.payload = pasv ? QByteArray("test-file-B-123") : QByteArray("test-file-A");
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        hamdrm::HamDrmHybridFtp client;
        QSignalSpy result(&client, &hamdrm::HamDrmHybridFtp::finished);
        QVERIFY(client.download({"127.0.0.1", "dummy-user", "dummy-password", "radio"},
                                "de_TEST-1-image.jpg", server.controlServer.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 5000);
        QVERIFY(result[0][0].toBool());
        QCOMPARE(result[0][1].toByteArray(), server.payload);
        QCOMPARE(server.commands.contains("PASV"), pasv);
        QVERIFY(server.commands.contains("RETR"));
    }
    void rejectsInvalidAndOversize() {
        hamdrm::HamDrmHybridFtp invalid;
        QVERIFY(!invalid.download({"127.0.0.1", "u", "p", "../secret"}, "de_TEST-1-a.jpg"));
        QVERIFY(!invalid.download({"127.0.0.1", "u", "p", ""}, "../secret.jpg"));
        LocalFtp server;
        server.payload = QByteArray(100, 'x');
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        QSignalSpy result(&invalid, &hamdrm::HamDrmHybridFtp::finished);
        QVERIFY(invalid.download({"127.0.0.1", "u", "p", ""}, "de_TEST-1-a.jpg", server.controlServer.serverPort(), 20));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 5000);
        QVERIFY(!result[0][0].toBool());
        QVERIFY(result[0][1].toByteArray().isEmpty());
    }
    void uploadAB_data() { downloadAB_data(); }
    void uploadAB() {
        QFETCH(bool, pasv);
        LocalFtp server;
        server.pasv = pasv;
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        const QByteArray original = pasv ? QByteArray("synthetic upload B") : QByteArray("synthetic upload A");
        hamdrm::HamDrmHybridFtp client;
        QSignalSpy result(&client, &hamdrm::HamDrmHybridFtp::finished);
        QVERIFY(client.upload({"127.0.0.1", "user", "dummy", "radio"}, "de_TEST-1-image.jpg", original,
                              server.controlServer.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 5000);
        QVERIFY(result[0][0].toBool());
        QCOMPARE(server.uploaded, original);
        QVERIFY(server.commands.contains("STOR"));
    }
    void controllerSavesImageAB_data() { downloadAB_data(); }
    void controllerRejectsReplacedReferenceAtCompletionAB_data() { downloadAB_data(); }
    void controllerRejectsReplacedReferenceAtCompletionAB() {
        QFETCH(bool, pasv);
        LocalFtp server;
        server.pasv = pasv;
        QImage image(16 + int(pasv), 18, QImage::Format_RGB32);
        image.fill(pasv ? Qt::blue : Qt::red);
        QBuffer buffer(&server.payload);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, "PNG"));
        buffer.close();
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        hamdrm::HamDrmControllerConfig config;
        config.hybridFtpPort = server.controlServer.serverPort();
        hamdrm::HamDrmController receiver(config, {});
        const auto reference = hamdrm::encodeHamDrmHybridServer({"127.0.0.1", "user", "dummy", "radio"});
        QVERIFY(reference.ok());
        hamdrm::HamDrmMotObjectMetadata meta;
        meta.filename = pasv ? "de_TEST2-1-b.png" : "de_TEST1-1-a.png";
        meta.transportId = pasv ? 111 : 222;
        const auto object = hamdrm::encodeHamDrmObject(meta, *reference.value, 64);
        QVERIFY(object.ok());
        const auto ingest = [&receiver](const std::vector<std::uint8_t>& group) {
            return receiver.ingestMotGroup(QByteArray(reinterpret_cast<const char*>(group.data()), qsizetype(group.size()))).ok();
        };
        for (const auto& group : object.value->headerGroups) QVERIFY(ingest(group));
        for (const auto& group : object.value->bodyGroups) QVERIFY(ingest(group));
        const QString token = receiver.inbox().front().toMap().value("hybridReferenceToken").toString();
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString path = temp.filePath("saved.png");
        QSignalSpy finished(&receiver, &hamdrm::HamDrmController::hybridDownloadFinished);
        QSignalSpy gallery(&receiver, &hamdrm::HamDrmController::receivedImageReady);
        bool replaced = false;
        connect(&receiver, &hamdrm::HamDrmController::hybridDownloadBusyChanged, &receiver, [&] {
            if (receiver.hybridDownloadBusy() || replaced) return;
            replaced = true;
            QVERIFY(receiver.takeCompletedObject(meta.transportId).ok());
            for (const auto& group : object.value->headerGroups) QVERIFY(ingest(group));
            for (const auto& group : object.value->bodyGroups) QVERIFY(ingest(group));
        });
        QVERIFY(receiver.downloadHybridIfUnchanged(meta.transportId, QUrl::fromLocalFile(path), token));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QVERIFY(replaced);
        QVERIFY(!finished[0][0].toBool());
        QVERIFY(!QFile::exists(path));
        QCOMPARE(gallery.count(), 0);
        const auto replacement = receiver.inbox().front().toMap();
        QVERIFY(replacement.value("hybridReferenceToken").toString() != token);
        QVERIFY(!replacement.value("hybridDownloaded").toBool());
    }
    void controllerSavesImageAB() {
        QFETCH(bool, pasv);
        LocalFtp server;
        server.pasv = pasv;
        QImage image(16 + int(pasv), 18, QImage::Format_RGB32);
        image.fill(pasv ? Qt::blue : Qt::red);
        QBuffer buffer(&server.payload);
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&buffer, "PNG"));
        buffer.close();
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        hamdrm::HamDrmControllerConfig config;
        config.hybridFtpPort = server.controlServer.serverPort();
        hamdrm::HamDrmController receiver(config, {});
        const auto reference = hamdrm::encodeHamDrmHybridServer({"127.0.0.1", "user", "dummy", "radio"});
        QVERIFY(reference.ok());
        hamdrm::HamDrmMotObjectMetadata meta;
        meta.filename = pasv ? ".de_TEST2-1-b.png" : ".de_TEST1-1-a.png";
        meta.transportId = pasv ? 111 : 222;
        const auto object = hamdrm::encodeHamDrmObject(meta, *reference.value, 64);
        QVERIFY(object.ok());
        const auto ingest = [&receiver](const std::vector<std::uint8_t>& group) {
            return receiver.ingestMotGroup(QByteArray(reinterpret_cast<const char*>(group.data()), qsizetype(group.size()))).ok();
        };
        for (const auto& group : object.value->headerGroups) QVERIFY(ingest(group));
        for (const auto& group : object.value->bodyGroups) QVERIFY(ingest(group));
        const QString oldToken = receiver.inbox().front().toMap().value("hybridReferenceToken").toString();
        QVERIFY(!oldToken.isEmpty());
        QVERIFY(receiver.discardObject(meta.transportId));
        for (const auto& group : object.value->headerGroups) QVERIFY(ingest(group));
        for (const auto& group : object.value->bodyGroups) QVERIFY(ingest(group));
        const QString currentToken = receiver.inbox().front().toMap().value("hybridReferenceToken").toString();
        QVERIFY(!currentToken.isEmpty());
        QVERIFY(currentToken != oldToken);
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString path = temp.filePath("saved.png");
        QSignalSpy finished(&receiver, &hamdrm::HamDrmController::hybridDownloadFinished);
        QSignalSpy gallery(&receiver, &hamdrm::HamDrmController::receivedImageReady);
        QVERIFY(!receiver.downloadHybridIfUnchanged(meta.transportId, QUrl::fromLocalFile(path), oldToken));
        QVERIFY(!receiver.hybridDownloadBusy());
        QVERIFY(!server.controlServer.hasPendingConnections());
        QVERIFY(!receiver.downloadHybridIfUnchanged(meta.transportId, QUrl::fromLocalFile(path), QString()));
        QVERIFY(receiver.downloadHybridIfUnchanged(meta.transportId, QUrl::fromLocalFile(path), currentToken));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QVERIFY(finished[0][0].toBool());
        QFile saved(path);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QCOMPARE(saved.readAll(), server.payload);
        QCOMPARE(server.retrievedName, QByteArray::fromStdString(meta.filename));
        QVERIFY(receiver.inbox().front().toMap().value("hybridDownloaded").toBool());
        QCOMPARE(gallery.count(), 1);
        const auto snapshot = qvariant_cast<QImage>(gallery[0][0]);
        QCOMPARE(snapshot.size(), image.size());
        QCOMPARE(snapshot.pixelColor(0, 0), image.pixelColor(0, 0));
        QVERIFY(gallery[0][2].toBool());
        const QString secondPath = temp.filePath("second.png");
        QVERIFY(receiver.downloadHybridIfUnchanged(meta.transportId, QUrl::fromLocalFile(secondPath), currentToken));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 2, 5000);
        QVERIFY(finished[1][0].toBool());
        QCOMPARE(gallery.count(), 1);
        QFile second(secondPath);
        QVERIFY(second.open(QIODevice::ReadOnly));
        QCOMPARE(second.readAll(), server.payload);
    }
    void controllerUploadsBeforeRadioAB_data() { downloadAB_data(); }
    void controllerUploadsBeforeRadioAB() {
        QFETCH(bool, pasv);
        LocalFtp server;
        server.pasv = pasv;
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString path = temp.filePath(pasv ? "b.png" : "a.png");
        QImage image(16 + int(pasv), 18, QImage::Format_RGB32);
        image.fill(pasv ? Qt::blue : Qt::red);
        QVERIFY(image.save(path, "PNG"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto original = file.readAll();
        hamdrm::HamDrmControllerConfig config;
        config.hybridFtpPort = server.controlServer.serverPort();
        auto tx = std::make_shared<RecordingTx>();
        hamdrm::HamDrmController receiver(config, {{}, tx, {}});
        QVERIFY(receiver.startHybridTx(QUrl::fromLocalFile(path), "TEST1", "127.0.0.1", "user", "dummy", "radio"));
        QCOMPARE(tx->starts, 0);
        QTRY_COMPARE_WITH_TIMEOUT(tx->starts, 1, 5000);
        QCOMPARE(server.uploaded, original);
        QVERIFY(tx->object.metadata.filename.rfind("de_TEST1-1-", 0) == 0);
        hamdrm::HamDrmObjectAssembler assembler(tx->object.metadata.transportId);
        for (const auto& group : tx->object.headerGroups) QVERIFY(assembler.ingest(group.data(), group.size()).ok());
        for (const auto& group : tx->object.bodyGroups) QVERIFY(assembler.ingest(group.data(), group.size()).ok());
        const auto result = assembler.assembledObject();
        QVERIFY(result.ok());
        const auto decoded = hamdrm::decodeHamDrmHybridServer(result.value->originalBytes);
        QVERIFY(decoded.ok());
        QCOMPARE(decoded.value->host, std::string("127.0.0.1"));
    }
    void rejectedUploadDoesNotStartRadio() {
        LocalFtp server;
        server.rejectUpload = true;
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString path = temp.filePath("a.png");
        QImage image(16, 18, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(path, "PNG"));
        hamdrm::HamDrmControllerConfig config;
        config.hybridFtpPort = server.controlServer.serverPort();
        auto tx = std::make_shared<RecordingTx>();
        hamdrm::HamDrmController receiver(config, {{}, tx, {}});
        QVERIFY(receiver.startHybridTx(QUrl::fromLocalFile(path), "TEST1", "127.0.0.1", "user", "dummy", ""));
        QTRY_COMPARE_WITH_TIMEOUT(receiver.txState(), hamdrm::HamDrmController::OperationState::Error, 5000);
        QCOMPARE(tx->starts, 0);
    }
    void cancelledUploadDoesNotStartRadio_data() { downloadAB_data(); }
    void cancelledUploadDoesNotStartRadio() {
        QFETCH(bool, pasv);
        LocalFtp server;
        server.pasv = pasv;
        server.stallLogin = true;
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString path = temp.filePath("a.png");
        QImage image(16, 18, QImage::Format_RGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(path, "PNG"));
        hamdrm::HamDrmControllerConfig config;
        config.hybridFtpPort = server.controlServer.serverPort();
        auto tx = std::make_shared<RecordingTx>();
        hamdrm::HamDrmController controller(config, {{}, tx, {}});
        QVERIFY(controller.startHybridTx(QUrl::fromLocalFile(path), "TEST1", "127.0.0.1", "user", "dummy", ""));
        QTRY_VERIFY_WITH_TIMEOUT(server.commands.contains("USER"), 5000);
        QVERIFY(controller.busy());
        QVERIFY(pasv ? controller.cancelAll() : controller.cancelTx());
        QCOMPARE(controller.txState(), hamdrm::HamDrmController::OperationState::Cancelled);
        QVERIFY(!controller.busy());
        QCOMPARE(tx->starts, 0);
        QVERIFY(!server.commands.contains("STOR"));
    }
    void cancelledDownloadDoesNotSave_data() { downloadAB_data(); }
    void cancelledDownloadDoesNotSave() {
        QFETCH(bool, pasv);
        LocalFtp server;
        server.stallLogin = true;
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        hamdrm::HamDrmControllerConfig config;
        config.hybridFtpPort = server.controlServer.serverPort();
        hamdrm::HamDrmController controller(config, {});
        const auto reference = hamdrm::encodeHamDrmHybridServer({"127.0.0.1", "user", "dummy", ""});
        QVERIFY(reference.ok());
        hamdrm::HamDrmMotObjectMetadata meta;
        meta.filename = "de_TEST-1-a.png"; meta.transportId = 346;
        const auto object = hamdrm::encodeHamDrmObject(meta, *reference.value, 64);
        QVERIFY(object.ok());
        const auto ingest = [&controller](const std::vector<std::uint8_t>& group) {
            return controller.ingestMotGroup(QByteArray(reinterpret_cast<const char*>(group.data()), qsizetype(group.size()))).ok();
        };
        for (const auto& group : object.value->headerGroups) QVERIFY(ingest(group));
        for (const auto& group : object.value->bodyGroups) QVERIFY(ingest(group));
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString path = temp.filePath("saved.png");
        QSignalSpy finished(&controller, &hamdrm::HamDrmController::hybridDownloadFinished);
        QVERIFY(controller.downloadHybrid(meta.transportId, QUrl::fromLocalFile(path)));
        QTRY_VERIFY_WITH_TIMEOUT(server.commands.contains("USER"), 5000);
        QVERIFY(controller.hybridDownloadBusy());
        QVERIFY(pasv ? controller.discardObject(meta.transportId) : controller.cancelAll());
        QVERIFY(!controller.hybridDownloadBusy());
        QCOMPARE(finished.count(), 1);
        QVERIFY(!finished[0][0].toBool());
        QVERIFY(!QFile::exists(path));
        QVERIFY(!server.commands.contains("RETR"));
    }
    void rejectsLoginWithoutLeakingCredentials() {
        LocalFtp server;
        server.rejectLogin = true;
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        hamdrm::HamDrmHybridFtp client;
        QSignalSpy result(&client, &hamdrm::HamDrmHybridFtp::finished);
        QVERIFY(client.download({"127.0.0.1", "u", "dummy-secret", ""}, "de_TEST-1-a.jpg", server.controlServer.serverPort()));
        QTRY_COMPARE_WITH_TIMEOUT(result.count(), 1, 5000);
        QVERIFY(!result[0][0].toBool());
        QVERIFY(!result[0][2].toString().contains("dummy-secret"));
        QVERIFY(!server.commands.contains("RETR"));
    }
    void invalidImageDoesNotOverwriteDestination() {
        LocalFtp server;
        server.payload = QByteArray("not a PNG");
        QVERIFY(server.controlServer.listen(QHostAddress::LocalHost, 0));
        hamdrm::HamDrmControllerConfig config;
        config.hybridFtpPort = server.controlServer.serverPort();
        hamdrm::HamDrmController receiver(config, {});
        const auto reference = hamdrm::encodeHamDrmHybridServer({"127.0.0.1", "u", "dummy", ""});
        QVERIFY(reference.ok());
        hamdrm::HamDrmMotObjectMetadata meta;
        meta.filename = "de_TEST-1-bad.png"; meta.transportId = 345;
        const auto object = hamdrm::encodeHamDrmObject(meta, *reference.value, 64);
        QVERIFY(object.ok());
        const auto ingest = [&receiver](const std::vector<std::uint8_t>& group) {
            return receiver.ingestMotGroup(QByteArray(reinterpret_cast<const char*>(group.data()), qsizetype(group.size()))).ok();
        };
        for (const auto& group : object.value->headerGroups) QVERIFY(ingest(group));
        for (const auto& group : object.value->bodyGroups) QVERIFY(ingest(group));
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString path = temp.filePath("existing.png");
        QFile existing(path);
        QVERIFY(existing.open(QIODevice::WriteOnly));
        QCOMPARE(existing.write("keep"), qint64(4));
        existing.close();
        QSignalSpy finished(&receiver, &hamdrm::HamDrmController::hybridDownloadFinished);
        QVERIFY(receiver.downloadHybrid(meta.transportId, QUrl::fromLocalFile(path)));
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QVERIFY(!finished[0][0].toBool());
        QVERIFY(existing.open(QIODevice::ReadOnly));
        QCOMPARE(existing.readAll(), QByteArray("keep"));
    }
};
QTEST_GUILESS_MAIN(TestHybridFtp)
#include "test_hamdrm_hybrid_ftp.moc"
