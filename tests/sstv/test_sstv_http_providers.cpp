// SPDX-License-Identifier: GPL-3.0-or-later

#include "src/sstv/sharing/SstvHttpShareProviders.h"
#include "src/sstv/sharing/SstvShareTransfer.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <functional>
#include <iostream>
#include <memory>
#include <limits>
#include <utility>
#include <vector>

#ifndef DECODIUM_SSTV_ALLOW_INSECURE_LOCAL_TEST_TRANSPORT
#define DECODIUM_SSTV_ALLOW_INSECURE_LOCAL_TEST_TRANSPORT 0
#endif

using namespace decodium::sstv::sharing;

namespace {

int g_checks = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        ++g_checks;                                                             \
        if (!(condition)) {                                                     \
            std::cerr << __func__ << ':' << __LINE__                            \
                      << ": check failed: " #condition << '\n';                \
            return false;                                                       \
        }                                                                       \
    } while (false)

struct HttpRequest final
{
    QByteArray method;
    QByteArray target;
    QHash<QByteArray, QByteArray> headers;
    QByteArray body;
};

struct HttpResponse final
{
    HttpResponse() = default;
    explicit HttpResponse(int suppliedStatus)
        : status(suppliedStatus)
    {
    }
    HttpResponse(int suppliedStatus,
                 QByteArray suppliedReason,
                 QList<QPair<QByteArray, QByteArray>> suppliedHeaders,
                 QByteArray suppliedBody = {},
                 int suppliedDelayMs = 0,
                 bool suppliedStall = false)
        : status(suppliedStatus)
        , reason(std::move(suppliedReason))
        , headers(std::move(suppliedHeaders))
        , body(std::move(suppliedBody))
        , delayMs(suppliedDelayMs)
        , stall(suppliedStall)
    {
    }

    int status {200};
    QByteArray reason {"OK"};
    QList<QPair<QByteArray, QByteArray>> headers;
    QByteArray body;
    int delayMs {0};
    bool stall {false};
};

class ScopedHttpServer final
{
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    explicit ScopedHttpServer(Handler handler)
        : m_handler(std::move(handler))
    {
        QObject::connect(&m_server, &QTcpServer::newConnection, &m_server,
                         [this] { acceptConnections(); });
        m_listening = m_server.listen(QHostAddress::LocalHost, 0);
    }

    bool isListening() const noexcept { return m_listening; }

    QUrl url(const QString& path = QStringLiteral("/")) const
    {
        QUrl value;
        value.setScheme(QStringLiteral("http"));
        value.setHost(QStringLiteral("127.0.0.1"));
        value.setPort(static_cast<int>(m_server.serverPort()));
        value.setPath(path);
        return value;
    }

    const QVector<HttpRequest>& requests() const noexcept { return m_requests; }

private:
    static QByteArray reasonForStatus(int status)
    {
        switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 206: return "Partial Content";
        case 207: return "Multi-Status";
        case 307: return "Temporary Redirect";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 429: return "Too Many Requests";
        case 503: return "Service Unavailable";
        default: return "Test Status";
        }
    }

    void acceptConnections()
    {
        while (QTcpSocket* socket = m_server.nextPendingConnection()) {
            m_buffers.insert(socket, {});
            QObject::connect(socket, &QTcpSocket::readyRead, socket,
                             [this, socket] { readRequest(socket); });
            QObject::connect(socket, &QTcpSocket::disconnected, socket,
                             &QObject::deleteLater);
        }
    }

    void readRequest(QTcpSocket* socket)
    {
        auto buffer = m_buffers.find(socket);
        if (buffer == m_buffers.end()) {
            return;
        }
        *buffer += socket->readAll();
        const qsizetype headerEnd = buffer->indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return;
        }
        const QList<QByteArray> lines = buffer->left(headerEnd).split('\n');
        if (lines.isEmpty()) {
            socket->disconnectFromHost();
            return;
        }
        const QList<QByteArray> first = lines.front().trimmed().split(' ');
        if (first.size() < 2) {
            socket->disconnectFromHost();
            return;
        }
        HttpRequest request;
        request.method = first.at(0);
        request.target = first.at(1);
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const QByteArray line = lines.at(i).trimmed();
            const qsizetype colon = line.indexOf(':');
            if (colon <= 0) {
                continue;
            }
            request.headers.insert(line.left(colon).trimmed().toLower(),
                                   line.mid(colon + 1).trimmed());
        }
        bool lengthOk = false;
        const qlonglong contentLength = request.headers.value(
            "content-length").toLongLong(&lengthOk);
        const qsizetype expectedBody = lengthOk && contentLength >= 0
            ? static_cast<qsizetype>(contentLength) : 0;
        const qsizetype bodyStart = headerEnd + 4;
        if (buffer->size() - bodyStart < expectedBody) {
            return;
        }
        request.body = buffer->mid(bodyStart, expectedBody);
        m_buffers.remove(socket);
        m_requests.push_back(request);
        const HttpResponse response = m_handler
            ? m_handler(request) : HttpResponse {};
        if (response.stall) {
            return;
        }
        const QPointer<QTcpSocket> guardedSocket(socket);
        QTimer::singleShot(std::max(0, response.delayMs), socket,
                           [guardedSocket, request, response] {
            if (!guardedSocket) {
                return;
            }
            QByteArray wire = "HTTP/1.1 " + QByteArray::number(response.status)
                + ' ' + (response.reason.isEmpty()
                             ? reasonForStatus(response.status)
                             : response.reason) + "\r\n";
            bool hasContentLength = false;
            bool hasConnection = false;
            for (const auto& header : response.headers) {
                wire += header.first + ": " + header.second + "\r\n";
                hasContentLength = hasContentLength
                    || header.first.compare("content-length",
                                            Qt::CaseInsensitive) == 0;
                hasConnection = hasConnection
                    || header.first.compare("connection",
                                            Qt::CaseInsensitive) == 0;
            }
            if (!hasContentLength) {
                wire += "Content-Length: "
                    + QByteArray::number(response.body.size()) + "\r\n";
            }
            if (!hasConnection) {
                wire += "Connection: close\r\n";
            }
            wire += "\r\n";
            if (request.method != "HEAD") {
                wire += response.body;
            }
            guardedSocket->write(wire);
            guardedSocket->disconnectFromHost();
        });
    }

    QTcpServer m_server;
    Handler m_handler;
    QHash<QTcpSocket*, QByteArray> m_buffers;
    QVector<HttpRequest> m_requests;
    bool m_listening {false};
};

class PlaintextTlsTrap final
{
public:
    PlaintextTlsTrap()
    {
        QObject::connect(&m_server, &QTcpServer::newConnection, &m_server,
                         [this] {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                ++m_connections;
                QObject::connect(socket, &QTcpSocket::readyRead, socket,
                                 [this, socket] {
                    m_received += socket->readAll();
                    if (m_replied) {
                        return;
                    }
                    m_replied = true;
                    socket->write(
                        "HTTP/1.1 400 Bad Request\r\n"
                        "Content-Length: 0\r\nConnection: close\r\n\r\n");
                    socket->disconnectFromHost();
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket,
                                 &QObject::deleteLater);
            }
        });
        m_listening = m_server.listen(QHostAddress::LocalHost, 0);
    }

    bool isListening() const noexcept { return m_listening; }
    int connections() const noexcept { return m_connections; }
    QByteArray received() const { return m_received; }

    QUrl url() const
    {
        QUrl value;
        value.setScheme(QStringLiteral("https"));
        value.setHost(QStringLiteral("127.0.0.1"));
        value.setPort(static_cast<int>(m_server.serverPort()));
        value.setPath(QStringLiteral("/api/uploads"));
        return value;
    }

private:
    QTcpServer m_server;
    QByteArray m_received;
    int m_connections {0};
    bool m_listening {false};
    bool m_replied {false};
};

class TestBearerLease final : public SstvShareCredentialLease
{
public:
    explicit TestBearerLease(QByteArray token) : m_token(std::move(token)) {}
    ~TestBearerLease() override
    {
        m_token.fill('\0');
    }
    bool applyTo(QNetworkRequest& request) const override
    {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + m_token);
        return true;
    }

private:
    mutable QByteArray m_token;
};

class TestCredentialSource final : public SstvShareCredentialSource
{
public:
    explicit TestCredentialSource(QByteArray token) : m_token(std::move(token)) {}
    ~TestCredentialSource() override { m_token.fill('\0'); }

    SstvShareAuthenticationStatus status() const noexcept override
    {
        return SstvShareAuthenticationStatus::Authenticated;
    }
    std::shared_ptr<const SstvShareCredentialLease> acquireLease(
        const QString&, SstvShareCredentialPurpose) override
    {
        return std::make_shared<TestBearerLease>(m_token);
    }

private:
    QByteArray m_token;
};

struct TestPresignedLeaseCounters final
{
    int acquired {0};
    int released {0};
    int live {0};
};

class TestPresignedLease final : public SstvSharePresignedTargetLease
{
public:
    TestPresignedLease(
        QUrl url, std::shared_ptr<TestPresignedLeaseCounters> counters)
        : m_url(std::move(url))
        , m_counters(std::move(counters))
    {
        ++m_counters->acquired;
        ++m_counters->live;
    }
    ~TestPresignedLease() override
    {
        m_url = QUrl();
        ++m_counters->released;
        --m_counters->live;
    }
    QUrl targetUrl() const override { return m_url; }
    bool applyTo(QNetworkRequest& request) const override
    {
        request.setRawHeader("X-Test-Target-Lease", "present");
        return true;
    }

private:
    QUrl m_url;
    std::shared_ptr<TestPresignedLeaseCounters> m_counters;
};

class TestPresignedSource final : public SstvSharePresignedTargetSource
{
public:
    explicit TestPresignedSource(QUrl url)
        : m_url(std::move(url))
        , m_counters(std::make_shared<TestPresignedLeaseCounters>())
    {
    }
    ~TestPresignedSource() override { m_url = QUrl(); }

    SstvShareAuthenticationStatus status() const noexcept override
    {
        return SstvShareAuthenticationStatus::Authenticated;
    }
    std::shared_ptr<const SstvSharePresignedTargetLease> acquireTarget(
        const QString&, const QUuid&, quint64, const QString&) override
    {
        return std::make_shared<TestPresignedLease>(m_url, m_counters);
    }

    int acquiredLeases() const noexcept { return m_counters->acquired; }
    int releasedLeases() const noexcept { return m_counters->released; }
    int liveLeases() const noexcept { return m_counters->live; }

private:
    QUrl m_url;
    std::shared_ptr<TestPresignedLeaseCounters> m_counters;
};

#if defined(DECODIUM_SSTV_PROVIDER_TESTING)
class BoundedTestHttpProvider final : public SstvHttpShareProvider
{
public:
    BoundedTestHttpProvider()
        : SstvHttpShareProvider(
              QStringLiteral("test-http-bound"), testCapabilities(),
              testOptions(), {}, false)
    {
    }

    void forceNextOperationId(SstvShareOperationId operationId)
    {
        setNextOperationIdForTesting(operationId);
    }

    qsizetype pendingCount() const noexcept
    {
        return pendingOperationCountForTesting();
    }

    quint64 reservedBytes() const noexcept
    {
        return pendingReservedBytesForTesting();
    }

    static constexpr qsizetype maximumPending() noexcept
    {
        return maximumPendingOperationsForTesting();
    }

    static constexpr quint64 maximumPendingBytes() noexcept
    {
        return maximumPendingBytesForTesting();
    }

    SstvShareOperationId startStalledUpload(
        const QUrl& url,
        const QByteArray& body,
        qsizetype maximumResponseBytes,
        SstvShareProviderCompletion completion)
    {
        Request request;
        request.method = QByteArrayLiteral("PUT");
        request.url = url;
        request.body = body;
        request.acceptedStatusCodes = {200};
        request.maximumResponseBytes = maximumResponseBytes;
        request.diagnosticLabel = QStringLiteral("bounded test upload");
        return startRequest(std::move(request), std::move(completion));
    }

private:
    static SstvShareProviderCapabilities testCapabilities()
    {
        SstvShareProviderCapabilities value;
        value.strictTlsRequired = true;
        value.maximumResponseBytes = 1024U * 1024U;
        return value;
    }

    static SstvHttpTransportOptions testOptions()
    {
        SstvHttpTransportOptions value;
        value.timeoutMs = 1'000;
        value.maximumResponseBytes = 1024 * 1024;
        value.maximumRedirects = 0;
        value.allowInsecureLocalhostForTests = true;
        return value;
    }
};
#endif

QString sha256Hex(const QByteArray& bytes)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

QByteArray sha256Base64(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes,
                                    QCryptographicHash::Sha256).toBase64();
}

QByteArray payload(qsizetype size = 2'048)
{
    QByteArray value(size, Qt::Uninitialized);
    quint32 state = 0x7481f2d3U;
    for (qsizetype i = 0; i < value.size(); ++i) {
        state ^= state << 13U;
        state ^= state >> 17U;
        state ^= state << 5U;
        value[i] = static_cast<char>(state & 0xffU);
    }
    return value;
}

SstvShareManifestV1 manifestFor(const QString& providerId,
                                const QByteArray& bytes,
                                quint32 chunkCount = 1U)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    SstvShareManifestV1 manifest;
    manifest.transferId = QUuid::createUuid();
    manifest.providerId = providerId;
    manifest.senderId = QStringLiteral("station:9h1abc");
    manifest.recipientId = QStringLiteral("recipient:9h1xyz");
    manifest.createdUtc = now.addSecs(-5);
    manifest.expiresUtc = now.addDays(1);
    manifest.mediaUtc = now.addSecs(-10);
    manifest.originalFilename = QStringLiteral("sstv-test.png");
    manifest.safeDisplayFilename = QStringLiteral("SSTV test.png");
    manifest.mimeType = QStringLiteral("image/png");
    manifest.byteSize = static_cast<quint64>(bytes.size());
    manifest.sha256 = sha256Hex(bytes);
    manifest.width = 320U;
    manifest.height = 256U;
    manifest.sstvMode = QStringLiteral("Martin M1");
    manifest.chunkCount = chunkCount;
    manifest.privacy.recipientConfirmed = true;
    return manifest;
}

SstvGenericRestProviderConfig restConfig(const ScopedHttpServer& server,
                                         const QString& providerId)
{
    SstvGenericRestProviderConfig config;
    config.providerId = providerId;
    config.baseUrl = server.url();
    config.createUploadPath = QStringLiteral("/api/uploads");
    config.uploadChunkPathTemplate =
        QStringLiteral("/api/uploads/{uploadId}/content");
    config.queryStatusPathTemplate =
        QStringLiteral("/api/uploads/{uploadId}/status");
    config.completeUploadPathTemplate =
        QStringLiteral("/api/uploads/{uploadId}/complete");
    config.cancelUploadPathTemplate =
        QStringLiteral("/api/uploads/{uploadId}");
    config.credentialsRequired = false;
    config.maximumChunkBytes = kMaximumSharedImageBytes;
    config.transport.timeoutMs = 750;
    config.transport.maximumResponseBytes = 4'096;
    config.transport.maximumRedirects = 2;
    config.transport.allowInsecureLocalhostForTests = true;
    return config;
}

struct Awaited final
{
    SstvShareProviderResult result;
    SstvShareOperationId operationId {0U};
    bool completed {false};
    bool timedOut {false};
};

Awaited awaitResult(
    const std::function<SstvShareOperationId(SstvShareProviderCompletion)>& start,
    int maximumWaitMs = 3'000)
{
    Awaited awaited;
    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, &loop, [&] {
        awaited.timedOut = true;
        loop.quit();
    });
    awaited.operationId = start([&](SstvShareProviderResult result) {
        awaited.result = std::move(result);
        awaited.completed = true;
        loop.quit();
    });
    if (!awaited.completed) {
        guard.start(maximumWaitMs);
        loop.exec();
    }
    return awaited;
}

void processEventsFor(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

struct AwaitedRecipient final
{
    SstvShareProviderResult result;
    SstvShareRecipientRecord recipient;
    bool completed {false};
};

AwaitedRecipient awaitRecipient(const std::function<SstvShareOperationId(
    SstvShareRecipientCompletion)>& start)
{
    AwaitedRecipient awaited;
    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    start([&](SstvShareProviderResult result,
              SstvShareRecipientRecord recipient) {
        awaited.result = std::move(result);
        awaited.recipient = std::move(recipient);
        awaited.completed = true;
        loop.quit();
    });
    if (!awaited.completed) {
        guard.start(3'000);
        loop.exec();
    }
    return awaited;
}

struct AwaitedIncoming final
{
    SstvShareProviderResult result;
    QVector<SstvShareIncomingItem> items;
    bool completed {false};
};

AwaitedIncoming awaitIncoming(const std::function<SstvShareOperationId(
    SstvShareIncomingCompletion)>& start)
{
    AwaitedIncoming awaited;
    QEventLoop loop;
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    start([&](SstvShareProviderResult result,
              QVector<SstvShareIncomingItem> items) {
        awaited.result = std::move(result);
        awaited.items = std::move(items);
        awaited.completed = true;
        loop.quit();
    });
    if (!awaited.completed) {
        guard.start(3'000);
        loop.exec();
    }
    return awaited;
}

QByteArray compactJson(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QByteArray fullCapabilitiesJson(quint64 maximumResponseBytes = 4'096U,
                                bool revocation = false,
                                bool remoteDelete = false)
{
    return compactJson({
        {QStringLiteral("protocolVersion"), 1},
        {QStringLiteral("recipientLookup"), true},
        {QStringLiteral("chunkedUpload"), true},
        {QStringLiteral("resumableUpload"), true},
        {QStringLiteral("download"), true},
        {QStringLiteral("acknowledgement"), true},
        {QStringLiteral("rejection"), true},
        {QStringLiteral("revocation"), revocation},
        {QStringLiteral("remoteDelete"), remoteDelete},
        {QStringLiteral("incomingList"), true},
        {QStringLiteral("endToEndEncryptionEnvelope"), false},
        {QStringLiteral("strictTlsRequired"), true},
        {QStringLiteral("maximumChunkBytes"), 4'096},
        {QStringLiteral("maximumResponseBytes"),
         static_cast<double>(maximumResponseBytes)},
    });
}

QByteArray inboxJson(const QString& incomingId,
                     const SstvShareManifestV1& manifest,
                     const QDateTime& receivedUtc)
{
    const QByteArray canonical = manifest.toCanonicalJson();
    const QJsonObject item {
        {QStringLiteral("incomingId"), incomingId},
        {QStringLiteral("providerId"), manifest.providerId},
        {QStringLiteral("senderId"), manifest.senderId},
        {QStringLiteral("manifestSha256"), sha256Hex(canonical)},
        {QStringLiteral("canonicalManifestJson"),
         QString::fromUtf8(canonical)},
        {QStringLiteral("byteSize"),
         static_cast<double>(manifest.byteSize)},
        {QStringLiteral("receivedUtc"),
         receivedUtc.toUTC().toString(Qt::ISODateWithMs)},
        {QStringLiteral("expiresUtc"),
         manifest.expiresUtc.toUTC().toString(Qt::ISODateWithMs)},
    };
    return compactJson({
        {QStringLiteral("items"), QJsonArray {item}},
    });
}

bool restHappyPathIsAsyncBoundedAndIdempotent()
{
    const QByteArray bytes = payload();
    quint64 committed = 0U;
    QByteArray received;
    ScopedHttpServer server([&](const HttpRequest& request) {
        if (request.method == "POST" && request.target == "/api/uploads") {
            return HttpResponse {201, {}, {{"Content-Type", "application/json"}},
                                 "{\"uploadId\":\"remote-1\",\"committedBytes\":0}"};
        }
        if (request.method == "PUT"
            && request.target == "/api/uploads/remote-1/content") {
            received = request.body;
            committed += static_cast<quint64>(request.body.size());
            return HttpResponse {200, {}, {{"Content-Type", "application/json"}},
                QByteArray("{\"committedBytes\":")
                    + QByteArray::number(committed) + '}'};
        }
        if (request.method == "GET"
            && request.target == "/api/uploads/remote-1/status") {
            return HttpResponse {200, {}, {{"Content-Type", "application/json"}},
                QByteArray("{\"committedBytes\":")
                    + QByteArray::number(committed) + '}'};
        }
        if (request.method == "POST"
            && request.target == "/api/uploads/remote-1/complete") {
            return HttpResponse {200, {}, {{"Content-Type", "application/json"}},
                QByteArray("{\"byteSize\":") + QByteArray::number(bytes.size())
                    + ",\"remoteObjectId\":\"object-1\",\"sha256\":\""
                    + sha256Hex(bytes).toLatin1() + "\"}"};
        }
        return HttpResponse {404};
    });
    CHECK(server.isListening());
    const auto manifest = manifestFor(QStringLiteral("rest-test"), bytes);
    const QString key = SstvShareTransfer::deriveIdempotencyKey(manifest);
    const auto config = restConfig(server, manifest.providerId);
    SstvGenericRestShareProvider provider(config, {});
    CHECK(provider.isConfigurationValid());

    const Awaited created = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.createUploadAsync(manifest, key, std::move(done));
    });
    CHECK(created.completed && !created.timedOut && created.result.ok());
    CHECK(created.operationId != 0U);
    CHECK(isSafeShareIdentifier(created.result.handle().opaqueId));
    CHECK(!containsNetworkUrl(created.result.handle().opaqueId));
    const Awaited duplicate = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.createUploadAsync(manifest, key, std::move(done));
    });
    CHECK(duplicate.result.ok());
    CHECK(server.requests().size() == 1);

    // A fresh provider instance can rebuild its in-memory session from the
    // idempotent create response while preserving the persisted opaque handle.
    SstvGenericRestShareProvider restartedProvider(config, {});
    const Awaited recreated = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return restartedProvider.createUploadAsync(
                manifest, key, std::move(done));
        });
    CHECK(recreated.result.ok());
    CHECK(recreated.result.handle().opaqueId
          == created.result.handle().opaqueId);
    CHECK(server.requests().size() == 2);

    quint64 progressBytes = 0U;
    const Awaited uploaded = awaitResult([&](SstvShareProviderCompletion done) {
        return restartedProvider.uploadChunkAsync(
            recreated.result.handle(), 0U, bytes,
            manifest.sha256,
            [&](quint64 sent, quint64) { progressBytes = std::max(progressBytes, sent); },
            std::move(done));
    });
    CHECK(uploaded.result.ok());
    CHECK(uploaded.result.handle().committedBytes
          == static_cast<quint64>(bytes.size()));
    CHECK(received == bytes);
    CHECK(progressBytes == static_cast<quint64>(bytes.size()));

    const Awaited status = awaitResult([&](SstvShareProviderCompletion done) {
        return restartedProvider.queryStatusAsync(
            uploaded.result.handle(), std::move(done));
    });
    CHECK(status.result.ok());
    const Awaited completed = awaitResult([&](SstvShareProviderCompletion done) {
        return restartedProvider.completeUploadAsync(
            status.result.handle(), key, std::move(done));
    });
    CHECK(completed.result.ok());
    CHECK(completed.result.handle().opaqueId == QStringLiteral("object-1"));
    const Awaited completedAgain = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return restartedProvider.completeUploadAsync(
                status.result.handle(), key, std::move(done));
        });
    CHECK(completedAgain.result.ok());
    CHECK(server.requests().size() == 5);
    return true;
}

bool restRemoteRevocationUsesOnlyDocumentedTransferDelete()
{
    QVector<HttpRequest> requests;
    ScopedHttpServer server([&](const HttpRequest& request) {
        requests.push_back(request);
        if (request.method == "GET"
            && request.target == "/api/v1/capabilities") {
            return HttpResponse {200, {},
                {{"Content-Type", "application/json"}},
                fullCapabilitiesJson(4'096U, true, true)};
        }
        if (request.method == "POST"
            && request.target == "/api/uploads") {
            return HttpResponse {201, {},
                {{"Content-Type", "application/json"}},
                "{\"uploadId\":\"remote-77\",\"committedBytes\":0}"};
        }
        if (request.method == "DELETE"
            && request.target == "/api/uploads/remote-77") {
            return HttpResponse {404};
        }
        return HttpResponse {500};
    });
    const QByteArray bytes = payload(96);
    auto config = restConfig(server, QStringLiteral("rest-revoke"));
    config.credentialsRequired = true;
    const auto manifest = manifestFor(config.providerId, bytes);
    const QString key = SstvShareTransfer::deriveIdempotencyKey(manifest);
    auto credentials = std::make_shared<TestCredentialSource>(
        QByteArray("remote-removal-test-token"));
    SstvGenericRestShareProvider provider(config, credentials);

    const Awaited unavailable = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.revokeAsync(
                {QStringLiteral("opaque-before-discovery"), 0U},
                std::move(done));
        });
    CHECK(!unavailable.result.ok());
    CHECK(requests.isEmpty());

    const Awaited discovered = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.refreshCapabilitiesAsync(std::move(done));
        });
    CHECK(discovered.result.ok());
    CHECK(provider.capabilities().revocation);
    CHECK(!provider.capabilities().remoteDelete);
    const Awaited created = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.createUploadAsync(manifest, key, std::move(done));
        });
    CHECK(created.result.ok());
    const Awaited revoked = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.revokeAsync(created.result.handle(),
                                        std::move(done));
        });
    CHECK(revoked.result.ok());
    CHECK(requests.size() == 3);
    CHECK(requests.at(2).method == QByteArrayLiteral("DELETE"));
    CHECK(requests.at(2).target == QByteArrayLiteral(
        "/api/uploads/remote-77"));
    CHECK(requests.at(2).headers.value("idempotency-key")
          == key.toLatin1());

    auto unauthenticatedConfig = config;
    unauthenticatedConfig.credentialsRequired = false;
    SstvGenericRestShareProvider unauthenticated(
        unauthenticatedConfig, {});
    CHECK(awaitResult([&](SstvShareProviderCompletion done) {
        return unauthenticated.refreshCapabilitiesAsync(std::move(done));
    }).result.ok());
    const Awaited unauthenticatedCreated = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return unauthenticated.createUploadAsync(
                manifest, key, std::move(done));
        });
    CHECK(unauthenticatedCreated.result.ok());
    const Awaited unauthenticatedMissing = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return unauthenticated.revokeAsync(
                unauthenticatedCreated.result.handle(), std::move(done));
        });
    CHECK(!unauthenticatedMissing.result.ok());
    CHECK(unauthenticatedMissing.result.category()
          == SstvShareProviderFailure::NotFound);
    CHECK(requests.size() == 6);

    const Awaited noInventedDelete = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.deleteRemoteObjectAsync(
                QStringLiteral("object-77"), std::move(done));
        });
    CHECK(!noInventedDelete.result.ok());
    CHECK(requests.size() == 6);
    return true;
}

bool credentialLeaseIsOpaqueAndDiagnosticsAreRedacted()
{
    const QByteArray secret("THIS_IS_A_SECRET_TOKEN_93");
    bool sawCredential = false;
    ScopedHttpServer server([&](const HttpRequest& request) {
        sawCredential = request.headers.value("authorization")
            == QByteArray("Bearer ") + secret;
        return HttpResponse {401, {}, {{"Content-Type", "application/json"}},
            QByteArray("{\"token\":\"") + secret + "\"}"};
    });
    auto config = restConfig(server, QStringLiteral("rest-auth"));
    config.credentialsRequired = true;
    auto credentials = std::make_shared<TestCredentialSource>(secret);
    SstvGenericRestShareProvider provider(config, credentials);
    const QByteArray bytes = payload(256);
    const auto manifest = manifestFor(config.providerId, bytes);
    const Awaited result = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.createUploadAsync(
            manifest, SstvShareTransfer::deriveIdempotencyKey(manifest),
            std::move(done));
    });
    CHECK(sawCredential);
    CHECK(!result.result.ok());
    CHECK(result.result.category() == SstvShareProviderFailure::Authentication);
    CHECK(!result.result.redactedDiagnostic().contains(
        QString::fromLatin1(secret)));
    CHECK(!result.result.redactedDiagnostic().contains(QStringLiteral("/api/")));
    CHECK(!result.result.redactedDiagnostic().contains(QStringLiteral("token"),
                                                        Qt::CaseInsensitive));
    return true;
}

bool crossOriginRedirectIsRejectedWithoutCredentialForwarding()
{
    int targetRequests = 0;
    bool sourceCredentialSeen = false;
    ScopedHttpServer target([&](const HttpRequest&) {
        ++targetRequests;
        return HttpResponse {};
    });
    ScopedHttpServer source([&](const HttpRequest& request) {
        sourceCredentialSeen = request.headers.value("authorization")
            == "Bearer redirect-secret";
        return HttpResponse {307, {},
            {{"Location", target.url(QStringLiteral("/capture")).toEncoded()}}, {}};
    });
    auto config = restConfig(source, QStringLiteral("rest-redirect"));
    config.credentialsRequired = true;
    auto credentials = std::make_shared<TestCredentialSource>(
        QByteArray("redirect-secret"));
    SstvGenericRestShareProvider provider(config, credentials);
    const QByteArray bytes = payload(128);
    const auto manifest = manifestFor(config.providerId, bytes);
    const Awaited result = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.createUploadAsync(
            manifest, SstvShareTransfer::deriveIdempotencyKey(manifest),
            std::move(done));
    });
    CHECK(!result.result.ok());
    CHECK(result.result.category()
          == SstvShareProviderFailure::PermanentProviderFailure);
    CHECK(sourceCredentialSeen);
    CHECK(targetRequests == 0);
    CHECK(source.requests().size() == 1);
    return true;
}

bool tlsHandshakeFailureIsFailClosedExactlyOnceWithoutCredentialLeak()
{
    PlaintextTlsTrap trap;
    CHECK(trap.isListening());
    const QByteArray secret("tls-client-secret");
    auto credentials = std::make_shared<TestCredentialSource>(secret);
    SstvGenericRestProviderConfig config;
    config.providerId = QStringLiteral("rest-tls-failure");
    QUrl baseUrl = trap.url();
    baseUrl.setPath(QStringLiteral("/"));
    config.baseUrl = baseUrl;
    config.createUploadPath = QStringLiteral("/api/uploads");
    config.uploadChunkPathTemplate =
        QStringLiteral("/api/uploads/{uploadId}/content");
    config.queryStatusPathTemplate =
        QStringLiteral("/api/uploads/{uploadId}/status");
    config.completeUploadPathTemplate =
        QStringLiteral("/api/uploads/{uploadId}/complete");
    config.cancelUploadPathTemplate =
        QStringLiteral("/api/uploads/{uploadId}");
    config.credentialsRequired = true;
    config.maximumChunkBytes = kMaximumSharedImageBytes;
    config.transport.timeoutMs = 1'000;
    config.transport.maximumResponseBytes = 4'096;
    config.transport.maximumRedirects = 0;
    SstvGenericRestShareProvider provider(config, credentials);
    const QByteArray bytes = payload(128);
    const auto manifest = manifestFor(config.providerId, bytes);

    SstvShareProviderResult completedResult;
    int completions = 0;
    QEventLoop loop;
    const SstvShareOperationId operation = provider.createUploadAsync(
        manifest, SstvShareTransfer::deriveIdempotencyKey(manifest),
        [&](SstvShareProviderResult result) {
            ++completions;
            completedResult = std::move(result);
            loop.quit();
        });
    CHECK(operation != 0U);
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    guard.start(2'000);
    loop.exec();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    CHECK(completions == 1);
    CHECK(completedResult.category()
          == SstvShareProviderFailure::TlsValidation);
    CHECK(trap.connections() == 1);
    CHECK(!trap.received().isEmpty());
    CHECK(!trap.received().contains(secret));
    CHECK(!completedResult.redactedDiagnostic().contains(
        QString::fromUtf8(secret)));
    return true;
}

bool timeoutAndCancelCompleteExactlyOnce()
{
    ScopedHttpServer server([](const HttpRequest&) {
        HttpResponse response;
        response.stall = true;
        return response;
    });
    auto timeoutConfig = restConfig(server, QStringLiteral("rest-timeout"));
    timeoutConfig.transport.timeoutMs = 120;
    SstvGenericRestShareProvider timeoutProvider(timeoutConfig, {});
    const QByteArray bytes = payload(128);
    const auto timeoutManifest = manifestFor(timeoutConfig.providerId, bytes);
    const Awaited timedOut = awaitResult([&](SstvShareProviderCompletion done) {
        return timeoutProvider.createUploadAsync(
            timeoutManifest,
            SstvShareTransfer::deriveIdempotencyKey(timeoutManifest),
            std::move(done));
    });
    CHECK(timedOut.completed && !timedOut.timedOut);
    CHECK(timedOut.result.category()
          == SstvShareProviderFailure::TransientNetwork);
    CHECK(isRetryableShareProviderFailure(timedOut.result.category()));

    auto cancelConfig = restConfig(server, QStringLiteral("rest-cancel-op"));
    cancelConfig.transport.timeoutMs = 2'000;
    SstvGenericRestShareProvider cancelProvider(cancelConfig, {});
    const auto cancelManifest = manifestFor(cancelConfig.providerId, bytes);
    Awaited cancelled;
    int completions = 0;
    bool cancellationIssued = false;
    QEventLoop loop;
    cancelled.operationId = cancelProvider.createUploadAsync(
        cancelManifest, SstvShareTransfer::deriveIdempotencyKey(cancelManifest),
        [&](SstvShareProviderResult result) {
            ++completions;
            cancelled.result = std::move(result);
            cancelled.completed = true;
            loop.quit();
        });
    QTimer::singleShot(20, &loop, [&] {
        cancellationIssued = cancelProvider.cancelOperation(cancelled.operationId);
    });
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    guard.start(1'000);
    loop.exec();
    QCoreApplication::processEvents();
    CHECK(cancelled.completed);
    CHECK(cancellationIssued);
    CHECK(cancelled.result.category() == SstvShareProviderFailure::Cancelled);
    CHECK(completions == 1);
    return true;
}

bool pendingCapacityAndCancellationAreBounded()
{
#if defined(DECODIUM_SSTV_PROVIDER_TESTING)
    BoundedTestHttpProvider provider;
    QVector<SstvShareOperationId> operationIds;
    operationIds.reserve(BoundedTestHttpProvider::maximumPending());
    QSet<SstvShareOperationId> uniqueIds;
    int completions = 0;
    for (qsizetype index = 0;
         index < BoundedTestHttpProvider::maximumPending(); ++index) {
        const SstvShareOperationId operationId =
            provider.refreshCredentialsAsync(
                [&completions](SstvShareProviderResult) { ++completions; });
        CHECK(operationId != 0U);
        CHECK(!uniqueIds.contains(operationId));
        uniqueIds.insert(operationId);
        operationIds.push_back(operationId);
    }
    CHECK(provider.pendingCount()
          == BoundedTestHttpProvider::maximumPending());

    bool overflowCompleted = false;
    const SstvShareOperationId overflow = provider.refreshCredentialsAsync(
        [&overflowCompleted](SstvShareProviderResult) {
            overflowCompleted = true;
        });
    CHECK(overflow == 0U);
    CHECK(!overflowCompleted);
    CHECK(provider.pendingCount()
          == BoundedTestHttpProvider::maximumPending());

    for (const SstvShareOperationId operationId : operationIds) {
        CHECK(provider.cancelOperation(operationId));
    }
    QElapsedTimer timer;
    timer.start();
    while (provider.pendingCount() != 0 && timer.elapsed() < 1'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    CHECK(provider.pendingCount() == 0);
    CHECK(completions == BoundedTestHttpProvider::maximumPending());

    const Awaited afterReclaim = awaitResult(
        [&provider](SstvShareProviderCompletion done) {
            return provider.refreshCredentialsAsync(std::move(done));
        });
    CHECK(afterReclaim.operationId != 0U);
    CHECK(afterReclaim.completed && afterReclaim.result.ok());
    return true;
#else
    return false;
#endif
}

bool operationIdExhaustionFailsClosedWithoutCollision()
{
#if defined(DECODIUM_SSTV_PROVIDER_TESTING)
    BoundedTestHttpProvider provider;
    provider.forceNextOperationId(
        std::numeric_limits<SstvShareOperationId>::max());
    int completions = 0;
    const SstvShareOperationId last = provider.refreshCredentialsAsync(
        [&completions](SstvShareProviderResult) { ++completions; });
    CHECK(last == std::numeric_limits<SstvShareOperationId>::max());
    const SstvShareOperationId exhausted = provider.refreshCredentialsAsync(
        [&completions](SstvShareProviderResult) { ++completions; });
    CHECK(exhausted == 0U);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    CHECK(completions == 1);
    CHECK(provider.pendingCount() == 0);

    const SstvShareOperationId remainsExhausted =
        provider.refreshCredentialsAsync(
            [&completions](SstvShareProviderResult) { ++completions; });
    CHECK(remainsExhausted == 0U);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    CHECK(completions == 1);
    return true;
#else
    return false;
#endif
}

bool aggregatePendingBodyAndResponseBudgetIsReclaimed()
{
#if defined(DECODIUM_SSTV_PROVIDER_TESTING)
    ScopedHttpServer server([](const HttpRequest&) {
        HttpResponse response;
        response.stall = true;
        return response;
    });
    CHECK(server.isListening());
    BoundedTestHttpProvider provider;
    constexpr qsizetype bodyBytes = 32 * 1024 * 1024;
    constexpr qsizetype responseBytes = 1024 * 1024;
    const quint64 perOperation = static_cast<quint64>(
        bodyBytes + responseBytes);
    const QByteArray body(bodyBytes, 'p');
    QVector<SstvShareOperationId> operations;
    int completions = 0;
    for (int index = 0; index < 3; ++index) {
        const SstvShareOperationId operation = provider.startStalledUpload(
            server.url(QStringLiteral("/bounded")), body, responseBytes,
            [&completions](SstvShareProviderResult) { ++completions; });
        CHECK(operation != 0U);
        operations.push_back(operation);
    }
    CHECK(provider.pendingCount() == 3);
    CHECK(provider.reservedBytes() == 3U * perOperation);
    CHECK(provider.reservedBytes()
          <= BoundedTestHttpProvider::maximumPendingBytes());

    bool rejectedCompleted = false;
    const SstvShareOperationId rejected = provider.startStalledUpload(
        server.url(QStringLiteral("/bounded")), body, responseBytes,
        [&rejectedCompleted](SstvShareProviderResult) {
            rejectedCompleted = true;
        });
    CHECK(rejected == 0U);
    CHECK(!rejectedCompleted);
    CHECK(provider.pendingCount() == 3);
    CHECK(provider.reservedBytes() == 3U * perOperation);

    CHECK(provider.cancelOperation(operations.constFirst()));
    QElapsedTimer timer;
    timer.start();
    while (provider.pendingCount() != 2 && timer.elapsed() < 1'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    CHECK(provider.pendingCount() == 2);
    CHECK(provider.reservedBytes() == 2U * perOperation);

    const SstvShareOperationId afterReclaim = provider.startStalledUpload(
        server.url(QStringLiteral("/bounded")), body, responseBytes,
        [&completions](SstvShareProviderResult) { ++completions; });
    CHECK(afterReclaim != 0U);
    operations.push_back(afterReclaim);
    CHECK(provider.reservedBytes() == 3U * perOperation);

    for (const SstvShareOperationId operation : operations) {
        provider.cancelOperation(operation);
    }
    timer.restart();
    while (provider.pendingCount() != 0 && timer.elapsed() < 1'000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }
    CHECK(provider.pendingCount() == 0);
    CHECK(provider.reservedBytes() == 0U);
    CHECK(completions == 4);
    return true;
#else
    return false;
#endif
}

bool oversizedAndMalformedResponsesAreRejected()
{
    int responseMode = 0;
    ScopedHttpServer server([&](const HttpRequest&) {
        if (responseMode == 1) {
            return HttpResponse {200, {}, {{"Content-Type", "application/json"}},
                                 "{broken"};
        }
        if (responseMode == 2) {
            return HttpResponse {200, {}, {{"Content-Type", "text/plain"}},
                                 "{\"uploadId\":\"remote-1\"}"};
        }
        return HttpResponse {200, {}, {{"Content-Type", "application/json"}},
                             QByteArray(1'024, 'x')};
    });
    auto config = restConfig(server, QStringLiteral("rest-hostile"));
    config.transport.maximumResponseBytes = 128;
    SstvGenericRestShareProvider provider(config, {});
    const QByteArray bytes = payload(64);
    auto manifest = manifestFor(config.providerId, bytes);
    Awaited result = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.createUploadAsync(
            manifest, SstvShareTransfer::deriveIdempotencyKey(manifest),
            std::move(done));
    });
    CHECK(!result.result.ok());
    CHECK(result.result.category() == SstvShareProviderFailure::Validation);
    CHECK(result.result.redactedDiagnostic().contains(QStringLiteral("bound")));

    responseMode = 1;
    manifest.transferId = QUuid::createUuid();
    result = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.createUploadAsync(
            manifest, SstvShareTransfer::deriveIdempotencyKey(manifest),
            std::move(done));
    });
    CHECK(!result.result.ok());
    CHECK(result.result.category() == SstvShareProviderFailure::Validation);
    CHECK(result.result.redactedDiagnostic().contains(QStringLiteral("malformed")));
    responseMode = 2;
    manifest.transferId = QUuid::createUuid();
    result = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.createUploadAsync(
            manifest, SstvShareTransfer::deriveIdempotencyKey(manifest),
            std::move(done));
    });
    CHECK(!result.result.ok());
    CHECK(result.result.category() == SstvShareProviderFailure::Validation);
    CHECK(result.result.redactedDiagnostic().contains(
        QStringLiteral("content type")));
    return true;
}

bool statusClassificationAndRetryAfterAreExact()
{
    int responseStatus = 503;
    QByteArray retryAfter("3");
    ScopedHttpServer server([&](const HttpRequest&) {
        HttpResponse response;
        response.status = responseStatus;
        response.headers = {{"Retry-After", retryAfter}};
        return response;
    });
    auto config = restConfig(server, QStringLiteral("rest-status"));
    SstvGenericRestShareProvider provider(config, {});
    const QByteArray bytes = payload(64);
    auto manifest = manifestFor(config.providerId, bytes);
    auto run = [&] {
        manifest.transferId = QUuid::createUuid();
        return awaitResult([&](SstvShareProviderCompletion done) {
            return provider.createUploadAsync(
                manifest, SstvShareTransfer::deriveIdempotencyKey(manifest),
                std::move(done));
        });
    };
    Awaited result = run();
    CHECK(result.result.category()
          == SstvShareProviderFailure::ProviderUnavailable);
    CHECK(isRetryableShareProviderFailure(result.result.category()));
    responseStatus = 429;
    result = run();
    CHECK(result.result.category() == SstvShareProviderFailure::RateLimited);
    CHECK(result.result.retryAfterMs() == 3'000);
    CHECK(isRetryableShareProviderFailure(result.result.category()));
    retryAfter = "9223372036854775807";
    result = run();
    CHECK(result.result.category() == SstvShareProviderFailure::RateLimited);
    CHECK(result.result.retryAfterMs() == 24LL * 60LL * 60LL * 1000LL);
    responseStatus = 403;
    result = run();
    CHECK(result.result.category() == SstvShareProviderFailure::Authorization);
    CHECK(!isRetryableShareProviderFailure(result.result.category()));
    return true;
}

bool restInboundDiscoveryLookupDownloadAndDecisionsAreVerified()
{
    const QByteArray token("inbound-test-token");
    const QByteArray bytes = payload(1'300);
    const QString providerId = QStringLiteral("rest-inbound");
    const auto manifest = manifestFor(providerId, bytes);
    const QDateTime received = manifest.createdUtc.addSecs(2);
    const QByteArray inbox = inboxJson(
        QStringLiteral("incoming:one"), manifest, received);
    QByteArray contentRange = QByteArray("bytes 0-255/")
        + QByteArray::number(bytes.size());
    QVector<QByteArray> acknowledgementKeys;
    QVector<QByteArray> rejectionKeys;
    int authenticatedRequests = 0;
    ScopedHttpServer server([&](const HttpRequest& request) {
        if (request.headers.value("authorization")
            == QByteArray("Bearer ") + token) {
            ++authenticatedRequests;
        } else {
            return HttpResponse {401};
        }
        if (request.method == "GET"
            && request.target == "/api/v1/capabilities") {
            return HttpResponse {200, {}, {{"Content-Type", "application/json"}},
                                 fullCapabilitiesJson()};
        }
        if (request.method == "GET"
            && request.target == "/api/v1/recipients/recipient:9h1xyz") {
            return HttpResponse {200, {}, {{"Content-Type", "application/json"}},
                compactJson({
                    {QStringLiteral("providerId"), providerId},
                    {QStringLiteral("stableRecipientId"),
                     QStringLiteral("recipient:9h1xyz")},
                    {QStringLiteral("displayCallsign"),
                     QStringLiteral("9H1XYZ")},
                    {QStringLiteral("displayName"),
                     QStringLiteral("Verified recipient")},
                    {QStringLiteral("publicEncryptionKey"), QString {}},
                    {QStringLiteral("publicKeyFingerprint"), QString {}},
                    {QStringLiteral("verification"),
                     QStringLiteral("provider-verified")},
                    {QStringLiteral("trust"), QStringLiteral("trusted")},
                })};
        }
        if (request.method == "GET"
            && request.target == "/api/v1/inbox?limit=10") {
            return HttpResponse {200, {}, {{"Content-Type", "application/json"}},
                                 inbox};
        }
        if (request.method == "GET"
            && request.target == "/api/v1/inbox/incoming:one/content") {
            const QByteArray expectedRange = "bytes=0-255";
            if (request.headers.value("range") != expectedRange) {
                return HttpResponse {400};
            }
            const QByteArray chunk = bytes.left(256);
            return HttpResponse {206, {},
                {{"Content-Type", "application/octet-stream"},
                 {"Content-Range", contentRange},
                 {"Digest", QByteArray("sha-256=")
                      + sha256Base64(chunk)}}, chunk};
        }
        if (request.method == "POST"
            && request.target
                == "/api/v1/inbox/incoming:one/acknowledge") {
            acknowledgementKeys.push_back(
                request.headers.value("idempotency-key"));
            return HttpResponse {204};
        }
        if (request.method == "POST"
            && request.target == "/api/v1/inbox/incoming:one/reject") {
            rejectionKeys.push_back(request.headers.value("idempotency-key"));
            return HttpResponse {204};
        }
        return HttpResponse {404};
    });
    auto config = restConfig(server, providerId);
    config.credentialsRequired = true;
    auto credentials = std::make_shared<TestCredentialSource>(token);
    SstvGenericRestShareProvider provider(config, credentials);
    CHECK(provider.isConfigurationValid());
    CHECK(!provider.capabilities().incomingList);

    const Awaited discovered = awaitResult(
        [&](SstvShareProviderCompletion completion) {
            return provider.refreshCapabilitiesAsync(std::move(completion));
        });
    CHECK(discovered.completed && discovered.result.ok());
    CHECK(provider.capabilities().recipientLookup);
    CHECK(provider.capabilities().incomingList);
    CHECK(provider.capabilities().download);
    CHECK(provider.capabilities().acknowledgement);
    CHECK(provider.capabilities().rejection);
    CHECK(!provider.capabilities().incomingDelete);
    CHECK(!provider.capabilities().senderBlocking);

    const AwaitedRecipient recipient = awaitRecipient(
        [&](SstvShareRecipientCompletion completion) {
            return provider.lookupRecipientAsync(
                QStringLiteral("recipient:9h1xyz"), std::move(completion));
        });
    CHECK(recipient.completed && recipient.result.ok());
    CHECK(recipient.recipient.providerId == providerId);
    CHECK(recipient.recipient.verification
          == SstvShareRecipientVerification::ProviderVerified);
    CHECK(recipient.recipient.trust == SstvShareRecipientTrust::Trusted);

    const AwaitedIncoming incoming = awaitIncoming(
        [&](SstvShareIncomingCompletion completion) {
            return provider.listIncomingAsync(10, std::move(completion));
        });
    CHECK(incoming.completed && incoming.result.ok());
    CHECK(incoming.items.size() == 1);
    CHECK(incoming.items.front().opaqueId == QStringLiteral("incoming:one"));

    quint64 progress = 0U;
    const Awaited downloaded = awaitResult(
        [&](SstvShareProviderCompletion completion) {
            return provider.downloadAsync(
                QStringLiteral("incoming:one"), 0U, 256U,
                [&](quint64 receivedBytes, quint64) {
                    progress = std::max(progress, receivedBytes);
                }, std::move(completion));
        });
    CHECK(downloaded.result.ok());
    CHECK(downloaded.result.boundedPayload() == bytes.left(256));
    CHECK(progress == 256U);

    // Route hostile response headers through the actual provider rather than
    // testing the private range parser in isolation. Every case must fail
    // before a provider payload is accepted.
    const QList<QByteArray> hostileRanges {
        QByteArray("items 0-255/") + QByteArray::number(bytes.size()),
        QByteArray("bytes 256-511/") + QByteArray::number(bytes.size()),
        QByteArray("bytes 0-254/") + QByteArray::number(bytes.size()),
        QByteArray("bytes 0-255/") + QByteArray::number(bytes.size() + 1),
        QByteArray("bytes 0-255/18446744073709551615"),
        QByteArray("bytes 0-255/") + QByteArray::number(bytes.size())
            + QByteArray(" trailing"),
        QByteArray("bytes ") + QByteArray(100, '0'),
    };
    for (const QByteArray& hostileRange : hostileRanges) {
        contentRange = hostileRange;
        const Awaited invalid = awaitResult(
            [&](SstvShareProviderCompletion completion) {
                return provider.downloadAsync(
                    QStringLiteral("incoming:one"), 0U, 256U, {},
                    std::move(completion));
            });
        CHECK(!invalid.result.ok());
        CHECK(invalid.result.category() == SstvShareProviderFailure::Validation);
    }
    contentRange = QByteArray("bytes 0-255/") + QByteArray::number(bytes.size());

    for (int repetition = 0; repetition < 2; ++repetition) {
        const Awaited acknowledged = awaitResult(
            [&](SstvShareProviderCompletion completion) {
                return provider.acknowledgeAsync(
                    QStringLiteral("incoming:one"), std::move(completion));
            });
        CHECK(acknowledged.result.ok());
        const Awaited rejected = awaitResult(
            [&](SstvShareProviderCompletion completion) {
                return provider.rejectAsync(
                    QStringLiteral("incoming:one"), std::move(completion));
            });
        CHECK(rejected.result.ok());
    }
    CHECK(acknowledgementKeys.size() == 2);
    CHECK(rejectionKeys.size() == 2);
    CHECK(acknowledgementKeys.at(0).size() == 64);
    CHECK(acknowledgementKeys.at(0) == acknowledgementKeys.at(1));
    CHECK(rejectionKeys.at(0).size() == 64);
    CHECK(rejectionKeys.at(0) == rejectionKeys.at(1));
    CHECK(acknowledgementKeys.at(0) != rejectionKeys.at(0));
    CHECK(authenticatedRequests == 15);
    return true;
}

bool restIncomingDeleteAndSenderBlockRequireVerifiedCapabilities()
{
    const QByteArray token("delete-block-secret");
    const QByteArray bytes = payload(384);
    const QString providerId = QStringLiteral("rest-abuse-actions");
    const auto manifest = manifestFor(providerId, bytes);
    QJsonObject capabilityObject = QJsonDocument::fromJson(
        fullCapabilitiesJson()).object();
    capabilityObject.insert(QStringLiteral("incomingDelete"), true);
    capabilityObject.insert(QStringLiteral("senderBlocking"), true);
    const QByteArray capabilities = compactJson(capabilityObject);
    const QByteArray incoming = inboxJson(
        QStringLiteral("incoming:delete"), manifest,
        manifest.createdUtc.addSecs(2));
    QVector<QByteArray> blockKeys;
    QVector<QByteArray> deleteKeys;
    int authenticatedRequests = 0;
    ScopedHttpServer server([&](const HttpRequest& request) {
        if (request.headers.value("authorization")
            != QByteArray("Bearer ") + token) {
            return HttpResponse {401};
        }
        ++authenticatedRequests;
        if (request.method == "GET"
            && request.target == "/api/v1/capabilities") {
            return HttpResponse {200, {},
                {{"Content-Type", "application/json"}}, capabilities};
        }
        if (request.method == "GET"
            && request.target == "/api/v1/inbox?limit=1") {
            return HttpResponse {200, {},
                {{"Content-Type", "application/json"}}, incoming};
        }
        if (request.method == "POST"
            && request.target
                == "/api/v1/senders/station:9h1abc/block") {
            blockKeys.push_back(request.headers.value("idempotency-key"));
            return HttpResponse {204};
        }
        if (request.method == "DELETE"
            && request.target == "/api/v1/inbox/incoming:delete") {
            deleteKeys.push_back(request.headers.value("idempotency-key"));
            return HttpResponse {204};
        }
        return HttpResponse {404};
    });
    auto config = restConfig(server, providerId);
    config.credentialsRequired = true;
    auto credentials = std::make_shared<TestCredentialSource>(token);
    SstvGenericRestShareProvider provider(config, credentials);
    CHECK(provider.isConfigurationValid());
    CHECK(!provider.capabilities().incomingDelete);
    CHECK(!provider.capabilities().senderBlocking);

    Awaited action = awaitResult([&](SstvShareProviderCompletion completion) {
        return provider.deleteIncomingAsync(
            QStringLiteral("incoming:delete"), std::move(completion));
    });
    CHECK(!action.result.ok());
    action = awaitResult([&](SstvShareProviderCompletion completion) {
        return provider.blockSenderAsync(
            QStringLiteral("station:9h1abc"), std::move(completion));
    });
    CHECK(!action.result.ok());
    CHECK(server.requests().isEmpty());

    const Awaited discovered = awaitResult(
        [&](SstvShareProviderCompletion completion) {
            return provider.refreshCapabilitiesAsync(std::move(completion));
        });
    CHECK(discovered.result.ok());
    CHECK(provider.capabilities().incomingDelete);
    CHECK(provider.capabilities().senderBlocking);
    const AwaitedIncoming listed = awaitIncoming(
        [&](SstvShareIncomingCompletion completion) {
            return provider.listIncomingAsync(1, std::move(completion));
        });
    CHECK(listed.result.ok());
    CHECK(listed.items.size() == 1);

    for (int repetition = 0; repetition < 2; ++repetition) {
        action = awaitResult([&](SstvShareProviderCompletion completion) {
            return provider.blockSenderAsync(
                QStringLiteral("station:9h1abc"), std::move(completion));
        });
        CHECK(action.result.ok());
    }
    CHECK(blockKeys.size() == 2);
    CHECK(blockKeys.at(0).size() == 64);
    CHECK(blockKeys.at(0) == blockKeys.at(1));

    action = awaitResult([&](SstvShareProviderCompletion completion) {
        return provider.deleteIncomingAsync(
            QStringLiteral("incoming:delete"), std::move(completion));
    });
    CHECK(action.result.ok());
    CHECK(deleteKeys.size() == 1);
    CHECK(deleteKeys.front().size() == 64);
    CHECK(deleteKeys.front() != blockKeys.front());
    const qsizetype requestsAfterDelete = server.requests().size();
    action = awaitResult([&](SstvShareProviderCompletion completion) {
        return provider.deleteIncomingAsync(
            QStringLiteral("incoming:delete"), std::move(completion));
    });
    CHECK(!action.result.ok());
    CHECK(server.requests().size() == requestsAfterDelete);
    CHECK(authenticatedRequests == 5);
    return true;
}

bool restInboundMalformedCapabilityRedirectAndCancelFailClosed()
{
    const QByteArray bytes = payload(512);
    const QString providerId = QStringLiteral("rest-inbound-policy");
    const auto manifest = manifestFor(providerId, bytes);
    const QDateTime received = manifest.createdUtc.addSecs(1);
    int targetRequests = 0;
    ScopedHttpServer target([&](const HttpRequest&) {
        ++targetRequests;
        return HttpResponse {200};
    });
    enum class Mode {
        MalformedCapability,
        RedirectDownload,
        StallDownload,
        CapabilityFailure,
    };
    Mode mode = Mode::MalformedCapability;
    ScopedHttpServer source([&](const HttpRequest& request) {
        if (request.target == "/api/v1/capabilities") {
            if (mode == Mode::MalformedCapability) {
                return HttpResponse {200, {},
                    {{"Content-Type", "application/json"}},
                    "{\"protocolVersion\":1,\"incomingList\":true}"};
            }
            if (mode == Mode::CapabilityFailure) {
                return HttpResponse {503};
            }
            return HttpResponse {200, {},
                {{"Content-Type", "application/json"}},
                fullCapabilitiesJson()};
        }
        if (request.target == "/api/v1/inbox?limit=1") {
            return HttpResponse {200, {},
                {{"Content-Type", "application/json"}},
                inboxJson(QStringLiteral("incoming:policy"),
                          manifest, received)};
        }
        if (request.target
            == "/api/v1/inbox/incoming:policy/content") {
            if (mode == Mode::RedirectDownload) {
                return HttpResponse {307, {},
                    {{"Location", target.url(
                         QStringLiteral("/stolen")).toEncoded()}}, {}};
            }
            HttpResponse response;
            response.stall = true;
            return response;
        }
        return HttpResponse {404};
    });
    auto config = restConfig(source, providerId);
    config.transport.timeoutMs = 2'000;
    SstvGenericRestShareProvider provider(config, {});

    Awaited result = awaitResult([&](SstvShareProviderCompletion completion) {
        return provider.refreshCapabilitiesAsync(std::move(completion));
    });
    CHECK(!result.result.ok());
    CHECK(result.result.category() == SstvShareProviderFailure::Validation);
    CHECK(!provider.capabilities().incomingList);
    const AwaitedIncoming unavailable = awaitIncoming(
        [&](SstvShareIncomingCompletion completion) {
            return provider.listIncomingAsync(1, std::move(completion));
        });
    CHECK(!unavailable.result.ok());
    CHECK(source.requests().size() == 1);

    mode = Mode::RedirectDownload;
    result = awaitResult([&](SstvShareProviderCompletion completion) {
        return provider.refreshCapabilitiesAsync(std::move(completion));
    });
    CHECK(result.result.ok());
    const AwaitedIncoming incoming = awaitIncoming(
        [&](SstvShareIncomingCompletion completion) {
            return provider.listIncomingAsync(1, std::move(completion));
        });
    CHECK(incoming.result.ok());
    result = awaitResult([&](SstvShareProviderCompletion completion) {
        return provider.downloadAsync(QStringLiteral("incoming:policy"),
                                      0U, 256U, {},
                                      std::move(completion));
    });
    CHECK(!result.result.ok());
    CHECK(result.result.category()
          == SstvShareProviderFailure::PermanentProviderFailure);
    CHECK(targetRequests == 0);

    mode = Mode::StallDownload;
    Awaited cancelled;
    int completions = 0;
    bool cancellationIssued = false;
    QEventLoop loop;
    cancelled.operationId = provider.downloadAsync(
        QStringLiteral("incoming:policy"), 0U, 256U, {},
        [&](SstvShareProviderResult completionResult) {
            ++completions;
            cancelled.result = std::move(completionResult);
            cancelled.completed = true;
            loop.quit();
        });
    QTimer::singleShot(20, &loop, [&] {
        cancellationIssued = provider.cancelOperation(cancelled.operationId);
    });
    QTimer guard;
    guard.setSingleShot(true);
    QObject::connect(&guard, &QTimer::timeout, &loop, &QEventLoop::quit);
    guard.start(1'000);
    loop.exec();
    QCoreApplication::processEvents();
    CHECK(cancelled.completed);
    CHECK(cancellationIssued);
    CHECK(cancelled.result.category() == SstvShareProviderFailure::Cancelled);
    CHECK(completions == 1);

    mode = Mode::CapabilityFailure;
    result = awaitResult([&](SstvShareProviderCompletion completion) {
        return provider.refreshCapabilitiesAsync(std::move(completion));
    });
    CHECK(!result.result.ok());
    CHECK(result.result.category()
          == SstvShareProviderFailure::ProviderUnavailable);
    CHECK(!provider.capabilities().incomingList);
    const qsizetype requestsBeforeDisabledList = source.requests().size();
    const AwaitedIncoming disabledAfterFailure = awaitIncoming(
        [&](SstvShareIncomingCompletion completion) {
            return provider.listIncomingAsync(1, std::move(completion));
        });
    CHECK(!disabledAfterFailure.result.ok());
    CHECK(source.requests().size() == requestsBeforeDisabledList);
    return true;
}

bool plaintextTransportNeedsBothCompileAndRuntimeTestGates()
{
    ScopedHttpServer server([](const HttpRequest&) { return HttpResponse {}; });
    auto config = restConfig(server, QStringLiteral("rest-gate"));
    config.transport.allowInsecureLocalhostForTests = false;
    SstvGenericRestShareProvider provider(config, {});
    CHECK(!provider.isConfigurationValid());
    const QByteArray bytes = payload(64);
    const auto manifest = manifestFor(config.providerId, bytes);
    const Awaited result = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.createUploadAsync(
            manifest, SstvShareTransfer::deriveIdempotencyKey(manifest),
            std::move(done));
    });
    CHECK(!result.result.ok());
    CHECK(server.requests().isEmpty());

    config.transport.allowInsecureLocalhostForTests = true;
    SstvGenericRestShareProvider compileGateProvider(config, {});
#if DECODIUM_SSTV_ALLOW_INSECURE_LOCAL_TEST_TRANSPORT
    CHECK(compileGateProvider.isConfigurationValid());
#else
    CHECK(!compileGateProvider.isConfigurationValid());
#endif
    return true;
}

bool webDavUsesPropfindPutHeadDeleteAndVerifiesDigest()
{
    const QByteArray bytes = payload(512);
    QByteArray stored;
    QVector<QByteArray> methods;
    bool semanticsValid = true;
    int deleteRequests = 0;
    ScopedHttpServer server([&](const HttpRequest& request) {
        methods.push_back(request.method);
        if (request.method == "PROPFIND") {
            semanticsValid = semanticsValid
                && request.headers.value("depth") == "0";
            return HttpResponse {207, {}, {{"Content-Type", "application/xml"}},
                                 "<multistatus/>"};
        }
        if (request.method == "PUT") {
            semanticsValid = semanticsValid
                && request.headers.value("if-none-match") == "*"
                && request.headers.value("digest")
                    == QByteArray("sha-256=") + sha256Base64(bytes);
            stored = request.body;
            return HttpResponse {201};
        }
        if (request.method == "HEAD") {
            return HttpResponse {200, {},
                {{"Content-Length", QByteArray::number(stored.size())},
                 {"Digest", QByteArray("sha-256=") + sha256Base64(stored)}}};
        }
        if (request.method == "DELETE") {
            ++deleteRequests;
            return HttpResponse {deleteRequests == 1 ? 204 : 404};
        }
        if (request.method == "GET"
            && request.target == "/dav/incoming.png") {
            const QByteArray chunk = bytes.left(128);
            return HttpResponse {206, {},
                {{"Content-Type", "image/png"},
                 {"Content-Range", QByteArray("bytes 0-127/")
                      + QByteArray::number(bytes.size())}}, chunk};
        }
        return HttpResponse {404};
    });
    SstvWebDavProviderConfig config;
    config.providerId = QStringLiteral("webdav-test");
    config.collectionUrl = server.url(QStringLiteral("/dav/"));
    config.credentialsRequired = true;
    config.transport.timeoutMs = 750;
    config.transport.maximumResponseBytes = 4'096;
    config.transport.allowInsecureLocalhostForTests = true;
    auto credentials = std::make_shared<TestCredentialSource>(
        QByteArray("webdav-delete-test-token"));
    SstvWebDavShareProvider provider(config, credentials);
    CHECK(provider.isConfigurationValid());
    CHECK(provider.capabilities().remoteDelete);
    CHECK(provider.capabilities().download);
    const Awaited downloaded = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.downloadAsync(QStringLiteral("incoming.png"),
                                          0U, 128U, {}, std::move(done));
        });
    CHECK(downloaded.result.ok());
    CHECK(downloaded.result.boundedPayload() == bytes.left(128));
    const auto manifest = manifestFor(config.providerId, bytes);
    const QString key = SstvShareTransfer::deriveIdempotencyKey(manifest);
    const Awaited created = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.createUploadAsync(manifest, key, std::move(done));
    });
    CHECK(created.result.ok());
    const Awaited uploaded = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.uploadChunkAsync(created.result.handle(), 0U, bytes,
                                         manifest.sha256, {}, std::move(done));
    });
    CHECK(uploaded.result.ok());
    CHECK(stored == bytes);
    const Awaited completed = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.completeUploadAsync(uploaded.result.handle(), key,
                                            std::move(done));
    });
    CHECK(completed.result.ok());
    const QString expectedRemoteObjectId =
        manifest.transferId.toString(QUuid::WithoutBraces)
        + QStringLiteral(".png");
    CHECK(completed.result.handle().opaqueId == expectedRemoteObjectId);
    SstvWebDavShareProvider restartedProvider(config, credentials);
    const Awaited cancelled = awaitResult([&](SstvShareProviderCompletion done) {
        return restartedProvider.deleteRemoteObjectAsync(
            completed.result.handle().opaqueId, std::move(done));
    });
    CHECK(cancelled.result.ok());
    const Awaited idempotentMissing = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return restartedProvider.deleteRemoteObjectAsync(
                completed.result.handle().opaqueId, std::move(done));
        });
    CHECK(idempotentMissing.result.ok());
    auto unauthenticatedConfig = config;
    unauthenticatedConfig.credentialsRequired = false;
    SstvWebDavShareProvider unauthenticatedProvider(
        unauthenticatedConfig, {});
    const Awaited unauthenticatedMissing = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return unauthenticatedProvider.deleteRemoteObjectAsync(
                completed.result.handle().opaqueId, std::move(done));
        });
    CHECK(!unauthenticatedMissing.result.ok());
    CHECK(unauthenticatedMissing.result.category()
          == SstvShareProviderFailure::NotFound);
    const qsizetype requestCount = server.requests().size();
    const Awaited hostileId = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return restartedProvider.deleteRemoteObjectAsync(
                QStringLiteral("unowned-object.png"), std::move(done));
        });
    CHECK(!hostileId.result.ok());
    CHECK(server.requests().size() == requestCount);
    CHECK(semanticsValid);
    CHECK(methods == QVector<QByteArray>(
        {"GET", "PROPFIND", "PUT", "HEAD", "DELETE", "DELETE",
         "DELETE"}));
    return true;
}

bool presignedPutKeepsSignedUrlOpaqueAndVerifiesServerHash()
{
    const QByteArray bytes = payload(768);
    QByteArray stored;
    QByteArray targetSeen;
    bool semanticsValid = true;
    ScopedHttpServer server([&](const HttpRequest& request) {
        targetSeen = request.target;
        semanticsValid = request.method == "PUT"
            && request.headers.value("x-test-target-lease") == "present"
            && request.headers.value("x-amz-checksum-sha256")
                == sha256Base64(bytes);
        stored = request.body;
        return HttpResponse {200, {},
            {{"X-Amz-Checksum-Sha256", sha256Base64(stored)}}};
    });
    QUrl signedUrl = server.url(QStringLiteral("/object/sstv.png"));
    signedUrl.setQuery(QStringLiteral("signature=VERY_SECRET_SIGNED_QUERY"));
    auto source = std::make_shared<TestPresignedSource>(signedUrl);
    SstvPresignedPutProviderConfig config;
    config.providerId = QStringLiteral("presigned-test");
    config.transport.timeoutMs = 750;
    config.transport.maximumResponseBytes = 4'096;
    config.transport.allowInsecureLocalhostForTests = true;
    SstvPresignedPutShareProvider provider(config, source);
    CHECK(provider.isConfigurationValid());
    const auto manifest = manifestFor(config.providerId, bytes);
    const QString key = SstvShareTransfer::deriveIdempotencyKey(manifest);
    const Awaited created = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.createUploadAsync(manifest, key, std::move(done));
    });
    CHECK(created.result.ok());
    CHECK(server.requests().isEmpty());
    const Awaited badHash = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.uploadChunkAsync(created.result.handle(), 0U, bytes,
            QString(64, QLatin1Char('0')), {}, std::move(done));
    });
    CHECK(badHash.result.category() == SstvShareProviderFailure::Integrity);
    CHECK(server.requests().isEmpty());
    const Awaited uploaded = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.uploadChunkAsync(created.result.handle(), 0U, bytes,
                                         manifest.sha256, {}, std::move(done));
    });
    CHECK(uploaded.result.ok());
    CHECK(stored == bytes);
    CHECK(semanticsValid);
    CHECK(targetSeen.contains("signature=VERY_SECRET_SIGNED_QUERY"));
    CHECK(!uploaded.result.redactedDiagnostic().contains(
        QStringLiteral("VERY_SECRET_SIGNED_QUERY")));
    CHECK(!containsNetworkUrl(uploaded.result.handle().opaqueId));
    const Awaited completed = awaitResult([&](SstvShareProviderCompletion done) {
        return provider.completeUploadAsync(uploaded.result.handle(), key,
                                            std::move(done));
    });
    CHECK(completed.result.ok());
    CHECK(source->liveLeases() == 0);
    const Awaited completedAgain = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return provider.completeUploadAsync(
                uploaded.result.handle(), key, std::move(done));
        });
    CHECK(completedAgain.result.ok());
    CHECK(source->liveLeases() == 0);
    CHECK(server.requests().size() == 1);
    return true;
}

bool providerSessionCapsTerminalReclaimExpiryAndLeaseRelease()
{
    const QByteArray bytes = payload(96);
    int restCreates = 0;
    ScopedHttpServer restServer([&](const HttpRequest& request) {
        if (request.method == "POST" && request.target == "/api/uploads") {
            ++restCreates;
            return HttpResponse {201, {}, {{"Content-Type", "application/json"}},
                QByteArray("{\"uploadId\":\"remote-")
                    + QByteArray::number(restCreates)
                    + "\",\"committedBytes\":0}"};
        }
        if (request.method == "DELETE"
            && request.target.startsWith("/api/uploads/")) {
            return HttpResponse {204};
        }
        return HttpResponse {404};
    });
    auto rest = restConfig(restServer, QStringLiteral("rest-session-bound"));
    rest.maximumActiveSessions = 2;
    rest.maximumTerminalRecords = 2;
    SstvGenericRestShareProvider restProvider(rest, {});
    CHECK(restProvider.isConfigurationValid());
    QVector<SstvShareManifestV1> restManifests;
    QVector<SstvShareUploadHandle> restHandles;
    for (int index = 0; index < 2; ++index) {
        restManifests.push_back(manifestFor(rest.providerId, bytes));
        const QString key = SstvShareTransfer::deriveIdempotencyKey(
            restManifests.back());
        const Awaited created = awaitResult(
            [&](SstvShareProviderCompletion done) {
                return restProvider.createUploadAsync(
                    restManifests.back(), key, std::move(done));
            });
        CHECK(created.result.ok());
        restHandles.push_back(created.result.handle());
    }
    const auto thirdRestManifest = manifestFor(rest.providerId, bytes);
    const QString thirdRestKey = SstvShareTransfer::deriveIdempotencyKey(
        thirdRestManifest);
    const Awaited restFull = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return restProvider.createUploadAsync(
                thirdRestManifest, thirdRestKey, std::move(done));
        });
    CHECK(!restFull.result.ok());
    CHECK(restFull.result.category()
          == SstvShareProviderFailure::ProviderUnavailable);
    const Awaited restCancelled = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return restProvider.cancelUploadAsync(
                restHandles.front(), std::move(done));
        });
    CHECK(restCancelled.result.ok());
    const qsizetype afterRestCancel = restServer.requests().size();
    const Awaited restCancelledAgain = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return restProvider.cancelUploadAsync(
                restHandles.front(), std::move(done));
        });
    CHECK(restCancelledAgain.result.ok());
    CHECK(restServer.requests().size() == afterRestCancel);
    const Awaited restAfterReclaim = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return restProvider.createUploadAsync(
                thirdRestManifest, thirdRestKey, std::move(done));
        });
    CHECK(restAfterReclaim.result.ok());

    ScopedHttpServer webDavServer([](const HttpRequest& request) {
        if (request.method == "PROPFIND") {
            return HttpResponse {207, {}, {{"Content-Type", "application/xml"}},
                                 "<multistatus/>"};
        }
        if (request.method == "DELETE") {
            return HttpResponse {204};
        }
        return HttpResponse {404};
    });
    SstvWebDavProviderConfig webDav;
    webDav.providerId = QStringLiteral("webdav-session-bound");
    webDav.collectionUrl = webDavServer.url(QStringLiteral("/dav/"));
    webDav.credentialsRequired = false;
    webDav.maximumActiveSessions = 2;
    webDav.maximumTerminalRecords = 2;
    webDav.transport.timeoutMs = 750;
    webDav.transport.maximumResponseBytes = 4'096;
    webDav.transport.allowInsecureLocalhostForTests = true;
    SstvWebDavShareProvider webDavProvider(webDav, {});
    CHECK(webDavProvider.isConfigurationValid());
    QVector<SstvShareUploadHandle> webDavHandles;
    for (int index = 0; index < 2; ++index) {
        const auto manifest = manifestFor(webDav.providerId, bytes);
        const QString key = SstvShareTransfer::deriveIdempotencyKey(manifest);
        const Awaited created = awaitResult(
            [&](SstvShareProviderCompletion done) {
                return webDavProvider.createUploadAsync(
                    manifest, key, std::move(done));
            });
        CHECK(created.result.ok());
        webDavHandles.push_back(created.result.handle());
    }
    const auto thirdWebDavManifest = manifestFor(webDav.providerId, bytes);
    const QString thirdWebDavKey = SstvShareTransfer::deriveIdempotencyKey(
        thirdWebDavManifest);
    const Awaited webDavFull = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return webDavProvider.createUploadAsync(
                thirdWebDavManifest, thirdWebDavKey, std::move(done));
        });
    CHECK(webDavFull.result.category()
          == SstvShareProviderFailure::ProviderUnavailable);
    const Awaited webDavCancelled = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return webDavProvider.cancelUploadAsync(
                webDavHandles.front(), std::move(done));
        });
    CHECK(webDavCancelled.result.ok());
    const Awaited webDavAfterReclaim = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return webDavProvider.createUploadAsync(
                thirdWebDavManifest, thirdWebDavKey, std::move(done));
        });
    CHECK(webDavAfterReclaim.result.ok());

    ScopedHttpServer targetServer([](const HttpRequest& request) {
        if (request.method != "PUT") {
            return HttpResponse {404};
        }
        return HttpResponse {200, {},
            {{"X-Amz-Checksum-Sha256", sha256Base64(request.body)}}};
    });
    auto source = std::make_shared<TestPresignedSource>(
        targetServer.url(QStringLiteral("/target")));
    SstvPresignedPutProviderConfig presigned;
    presigned.providerId = QStringLiteral("presigned-session-bound");
    presigned.maximumActiveSessions = 2;
    presigned.maximumTerminalRecords = 2;
    presigned.transport.timeoutMs = 750;
    presigned.transport.maximumResponseBytes = 4'096;
    presigned.transport.allowInsecureLocalhostForTests = true;
    SstvPresignedPutShareProvider presignedProvider(presigned, source);
    CHECK(presignedProvider.isConfigurationValid());
    QVector<SstvShareManifestV1> signedManifests;
    QVector<SstvShareUploadHandle> signedHandles;
    QVector<QString> signedKeys;
    for (int index = 0; index < 2; ++index) {
        signedManifests.push_back(manifestFor(presigned.providerId, bytes));
        signedKeys.push_back(SstvShareTransfer::deriveIdempotencyKey(
            signedManifests.back()));
        const Awaited created = awaitResult(
            [&](SstvShareProviderCompletion done) {
                return presignedProvider.createUploadAsync(
                    signedManifests.back(), signedKeys.back(), std::move(done));
            });
        CHECK(created.result.ok());
        signedHandles.push_back(created.result.handle());
    }
    CHECK(source->liveLeases() == 2);
    const auto thirdSignedManifest = manifestFor(presigned.providerId, bytes);
    const QString thirdSignedKey = SstvShareTransfer::deriveIdempotencyKey(
        thirdSignedManifest);
    const Awaited signedFull = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return presignedProvider.createUploadAsync(
                thirdSignedManifest, thirdSignedKey, std::move(done));
        });
    CHECK(signedFull.result.category()
          == SstvShareProviderFailure::ProviderUnavailable);
    CHECK(source->acquiredLeases() == 2 && source->liveLeases() == 2);
    const Awaited signedCancelled = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return presignedProvider.cancelUploadAsync(
                signedHandles.front(), std::move(done));
        });
    CHECK(signedCancelled.result.ok());
    CHECK(source->liveLeases() == 1);
    const Awaited signedAfterReclaim = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return presignedProvider.createUploadAsync(
                thirdSignedManifest, thirdSignedKey, std::move(done));
        });
    CHECK(signedAfterReclaim.result.ok());
    CHECK(source->liveLeases() == 2);
    const Awaited signedUploaded = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return presignedProvider.uploadChunkAsync(
                signedHandles.at(1), 0U, bytes,
                signedManifests.at(1).sha256, {}, std::move(done));
        });
    CHECK(signedUploaded.result.ok());
    const Awaited signedCompleted = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return presignedProvider.completeUploadAsync(
                signedUploaded.result.handle(), signedKeys.at(1),
                std::move(done));
        });
    CHECK(signedCompleted.result.ok());
    CHECK(source->liveLeases() == 1);
    const Awaited signedThirdCancelled = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return presignedProvider.cancelUploadAsync(
                signedAfterReclaim.result.handle(), std::move(done));
        });
    CHECK(signedThirdCancelled.result.ok());
    CHECK(source->liveLeases() == 0);

    auto expirySource = std::make_shared<TestPresignedSource>(
        targetServer.url(QStringLiteral("/expiring-target")));
    auto expiryConfig = presigned;
    expiryConfig.providerId = QStringLiteral("presigned-expiry-bound");
    expiryConfig.maximumActiveSessions = 1;
    SstvPresignedPutShareProvider expiryProvider(expiryConfig, expirySource);
    auto expiring = manifestFor(expiryConfig.providerId, bytes);
    expiring.expiresUtc = QDateTime::currentDateTimeUtc().addMSecs(75);
    const QString expiringKey = SstvShareTransfer::deriveIdempotencyKey(expiring);
    const Awaited expiringCreated = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return expiryProvider.createUploadAsync(
                expiring, expiringKey, std::move(done));
        });
    CHECK(expiringCreated.result.ok() && expirySource->liveLeases() == 1);
    processEventsFor(125);
    const auto replacement = manifestFor(expiryConfig.providerId, bytes);
    const QString replacementKey = SstvShareTransfer::deriveIdempotencyKey(
        replacement);
    const Awaited replacementCreated = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return expiryProvider.createUploadAsync(
                replacement, replacementKey, std::move(done));
        });
    CHECK(replacementCreated.result.ok());
    CHECK(expirySource->acquiredLeases() == 2);
    CHECK(expirySource->releasedLeases() == 1);
    CHECK(expirySource->liveLeases() == 1);

    const Awaited replacementCancelled = awaitResult(
        [&](SstvShareProviderCompletion done) {
            return expiryProvider.cancelUploadAsync(
                replacementCreated.result.handle(), std::move(done));
        });
    CHECK(replacementCancelled.result.ok());
    CHECK(expirySource->liveLeases() == 0);

    auto destructionSource = std::make_shared<TestPresignedSource>(
        targetServer.url(QStringLiteral("/destruction-target")));
    {
        auto destructionConfig = presigned;
        destructionConfig.providerId = QStringLiteral("presigned-destruction");
        SstvPresignedPutShareProvider destructionProvider(
            destructionConfig, destructionSource);
        const auto destructionManifest = manifestFor(
            destructionConfig.providerId, bytes);
        const QString destructionKey = SstvShareTransfer::deriveIdempotencyKey(
            destructionManifest);
        const Awaited destructionCreated = awaitResult(
            [&](SstvShareProviderCompletion done) {
                return destructionProvider.createUploadAsync(
                    destructionManifest, destructionKey, std::move(done));
            });
        CHECK(destructionCreated.result.ok());
        CHECK(destructionSource->liveLeases() == 1);
    }
    CHECK(destructionSource->liveLeases() == 0);
    CHECK(destructionSource->acquiredLeases()
          == destructionSource->releasedLeases());

    auto invalid = presigned;
    invalid.maximumActiveSessions = 257;
    SstvPresignedPutShareProvider invalidProvider(invalid, source);
    CHECK(!invalidProvider.isConfigurationValid());
    auto invalidRest = rest;
    invalidRest.maximumTerminalRecords = 0;
    SstvGenericRestShareProvider invalidRestProvider(invalidRest, {});
    CHECK(!invalidRestProvider.isConfigurationValid());
    auto invalidWebDav = webDav;
    invalidWebDav.maximumActiveSessions = 257;
    SstvWebDavShareProvider invalidWebDavProvider(invalidWebDav, {});
    CHECK(!invalidWebDavProvider.isConfigurationValid());
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)
    const std::vector<std::pair<const char*, bool (*)()>> tests {
        {"restHappyPathIsAsyncBoundedAndIdempotent",
         restHappyPathIsAsyncBoundedAndIdempotent},
        {"credentialLeaseIsOpaqueAndDiagnosticsAreRedacted",
         credentialLeaseIsOpaqueAndDiagnosticsAreRedacted},
        {"crossOriginRedirectIsRejectedWithoutCredentialForwarding",
         crossOriginRedirectIsRejectedWithoutCredentialForwarding},
        {"tlsHandshakeFailureIsFailClosedExactlyOnceWithoutCredentialLeak",
         tlsHandshakeFailureIsFailClosedExactlyOnceWithoutCredentialLeak},
        {"timeoutAndCancelCompleteExactlyOnce",
         timeoutAndCancelCompleteExactlyOnce},
        {"pendingCapacityAndCancellationAreBounded",
         pendingCapacityAndCancellationAreBounded},
        {"operationIdExhaustionFailsClosedWithoutCollision",
         operationIdExhaustionFailsClosedWithoutCollision},
        {"aggregatePendingBodyAndResponseBudgetIsReclaimed",
         aggregatePendingBodyAndResponseBudgetIsReclaimed},
        {"oversizedAndMalformedResponsesAreRejected",
         oversizedAndMalformedResponsesAreRejected},
        {"statusClassificationAndRetryAfterAreExact",
         statusClassificationAndRetryAfterAreExact},
        {"restRemoteRevocationUsesOnlyDocumentedTransferDelete",
         restRemoteRevocationUsesOnlyDocumentedTransferDelete},
        {"restInboundDiscoveryLookupDownloadAndDecisionsAreVerified",
         restInboundDiscoveryLookupDownloadAndDecisionsAreVerified},
        {"restIncomingDeleteAndSenderBlockRequireVerifiedCapabilities",
         restIncomingDeleteAndSenderBlockRequireVerifiedCapabilities},
        {"restInboundMalformedCapabilityRedirectAndCancelFailClosed",
         restInboundMalformedCapabilityRedirectAndCancelFailClosed},
        {"plaintextTransportNeedsBothCompileAndRuntimeTestGates",
         plaintextTransportNeedsBothCompileAndRuntimeTestGates},
        {"webDavUsesPropfindPutHeadDeleteAndVerifiesDigest",
         webDavUsesPropfindPutHeadDeleteAndVerifiesDigest},
        {"presignedPutKeepsSignedUrlOpaqueAndVerifiesServerHash",
         presignedPutKeepsSignedUrlOpaqueAndVerifiesServerHash},
        {"providerSessionCapsTerminalReclaimExpiryAndLeaseRelease",
         providerSessionCapsTerminalReclaimExpiryAndLeaseRelease},
    };

    int failed = 0;
    qsizetype executed = 0;
    for (const auto& test : tests) {
        if (argc > 1 && QByteArray(argv[1]) != test.first) {
            continue;
        }
        ++executed;
        const bool passed = test.second();
        std::cout << (passed ? "PASS " : "FAIL ") << test.first << '\n';
        failed += passed ? 0 : 1;
    }
    std::cout << "checks=" << g_checks << " tests=" << executed
              << " failed=" << failed << '\n';
    return failed == 0 && executed > 0 ? 0 : 1;
}
