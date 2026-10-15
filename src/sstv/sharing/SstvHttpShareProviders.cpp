// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvHttpShareProviders.h"

#include "SstvShareSecurity.h"
#include "SstvShareTransfer.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QSslError>
#include <QSslSocket>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>

#ifndef DECODIUM_SSTV_ALLOW_INSECURE_LOCAL_TEST_TRANSPORT
#define DECODIUM_SSTV_ALLOW_INSECURE_LOCAL_TEST_TRANSPORT 0
#endif

namespace decodium::sstv::sharing {
namespace {

constexpr qsizetype kAbsoluteMaximumHttpResponseBytes = 1024 * 1024;
constexpr qsizetype kMaximumHttpResponseHeaderBytes = 32 * 1024;
constexpr qsizetype kMaximumHttpResponseHeaders = 128;
constexpr qsizetype kMaximumTransportUrlBytes = 8 * 1024;
constexpr int kAbsoluteMaximumRedirects = 5;
constexpr int kMinimumTimeoutMs = 100;
constexpr int kMaximumTimeoutMs = 5 * 60 * 1000;
constexpr qsizetype kHardMaximumProviderSessions = 256;
constexpr qsizetype kHardMaximumTerminalRecords = 256;
constexpr double kMaximumExactlyRepresentableJsonInteger = 9'007'199'254'740'991.0;

bool validSessionBounds(qsizetype active, qsizetype terminal) noexcept
{
    return active > 0 && active <= kHardMaximumProviderSessions
        && terminal > 0 && terminal <= kHardMaximumTerminalRecords;
}

bool validTransportOptions(const SstvHttpTransportOptions& options)
{
    return options.timeoutMs >= kMinimumTimeoutMs
        && options.timeoutMs <= kMaximumTimeoutMs
        && options.maximumResponseBytes > 0
        && options.maximumResponseBytes <= kAbsoluteMaximumHttpResponseBytes
        && options.maximumRedirects >= 0
        && options.maximumRedirects <= kAbsoluteMaximumRedirects;
}

#if DECODIUM_SSTV_ALLOW_INSECURE_LOCAL_TEST_TRANSPORT
bool isLoopbackHost(const QString& host)
{
    if (host.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    const QHostAddress address(host);
    return !address.isNull() && address.isLoopback();
}
#endif

int effectivePort(const QUrl& url)
{
    const int explicitPort = url.port(-1);
    if (explicitPort >= 0) {
        return explicitPort;
    }
    return url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
        ? 443
        : 80;
}

bool containsSensitiveHeader(const QByteArray& name)
{
    const QByteArray lowered = name.trimmed().toLower();
    return lowered == "authorization" || lowered == "proxy-authorization"
        || lowered == "cookie" || lowered == "set-cookie"
        || lowered == "x-api-key";
}

bool validMethod(const QByteArray& method)
{
    return method == "GET" || method == "HEAD" || method == "POST"
        || method == "PUT" || method == "DELETE" || method == "PROPFIND";
}

void applySecureRequestPolicy(QNetworkRequest& request)
{
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::ManualRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,
                         QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,
                         QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute,
                         QNetworkRequest::Manual);
}

bool validHeader(const QPair<QByteArray, QByteArray>& header)
{
    if (header.first.isEmpty() || header.first.size() > 128
        || header.second.size() > 8'192
        || containsSensitiveHeader(header.first)) {
        return false;
    }
    for (const char value : header.first) {
        const unsigned char byte = static_cast<unsigned char>(value);
        if (!(std::isalnum(byte) || value == '-')) {
            return false;
        }
    }
    return !header.second.contains('\r') && !header.second.contains('\n');
}

bool statusAccepted(const QVector<int>& accepted, int status)
{
    return std::find(accepted.cbegin(), accepted.cend(), status)
        != accepted.cend();
}

qint64 boundedRetryAfterMs(const QByteArray& value)
{
    const QByteArray trimmed = value.trimmed();
    bool integerOk = false;
    const qint64 seconds = trimmed.toLongLong(&integerOk, 10);
    if (integerOk && seconds >= 0) {
        constexpr qint64 maximum = 24LL * 60LL * 60LL * 1000LL;
        return seconds >= maximum / 1000
            ? maximum
            : seconds * 1000;
    }
    const QDateTime date = QDateTime::fromString(QString::fromLatin1(trimmed),
                                                 Qt::RFC2822Date).toUTC();
    if (!date.isValid()) {
        return 0;
    }
    return std::clamp<qint64>(QDateTime::currentDateTimeUtc().msecsTo(date),
                              0, 24LL * 60LL * 60LL * 1000LL);
}

SstvShareProviderResult httpStatusFailure(int status,
                                          const QByteArray& retryAfter,
                                          const QString& label,
                                          const QUrl& url)
{
    SstvShareProviderFailure category {
        SstvShareProviderFailure::PermanentProviderFailure};
    if (status == 401 || status == 407) {
        category = SstvShareProviderFailure::Authentication;
    } else if (status == 403) {
        category = SstvShareProviderFailure::Authorization;
    } else if (status == 404 || status == 410) {
        category = SstvShareProviderFailure::NotFound;
    } else if (status == 409 || status == 412) {
        category = SstvShareProviderFailure::Conflict;
    } else if (status == 429) {
        category = SstvShareProviderFailure::RateLimited;
    } else if (status == 408 || status == 425) {
        category = SstvShareProviderFailure::TransientNetwork;
    } else if (status == 500 || status == 502 || status == 503
               || status == 504) {
        category = SstvShareProviderFailure::ProviderUnavailable;
    } else if (status == 400 || status == 405 || status == 406
               || status == 411 || status == 413 || status == 415
               || status == 422) {
        category = SstvShareProviderFailure::Validation;
    }
    const QString diagnostic = QStringLiteral("%1 failed with HTTP %2 at %3")
        .arg(sanitizeShareDisplayText(label, 64), QString::number(status),
             redactedShareUrl(url));
    return SstvShareProviderResult::failure(
        category, diagnostic,
        category == SstvShareProviderFailure::RateLimited
            ? boundedRetryAfterMs(retryAfter)
            : 0);
}

SstvShareProviderFailure classifyNetworkFailure(QNetworkReply::NetworkError error)
{
    switch (error) {
    case QNetworkReply::SslHandshakeFailedError:
        return SstvShareProviderFailure::TlsValidation;
    case QNetworkReply::AuthenticationRequiredError:
    case QNetworkReply::ProxyAuthenticationRequiredError:
        return SstvShareProviderFailure::Authentication;
    case QNetworkReply::ContentAccessDenied:
        return SstvShareProviderFailure::Authorization;
    case QNetworkReply::ContentNotFoundError:
        return SstvShareProviderFailure::NotFound;
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::NetworkSessionFailedError:
        return SstvShareProviderFailure::Offline;
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyTimeoutError:
    case QNetworkReply::ServiceUnavailableError:
        return SstvShareProviderFailure::TransientNetwork;
    default:
        return SstvShareProviderFailure::PermanentProviderFailure;
    }
}

bool jsonHasOnly(const QJsonObject& object,
                 std::initializer_list<QString> allowed)
{
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (std::find(allowed.begin(), allowed.end(), it.key()) == allowed.end()) {
            return false;
        }
    }
    return true;
}

std::optional<quint64> jsonUnsigned(const QJsonObject& object,
                                    const QString& name,
                                    bool required)
{
    const auto it = object.constFind(name);
    if (it == object.constEnd()) {
        return required ? std::optional<quint64>() : std::optional<quint64>(0U);
    }
    if (!it->isDouble()) {
        return {};
    }
    const double value = it->toDouble(-1.0);
    if (!std::isfinite(value) || value < 0.0
        || value > kMaximumExactlyRepresentableJsonInteger
        || std::floor(value) != value) {
        return {};
    }
    return static_cast<quint64>(value);
}

bool validEndpointPath(const QString& path, bool requiresUploadId)
{
    if (path.isEmpty() || path.size() > 512 || !path.startsWith(QLatin1Char('/'))
        || path.contains(QLatin1Char('\\')) || path.contains(QLatin1Char('?'))
        || path.contains(QLatin1Char('#')) || path.contains(QStringLiteral(".."))
        || path.contains(QStringLiteral("://")) || path.contains(QLatin1Char('@'))
        || sanitizeShareDisplayText(path, 512) != path) {
        return false;
    }
    const qsizetype first = path.indexOf(QStringLiteral("{uploadId}"));
    const qsizetype last = path.lastIndexOf(QStringLiteral("{uploadId}"));
    return requiresUploadId ? first >= 0 && first == last : first < 0;
}

bool validEndpointTemplate(const QString& path,
                           const QString& token,
                           bool requiresToken)
{
    if (path.isEmpty() || path.size() > 512
        || !path.startsWith(QLatin1Char('/'))
        || path.contains(QLatin1Char('\\')) || path.contains(QLatin1Char('?'))
        || path.contains(QLatin1Char('#')) || path.contains(QStringLiteral(".."))
        || path.contains(QStringLiteral("://")) || path.contains(QLatin1Char('@'))
        || sanitizeShareDisplayText(path, 512) != path) {
        return false;
    }
    const qsizetype first = path.indexOf(token);
    return requiresToken
        ? first >= 0 && first == path.lastIndexOf(token)
        : first < 0;
}

QUrl endpointWithPath(const QUrl& base, const QString& path)
{
    QUrl result = base;
    result.setPath(path);
    result.setQuery(QString {});
    result.setFragment({});
    return result;
}

QString pathForUpload(const QString& pathTemplate, const QString& remoteId)
{
    QString result = pathTemplate;
    result.replace(QStringLiteral("{uploadId}"), remoteId);
    return result;
}

QString pathForIdentifier(const QString& pathTemplate,
                          const QString& token,
                          const QString& identifier)
{
    QString result = pathTemplate;
    result.replace(token, identifier);
    return result;
}

QByteArray idempotencyForIncomingAction(const QString& providerId,
                                        const QString& incomingId,
                                        const QByteArray& action)
{
    QCryptographicHash digest(QCryptographicHash::Sha256);
    digest.addData(providerId.toUtf8());
    digest.addData(QByteArray(1, '\0'));
    digest.addData(incomingId.toUtf8());
    digest.addData(QByteArray(1, '\0'));
    digest.addData(action);
    return digest.result().toHex();
}

std::optional<QDateTime> exactUtcTimestamp(const QJsonValue& value)
{
    if (!value.isString()) {
        return {};
    }
    const QString text = value.toString();
    QDateTime parsed = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!parsed.isValid() || parsed.offsetFromUtc() != 0
        || !text.endsWith(QLatin1Char('Z'))
        || parsed.toUTC().toString(Qt::ISODateWithMs) != text) {
        return {};
    }
    return parsed.toUTC();
}

struct ParsedContentRange final
{
    quint64 first {0U};
    quint64 last {0U};
    quint64 total {0U};
};

std::optional<ParsedContentRange> parseContentRange(const QByteArray& header)
{
    const QByteArray value = header.trimmed();
    if (!value.startsWith("bytes ") || value.size() > 96) {
        return {};
    }
    const qsizetype dash = value.indexOf('-', 6);
    const qsizetype slash = value.indexOf('/', dash + 1);
    if (dash <= 6 || slash <= dash + 1 || slash == value.size() - 1) {
        return {};
    }
    bool firstOk = false;
    bool lastOk = false;
    bool totalOk = false;
    const quint64 first = value.mid(6, dash - 6).toULongLong(&firstOk);
    const quint64 last = value.mid(dash + 1, slash - dash - 1)
                             .toULongLong(&lastOk);
    const quint64 total = value.mid(slash + 1).toULongLong(&totalOk);
    if (!firstOk || !lastOk || !totalOk || first > last || last >= total
        || total > kMaximumSharedImageBytes) {
        return {};
    }
    return ParsedContentRange {first, last, total};
}

bool responseChunkIsValid(const SstvHttpResponse& response,
                          quint64 offset,
                          quint64 maximumBytes,
                          quint64 expectedTotal)
{
    if (response.body.isEmpty()
        || static_cast<quint64>(response.body.size()) > maximumBytes) {
        return false;
    }
    if (response.statusCode == 206) {
        const auto range = parseContentRange(response.header("content-range"));
        return range && range->first == offset
            && range->last - range->first + 1U
                == static_cast<quint64>(response.body.size())
            && range->total == expectedTotal;
    }
    return response.statusCode == 200 && offset == 0U
        && expectedTotal <= maximumBytes
        && static_cast<quint64>(response.body.size()) == expectedTotal;
}

QByteArray idempotencyForChunk(const QString& base,
                               quint64 offset,
                               const QString& hash)
{
    QCryptographicHash digest(QCryptographicHash::Sha256);
    digest.addData(base.toUtf8());
    digest.addData(QByteArray(1, '\0'));
    digest.addData(QByteArray::number(offset));
    digest.addData(QByteArray(1, '\0'));
    digest.addData(hash.toLatin1());
    return digest.result().toHex();
}

bool responseContainsExpectedSha256(const SstvHttpResponse& response,
                                    const QString& expectedHex)
{
    const QByteArray expectedBase64 =
        QByteArray::fromHex(expectedHex.toLatin1()).toBase64();
    const QByteArray checksum = response.header("x-checksum-sha256").trimmed();
    if (!checksum.isEmpty()) {
        return checksum.toLower() == expectedHex.toLatin1();
    }
    const QByteArray amazonChecksum =
        response.header("x-amz-checksum-sha256").trimmed();
    if (!amazonChecksum.isEmpty()) {
        return amazonChecksum == expectedBase64;
    }
    const QList<QByteArray> digests = response.header("digest").split(',');
    for (QByteArray digest : digests) {
        digest = digest.trimmed();
        const qsizetype equals = digest.indexOf('=');
        if (equals > 0
            && digest.left(equals).trimmed().compare("sha-256",
                                                     Qt::CaseInsensitive) == 0) {
            QByteArray value = digest.mid(equals + 1).trimmed();
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                value = value.mid(1, value.size() - 2);
            }
            return value == expectedBase64;
        }
    }
    return false;
}

SstvShareProviderResult invalidRequest(const QString& label)
{
    return SstvShareProviderResult::failure(
        SstvShareProviderFailure::Validation,
        QStringLiteral("invalid %1 request").arg(label));
}

SstvShareProviderCapabilities restCapabilities(
    const SstvGenericRestProviderConfig& config)
{
    SstvShareProviderCapabilities capabilities;
    capabilities.chunkedUpload = true;
    capabilities.resumableUpload = true;
    capabilities.strictTlsRequired = true;
    capabilities.maximumChunkBytes = config.maximumChunkBytes;
    capabilities.maximumResponseBytes =
        static_cast<quint64>(std::max<qsizetype>(0,
            config.transport.maximumResponseBytes));
    return capabilities;
}

std::optional<bool> jsonBoolean(const QJsonObject& object,
                                const QString& name)
{
    const auto found = object.constFind(name);
    if (found == object.constEnd() || !found->isBool()) {
        return {};
    }
    return found->toBool();
}

std::optional<SstvShareProviderCapabilities> parseRestCapabilities(
    const QJsonObject& object,
    const SstvGenericRestProviderConfig& config)
{
    if (!jsonHasOnly(object,
                     {QStringLiteral("protocolVersion"),
                      QStringLiteral("recipientLookup"),
                      QStringLiteral("chunkedUpload"),
                      QStringLiteral("resumableUpload"),
                      QStringLiteral("download"),
                      QStringLiteral("acknowledgement"),
                      QStringLiteral("rejection"),
                      QStringLiteral("incomingDelete"),
                      QStringLiteral("senderBlocking"),
                      QStringLiteral("revocation"),
                      QStringLiteral("remoteDelete"),
                      QStringLiteral("incomingList"),
                      QStringLiteral("endToEndEncryptionEnvelope"),
                      QStringLiteral("strictTlsRequired"),
                      QStringLiteral("maximumChunkBytes"),
                      QStringLiteral("maximumResponseBytes")})) {
        return {};
    }
    const auto version = jsonUnsigned(
        object, QStringLiteral("protocolVersion"), true);
    const auto recipientLookup = jsonBoolean(
        object, QStringLiteral("recipientLookup"));
    const auto chunkedUpload = jsonBoolean(
        object, QStringLiteral("chunkedUpload"));
    const auto resumableUpload = jsonBoolean(
        object, QStringLiteral("resumableUpload"));
    const auto download = jsonBoolean(object, QStringLiteral("download"));
    const auto acknowledgement = jsonBoolean(
        object, QStringLiteral("acknowledgement"));
    const auto rejection = jsonBoolean(object, QStringLiteral("rejection"));
    const std::optional<bool> incomingDelete =
        object.contains(QStringLiteral("incomingDelete"))
        ? jsonBoolean(object, QStringLiteral("incomingDelete"))
        : std::optional<bool> {false};
    const std::optional<bool> senderBlocking =
        object.contains(QStringLiteral("senderBlocking"))
        ? jsonBoolean(object, QStringLiteral("senderBlocking"))
        : std::optional<bool> {false};
    const auto revocation = jsonBoolean(object, QStringLiteral("revocation"));
    const auto remoteDelete = jsonBoolean(
        object, QStringLiteral("remoteDelete"));
    const auto incomingList = jsonBoolean(
        object, QStringLiteral("incomingList"));
    const auto encryption = jsonBoolean(
        object, QStringLiteral("endToEndEncryptionEnvelope"));
    const auto strictTls = jsonBoolean(
        object, QStringLiteral("strictTlsRequired"));
    const auto maximumChunk = jsonUnsigned(
        object, QStringLiteral("maximumChunkBytes"), true);
    const auto maximumResponse = jsonUnsigned(
        object, QStringLiteral("maximumResponseBytes"), true);
    if (!version || *version != 1U || !recipientLookup || !chunkedUpload
        || !resumableUpload || !download || !acknowledgement || !rejection
        || !incomingDelete || !senderBlocking || !revocation || !remoteDelete
        || !incomingList || !encryption
        || !strictTls || !*strictTls || !maximumChunk || !maximumResponse
        || *maximumChunk > kMaximumSharedImageBytes
        || *maximumResponse == 0U
        || *maximumResponse > kMaximumSharedImageBytes
        || (*chunkedUpload && *maximumChunk == 0U)
        || (*resumableUpload && !*chunkedUpload)
        || ((*download || *incomingList) && !*maximumResponse)) {
        return {};
    }
    SstvShareProviderCapabilities capabilities;
    capabilities.recipientLookup = *recipientLookup;
    capabilities.chunkedUpload = *chunkedUpload;
    capabilities.resumableUpload = *resumableUpload;
    capabilities.download = *download;
    capabilities.acknowledgement = *acknowledgement;
    capabilities.rejection = *rejection;
    capabilities.incomingDelete = *incomingDelete;
    capabilities.senderBlocking = *senderBlocking;
    capabilities.revocation = *revocation;
    // V1 documents deletion of the upload resource as cancel/revoke. It has
    // no remote-object DELETE endpoint, so an advertised server-side
    // remoteDelete flag cannot become an executable client capability.
    capabilities.remoteDelete = false;
    capabilities.incomingList = *incomingList;
    capabilities.endToEndEncryptionEnvelope = *encryption;
    capabilities.strictTlsRequired = true;
    capabilities.maximumChunkBytes = std::min(
        *maximumChunk, config.maximumChunkBytes);
    capabilities.maximumResponseBytes = std::min(
        *maximumResponse,
        static_cast<quint64>(config.transport.maximumResponseBytes));
    if (capabilities.maximumResponseBytes == 0U
        || (capabilities.chunkedUpload
            && capabilities.maximumChunkBytes == 0U)) {
        return {};
    }
    return capabilities;
}

SstvShareProviderCapabilities webDavCapabilities(
    const SstvWebDavProviderConfig& config)
{
    SstvShareProviderCapabilities capabilities;
    capabilities.download = true;
    capabilities.remoteDelete = true;
    capabilities.strictTlsRequired = true;
    capabilities.maximumChunkBytes = kMaximumSharedImageBytes;
    capabilities.maximumResponseBytes =
        static_cast<quint64>(std::max<qsizetype>(0,
            config.transport.maximumResponseBytes));
    return capabilities;
}

SstvShareProviderCapabilities presignedCapabilities(
    const SstvPresignedPutProviderConfig& config)
{
    SstvShareProviderCapabilities capabilities;
    capabilities.strictTlsRequired = true;
    capabilities.maximumChunkBytes = kMaximumSharedImageBytes;
    capabilities.maximumResponseBytes =
        static_cast<quint64>(std::max<qsizetype>(0,
            config.transport.maximumResponseBytes));
    return capabilities;
}

} // namespace

QByteArray SstvHttpResponse::header(const QByteArray& lowercaseName) const
{
    return headers.value(lowercaseName.toLower());
}

struct SstvHttpShareProvider::Impl final
{
    static constexpr std::size_t kMaximumPendingOperations = 16U;
    static constexpr quint64 kMaximumPendingBytes =
        128U * 1024U * 1024U;

    struct Pending final
    {
        SstvShareOperationId id {0U};
        Request request;
        SstvShareProviderCompletion completion;
        std::shared_ptr<const SstvShareCredentialLease> credentialLease;
        QNetworkReply* reply {nullptr};
        std::unique_ptr<QTimer> timer;
        QByteArray response;
        int redirects {0};
        bool timedOut {false};
        bool cancelled {false};
        bool oversized {false};
        bool tlsFailed {false};
        quint64 reservedBytes {0U};
    };

    Impl(QString suppliedProviderId,
         SstvShareProviderCapabilities suppliedCapabilities,
         SstvHttpTransportOptions suppliedOptions,
         std::shared_ptr<SstvShareCredentialSource> suppliedCredentialSource,
         bool suppliedCredentialsRequired)
        : providerId(std::move(suppliedProviderId))
        , capabilities(std::move(suppliedCapabilities))
        , options(std::move(suppliedOptions))
        , credentialSource(std::move(suppliedCredentialSource))
        , credentialsRequired(suppliedCredentialsRequired)
    {
    }

    Pending* find(SstvShareOperationId id)
    {
        const auto it = pending.find(id);
        return it == pending.end() ? nullptr : it->second.get();
    }

    bool hasCapacity(quint64 reservationBytes = 0U) const noexcept
    {
        return pending.size() < kMaximumPendingOperations
            && nextOperationId != 0U
            && reservationBytes <= kMaximumPendingBytes
            && pendingReservedBytes
                <= kMaximumPendingBytes - reservationBytes;
    }

    SstvShareOperationId takeOperationId(quint64 reservationBytes) noexcept
    {
        if (!hasCapacity(reservationBytes)) {
            return 0U;
        }
        const SstvShareOperationId id = nextOperationId;
        if (pending.find(id) != pending.end()) {
            nextOperationId = 0U;
            return 0U;
        }
        if (id == std::numeric_limits<SstvShareOperationId>::max()) {
            nextOperationId = 0U;
        } else {
            ++nextOperationId;
        }
        pendingReservedBytes += reservationBytes;
        return id;
    }

    void finish(SstvHttpShareProvider* owner,
                SstvShareOperationId id,
                SstvShareProviderResult result)
    {
        const auto it = pending.find(id);
        if (it == pending.end()) {
            return;
        }
        std::unique_ptr<Pending> operation = std::move(it->second);
        pending.erase(it);
        pendingReservedBytes = operation->reservedBytes > pendingReservedBytes
            ? 0U : pendingReservedBytes - operation->reservedBytes;
        if (operation->timer) {
            operation->timer->stop();
        }
        if (operation->reply) {
            operation->reply->disconnect(owner);
            operation->reply->deleteLater();
            operation->reply = nullptr;
        }
        SstvShareProviderCompletion completion = std::move(operation->completion);
        if (completion) {
            completion(std::move(result));
        }
    }

    SstvShareProviderResult networkFailure(const Pending& operation,
                                           QNetworkReply::NetworkError error) const
    {
        const SstvShareProviderFailure category = classifyNetworkFailure(error);
        return SstvShareProviderResult::failure(
            category,
            QStringLiteral("%1 network failure %2 at %3")
                .arg(sanitizeShareDisplayText(operation.request.diagnosticLabel, 64),
                     QString::number(static_cast<int>(error)),
                     redactedShareUrl(operation.request.url)));
    }

    void issue(SstvHttpShareProvider* owner, Pending& operation)
    {
        QNetworkRequest request(operation.request.url);
        applySecureRequestPolicy(request);
        for (const auto& header : operation.request.headers) {
            request.setRawHeader(header.first, header.second);
        }
        if (operation.credentialLease
            && !operation.credentialLease->applyTo(request)) {
            const SstvShareOperationId id = operation.id;
            QMetaObject::invokeMethod(owner, [this, owner, id] {
                finish(owner, id, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Authentication,
                    QStringLiteral("credential lease could not authorize request")));
            }, Qt::QueuedConnection);
            return;
        }
        if (request.url() != operation.request.url) {
            const SstvShareOperationId id = operation.id;
            QMetaObject::invokeMethod(owner, [this, owner, id] {
                finish(owner, id, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Authentication,
                    QStringLiteral("credential lease changed the request endpoint")));
            }, Qt::QueuedConnection);
            return;
        }
        // A lease cannot relax redirect, cookie, authentication reuse or cache
        // policy even accidentally.
        applySecureRequestPolicy(request);
        if (operation.request.url.scheme().compare(
                QStringLiteral("https"), Qt::CaseInsensitive) == 0) {
            QSslConfiguration ssl = request.sslConfiguration();
            ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
            request.setSslConfiguration(ssl);
        }

        QNetworkReply* const reply = manager.sendCustomRequest(
            request, operation.request.method, operation.request.body);
        operation.reply = reply;
        reply->setReadBufferSize(operation.request.maximumResponseBytes + 1);
        const SstvShareOperationId id = operation.id;

        QObject::connect(reply, &QNetworkReply::readyRead, owner,
                         [this, id, reply] {
            Pending* const current = find(id);
            if (!current || current->reply != reply) {
                return;
            }
            const qsizetype remaining = current->request.maximumResponseBytes
                - current->response.size();
            const QByteArray bytes = reply->read(
                std::max<qsizetype>(0, remaining) + 1);
            current->response += bytes;
            if (current->response.size()
                > current->request.maximumResponseBytes) {
                current->oversized = true;
                reply->abort();
            }
        });
        QObject::connect(reply, &QNetworkReply::metaDataChanged, owner,
                         [this, id, reply] {
            Pending* const current = find(id);
            if (!current || current->reply != reply) {
                return;
            }
            bool ok = false;
            const qlonglong contentLength = reply->header(
                QNetworkRequest::ContentLengthHeader).toLongLong(&ok);
            if (ok && contentLength > current->request.maximumResponseBytes) {
                current->oversized = true;
                reply->abort();
            }
        });
        QObject::connect(reply, &QNetworkReply::uploadProgress, owner,
                         [this, id, reply](qint64 sent, qint64 total) {
            Pending* const current = find(id);
            if (!current || current->reply != reply || current->cancelled
                || !current->request.progress || sent < 0) {
                return;
            }
            const quint64 safeTotal = total >= 0
                ? static_cast<quint64>(total)
                : static_cast<quint64>(current->request.body.size());
            current->request.progress(static_cast<quint64>(sent), safeTotal);
        });
        QObject::connect(reply, &QNetworkReply::downloadProgress, owner,
                         [this, id, reply](qint64 received, qint64 total) {
            Pending* const current = find(id);
            if (!current || current->reply != reply || current->cancelled
                || !current->request.progress || received < 0) {
                return;
            }
            const quint64 safeTotal = total >= 0
                ? static_cast<quint64>(total)
                : static_cast<quint64>(
                      current->request.maximumResponseBytes);
            current->request.progress(
                static_cast<quint64>(received), safeTotal);
        });
        QObject::connect(reply, &QNetworkReply::sslErrors, owner,
                         [this, id, reply](const QList<QSslError>&) {
            Pending* const current = find(id);
            if (!current || current->reply != reply) {
                return;
            }
            current->tlsFailed = true;
            reply->abort();
        });
        QObject::connect(reply, &QNetworkReply::finished, owner,
                         [this, owner, id, reply] {
            Pending* const current = find(id);
            if (!current || current->reply != reply) {
                return;
            }
            if (reply->bytesAvailable() > 0 && !current->oversized) {
                const qsizetype remaining = current->request.maximumResponseBytes
                    - current->response.size();
                current->response += reply->read(
                    std::max<qsizetype>(0, remaining) + 1);
                current->oversized = current->response.size()
                    > current->request.maximumResponseBytes;
            }
            if (current->cancelled) {
                finish(owner, id, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Cancelled,
                    QStringLiteral("share transport operation cancelled")));
                return;
            }
            if (current->timedOut) {
                finish(owner, id, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::TransientNetwork,
                    QStringLiteral("share transport operation timed out")));
                return;
            }
            if (current->tlsFailed) {
                finish(owner, id, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::TlsValidation,
                    QStringLiteral("TLS certificate validation failed")));
                return;
            }
            if (current->oversized) {
                finish(owner, id, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Validation,
                    QStringLiteral("provider response exceeded configured bound")));
                return;
            }

            const int status = reply->attribute(
                QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QVariant redirectValue = reply->attribute(
                QNetworkRequest::RedirectionTargetAttribute);
            if (status >= 300 && status < 400 && redirectValue.isValid()) {
                const QUrl target = current->request.url.resolved(
                    redirectValue.toUrl());
                const bool methodCanRewrite = current->request.method == "GET"
                    || current->request.method == "HEAD";
                const bool statusAllowed = status == 307 || status == 308
                    || (methodCanRewrite
                        && (status == 301 || status == 302 || status == 303));
                if (!statusAllowed
                    || current->redirects >= options.maximumRedirects
                    || !owner->endpointAllowed(target)
                    || !SstvHttpShareProvider::sameOrigin(current->request.url,
                                                          target)) {
                    finish(owner, id, SstvShareProviderResult::failure(
                        SstvShareProviderFailure::PermanentProviderFailure,
                        QStringLiteral("redirect rejected by SSTV transport policy")));
                    return;
                }
                ++current->redirects;
                current->request.url = target;
                current->response.clear();
                reply->disconnect(owner);
                reply->deleteLater();
                current->reply = nullptr;
                issue(owner, *current);
                return;
            }

            SstvHttpResponse response;
            response.statusCode = status;
            response.body = std::move(current->response);
            const auto rawHeaders = reply->rawHeaderPairs();
            qsizetype headerBytes = 0;
            if (rawHeaders.size() > kMaximumHttpResponseHeaders) {
                finish(owner, id, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Validation,
                    QStringLiteral("provider response headers exceeded configured bound")));
                return;
            }
            for (const auto& header : rawHeaders) {
                if (header.first.size() > kMaximumHttpResponseHeaderBytes
                    - headerBytes) {
                    headerBytes = kMaximumHttpResponseHeaderBytes + 1;
                    break;
                }
                headerBytes += header.first.size();
                if (header.second.size() > kMaximumHttpResponseHeaderBytes
                    - headerBytes) {
                    headerBytes = kMaximumHttpResponseHeaderBytes + 1;
                    break;
                }
                headerBytes += header.second.size();
                response.headers.insert(header.first.toLower(), header.second);
            }
            if (headerBytes > kMaximumHttpResponseHeaderBytes) {
                finish(owner, id, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Validation,
                    QStringLiteral("provider response headers exceeded configured bound")));
                return;
            }
            if (!statusAccepted(current->request.acceptedStatusCodes, status)) {
                if (status > 0) {
                    finish(owner, id, httpStatusFailure(
                        status, response.header("retry-after"),
                        current->request.diagnosticLabel,
                        current->request.url));
                } else {
                    finish(owner, id, networkFailure(*current, reply->error()));
                }
                return;
            }
            if (reply->error() != QNetworkReply::NoError
                && status < 400) {
                finish(owner, id, networkFailure(*current, reply->error()));
                return;
            }

            const bool expectsJson = current->request.responseBody
                != SstvHttpResponseBody::None;
            const bool requiresJson = current->request.responseBody
                == SstvHttpResponseBody::RequiredJson;
            if (expectsJson && (!response.body.isEmpty() || requiresJson)) {
                const QByteArray contentType =
                    response.header("content-type").split(';').value(0).trimmed().toLower();
                if (contentType != "application/json") {
                    finish(owner, id, SstvShareProviderResult::failure(
                        SstvShareProviderFailure::Validation,
                        QStringLiteral("provider response has invalid content type")));
                    return;
                }
                const SstvShareBoundedJsonResult parsed = parseBoundedJsonObject(
                    response.body, current->request.maximumResponseBytes);
                if (!parsed.ok()) {
                    finish(owner, id, SstvShareProviderResult::failure(
                        SstvShareProviderFailure::Validation,
                        QStringLiteral("provider returned malformed bounded JSON")));
                    return;
                }
                response.json = parsed.object;
                response.hasJson = true;
            }

            SstvShareProviderResult result = current->request.responseHandler
                ? current->request.responseHandler(response)
                : SstvShareProviderResult::success({}, response.body);
            finish(owner, id, std::move(result));
        });
    }

    QString providerId;
    SstvShareProviderCapabilities capabilities;
    SstvHttpTransportOptions options;
    std::shared_ptr<SstvShareCredentialSource> credentialSource;
    bool credentialsRequired {false};
    QNetworkAccessManager manager;
    std::unordered_map<SstvShareOperationId, std::unique_ptr<Pending>> pending;
    SstvShareOperationId nextOperationId {1U};
    quint64 pendingReservedBytes {0U};
};

SstvHttpShareProvider::SstvHttpShareProvider(
    QString providerId,
    SstvShareProviderCapabilities capabilities,
    SstvHttpTransportOptions options,
    std::shared_ptr<SstvShareCredentialSource> credentialSource,
    bool credentialsRequired,
    QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>(std::move(providerId),
                                    std::move(capabilities),
                                    std::move(options),
                                    std::move(credentialSource),
                                    credentialsRequired))
{
}

SstvHttpShareProvider::~SstvHttpShareProvider()
{
    for (auto& entry : m_impl->pending) {
        if (entry.second->reply) {
            entry.second->reply->disconnect(this);
            entry.second->reply->abort();
        }
    }
    m_impl->pending.clear();
    m_impl->pendingReservedBytes = 0U;
}

QString SstvHttpShareProvider::providerId() const
{
    return m_impl->providerId;
}

SstvShareProviderCapabilities SstvHttpShareProvider::capabilities() const
{
    return m_impl->capabilities;
}

SstvShareAuthenticationStatus SstvHttpShareProvider::authenticationStatus() const
{
    return m_impl->credentialSource
        ? m_impl->credentialSource->status()
        : (m_impl->credentialsRequired
               ? SstvShareAuthenticationStatus::Unavailable
               : SstvShareAuthenticationStatus::NotRequired);
}

SstvShareOperationId SstvHttpShareProvider::startRequest(
    Request request,
    SstvShareProviderCompletion completion)
{
    if (QThread::currentThread() != thread()) {
        if (completion) {
            completion(SstvShareProviderResult::failure(
                SstvShareProviderFailure::PermanentProviderFailure,
                QStringLiteral("provider called from the wrong thread")));
        }
        return 0U;
    }
    // Refuse before parsing, credential acquisition, or network allocation.
    // Returning zero is the provider contract's deterministic fail-closed
    // signal; scheduling a capacity error here would itself create an
    // unbounded queue under hostile repeated calls.
    if (!completion || !m_impl->hasCapacity()) {
        return 0U;
    }
    if (!validTransportOptions(m_impl->options)
        || !validMethod(request.method) || !endpointAllowed(request.url)
        || request.acceptedStatusCodes.isEmpty()
        || request.body.size() > static_cast<qsizetype>(kMaximumSharedImageBytes)
        || request.diagnosticLabel.isEmpty()) {
        return completeSoon(invalidRequest(QStringLiteral("HTTP transport")),
                            std::move(completion));
    }
    if (request.maximumResponseBytes == 0) {
        request.maximumResponseBytes = m_impl->options.maximumResponseBytes;
    }
    if (request.maximumResponseBytes <= 0
        || request.maximumResponseBytes > m_impl->options.maximumResponseBytes) {
        return completeSoon(invalidRequest(QStringLiteral("response bound")),
                            std::move(completion));
    }
    for (const auto& header : request.headers) {
        if (!validHeader(header)) {
            return completeSoon(invalidRequest(QStringLiteral("HTTP header")),
                                std::move(completion));
        }
    }

    auto operation = std::make_unique<Impl::Pending>();
    operation->request = std::move(request);
    operation->completion = std::move(completion);
    operation->credentialLease = operation->request.explicitLease;
    if (operation->request.useProviderCredentials) {
        if (!m_impl->credentialSource) {
            return completeSoon(SstvShareProviderResult::failure(
                SstvShareProviderFailure::Authentication,
                QStringLiteral("credential source is unavailable")),
                std::move(operation->completion));
        }
        operation->credentialLease = m_impl->credentialSource->acquireLease(
            m_impl->providerId, operation->request.credentialPurpose);
        if (!operation->credentialLease) {
            return completeSoon(SstvShareProviderResult::failure(
                SstvShareProviderFailure::Authentication,
                QStringLiteral("credential lease is unavailable")),
                std::move(operation->completion));
        }
    }

    const quint64 requestBytes = static_cast<quint64>(
        operation->request.body.size());
    const quint64 responseBytes = static_cast<quint64>(
        operation->request.maximumResponseBytes);
    if (requestBytes > Impl::kMaximumPendingBytes
        || responseBytes > Impl::kMaximumPendingBytes - requestBytes) {
        return 0U;
    }
    operation->reservedBytes = requestBytes + responseBytes;
    operation->id = m_impl->takeOperationId(operation->reservedBytes);
    if (operation->id == 0U) {
        return 0U;
    }
    const SstvShareOperationId id = operation->id;
    operation->timer = std::make_unique<QTimer>();
    operation->timer->setSingleShot(true);
    QObject::connect(operation->timer.get(), &QTimer::timeout, this,
                     [this, id] {
        Impl::Pending* const current = m_impl->find(id);
        if (!current) {
            return;
        }
        current->timedOut = true;
        if (current->reply) {
            current->reply->abort();
        }
        QMetaObject::invokeMethod(this, [this, id] {
            Impl::Pending* const pending = m_impl->find(id);
            if (pending && pending->timedOut) {
                m_impl->finish(this, id, SstvShareProviderResult::failure(
                    SstvShareProviderFailure::TransientNetwork,
                    QStringLiteral("share transport operation timed out")));
            }
        }, Qt::QueuedConnection);
    });
    Impl::Pending* const raw = operation.get();
    m_impl->pending.emplace(id, std::move(operation));
    raw->timer->start(m_impl->options.timeoutMs);
    m_impl->issue(this, *raw);
    return id;
}

SstvShareOperationId SstvHttpShareProvider::completeSoon(
    SstvShareProviderResult result,
    SstvShareProviderCompletion completion)
{
    if (!completion) {
        return 0U;
    }
    if (QThread::currentThread() != thread()) {
        completion(SstvShareProviderResult::failure(
            SstvShareProviderFailure::PermanentProviderFailure,
            QStringLiteral("provider called from the wrong thread")));
        return 0U;
    }
    if (!m_impl->hasCapacity()) {
        return 0U;
    }
    auto operation = std::make_unique<Impl::Pending>();
    operation->id = m_impl->takeOperationId(0U);
    if (operation->id == 0U) {
        return 0U;
    }
    operation->completion = std::move(completion);
    const SstvShareOperationId id = operation->id;
    m_impl->pending.emplace(id, std::move(operation));
    QMetaObject::invokeMethod(this,
        [this, id, result = std::move(result)]() mutable {
            m_impl->finish(this, id, std::move(result));
        }, Qt::QueuedConnection);
    return id;
}

#if defined(DECODIUM_SSTV_PROVIDER_TESTING)
void SstvHttpShareProvider::setNextOperationIdForTesting(
    SstvShareOperationId operationId)
{
    Q_ASSERT(m_impl->pending.empty());
    m_impl->nextOperationId = operationId;
}

qsizetype SstvHttpShareProvider::pendingOperationCountForTesting() const noexcept
{
    return static_cast<qsizetype>(m_impl->pending.size());
}

quint64 SstvHttpShareProvider::pendingReservedBytesForTesting() const noexcept
{
    return m_impl->pendingReservedBytes;
}
#endif

SstvShareOperationId SstvHttpShareProvider::completeRecipientSoon(
    SstvShareProviderResult result,
    SstvShareRecipientRecord recipient,
    SstvShareRecipientCompletion completion)
{
    return completeSoon(std::move(result),
        [completion = std::move(completion),
         recipient = std::move(recipient)](SstvShareProviderResult value) mutable {
            if (completion) {
                completion(std::move(value), std::move(recipient));
            }
        });
}

SstvShareOperationId SstvHttpShareProvider::completeIncomingSoon(
    SstvShareProviderResult result,
    QVector<SstvShareIncomingItem> items,
    SstvShareIncomingCompletion completion)
{
    return completeSoon(std::move(result),
        [completion = std::move(completion),
         items = std::move(items)](SstvShareProviderResult value) mutable {
            if (completion) {
                completion(std::move(value), std::move(items));
            }
        });
}

bool SstvHttpShareProvider::endpointAllowed(const QUrl& url) const
{
    const QByteArray encoded = url.toEncoded(QUrl::FullyEncoded);
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty()
        || url.hasFragment() || encoded.isEmpty()
        || encoded.size() > kMaximumTransportUrlBytes) {
        return false;
    }
    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0) {
        return true;
    }
#if DECODIUM_SSTV_ALLOW_INSECURE_LOCAL_TEST_TRANSPORT
    return m_impl->options.allowInsecureLocalhostForTests
        && url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0
        && isLoopbackHost(url.host());
#else
    Q_UNUSED(url)
    return false;
#endif
}

const SstvHttpTransportOptions&
SstvHttpShareProvider::transportOptions() const noexcept
{
    return m_impl->options;
}

bool SstvHttpShareProvider::sameOrigin(const QUrl& first, const QUrl& second)
{
    return first.scheme().compare(second.scheme(), Qt::CaseInsensitive) == 0
        && first.host().compare(second.host(), Qt::CaseInsensitive) == 0
        && effectivePort(first) == effectivePort(second);
}

QString SstvHttpShareProvider::makeOpaqueUploadId(
    const QString& prefix,
    const QString& stableIdempotencyBinding)
{
    const QString safePrefix = isSafeShareIdentifier(prefix, 32)
        ? prefix
        : QStringLiteral("upload");
    if (isLowercaseSha256(stableIdempotencyBinding)) {
        return safePrefix + QLatin1Char(':') + stableIdempotencyBinding;
    }
    return safePrefix + QLatin1Char(':')
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QByteArray SstvHttpShareProvider::sha256Hex(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

QByteArray SstvHttpShareProvider::sha256Base64(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toBase64();
}

void SstvHttpShareProvider::setCapabilities(
    const SstvShareProviderCapabilities& capabilities)
{
    if (QThread::currentThread() == thread()) {
        m_impl->capabilities = capabilities;
    }
}

bool SstvHttpShareProvider::cancelOperation(SstvShareOperationId operationId)
{
    if (QThread::currentThread() != thread()) {
        return false;
    }
    Impl::Pending* const operation = m_impl->find(operationId);
    if (!operation) {
        return false;
    }
    operation->cancelled = true;
    if (operation->reply) {
        operation->reply->abort();
    }
    QMetaObject::invokeMethod(this, [this, operationId] {
        Impl::Pending* const pending = m_impl->find(operationId);
        if (pending && pending->cancelled) {
            m_impl->finish(this, operationId, SstvShareProviderResult::failure(
                SstvShareProviderFailure::Cancelled,
                QStringLiteral("share transport operation cancelled")));
        }
    }, Qt::QueuedConnection);
    return true;
}

SstvShareOperationId SstvHttpShareProvider::lookupRecipientAsync(
    const QString&, SstvShareRecipientCompletion completion)
{
    return completeRecipientSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("recipient lookup is not supported by this transport")),
        {}, std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::createUploadAsync(
    const SstvShareManifestV1&, const QString&,
    SstvShareProviderCompletion completion)
{
    return completeSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("create upload is not supported by this transport")),
        std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::uploadChunkAsync(
    const SstvShareUploadHandle&, quint64, const QByteArray&, const QString&,
    SstvShareProgressCallback, SstvShareProviderCompletion completion)
{
    return completeSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("upload is not supported by this transport")),
        std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::resumeUploadAsync(
    const SstvShareUploadHandle&, SstvShareProviderCompletion completion)
{
    return completeSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("resume is not supported by this transport")),
        std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::completeUploadAsync(
    const SstvShareUploadHandle&, const QString&,
    SstvShareProviderCompletion completion)
{
    return completeSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("completion is not supported by this transport")),
        std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::cancelUploadAsync(
    const SstvShareUploadHandle&, SstvShareProviderCompletion completion)
{
    return completeSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("remote cancellation is not supported by this transport")),
        std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::queryStatusAsync(
    const SstvShareUploadHandle&, SstvShareProviderCompletion completion)
{
    return completeSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("status query is not supported by this transport")),
        std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::downloadAsync(
    const QString&, quint64, quint64, SstvShareProgressCallback,
    SstvShareProviderCompletion completion)
{
    return completeSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("download is not supported by this transport")),
        std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::acknowledgeAsync(
    const QString&, SstvShareProviderCompletion completion)
{
    return completeSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("acknowledgement is not supported by this transport")),
        std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::rejectAsync(
    const QString&, SstvShareProviderCompletion completion)
{
    return completeSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("rejection is not supported by this transport")),
        std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::deleteIncomingAsync(
    const QString&, SstvShareProviderCompletion completion)
{
    return completeSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("incoming deletion is not supported by this transport")),
        std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::blockSenderAsync(
    const QString&, SstvShareProviderCompletion completion)
{
    return completeSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("sender blocking is not supported by this transport")),
        std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::revokeAsync(
    const SstvShareUploadHandle&, SstvShareProviderCompletion completion)
{
    return completeSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("revocation is not supported by this transport")),
        std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::deleteRemoteObjectAsync(
    const QString&, SstvShareProviderCompletion completion)
{
    return completeSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("remote delete is not supported by this transport")),
        std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::refreshCredentialsAsync(
    SstvShareProviderCompletion completion)
{
    const auto status = authenticationStatus();
    return completeSoon(
        status == SstvShareAuthenticationStatus::Authenticated
                || status == SstvShareAuthenticationStatus::NotRequired
            ? SstvShareProviderResult::success()
            : SstvShareProviderResult::failure(
                SstvShareProviderFailure::Authentication,
                QStringLiteral("credentials require external refresh")),
        std::move(completion));
}

SstvShareOperationId SstvHttpShareProvider::listIncomingAsync(
    qsizetype, SstvShareIncomingCompletion completion)
{
    return completeIncomingSoon(SstvShareProviderResult::failure(
        SstvShareProviderFailure::PermanentProviderFailure,
        QStringLiteral("incoming list is not supported by this transport")),
        {}, std::move(completion));
}

// Generic HTTPS REST ---------------------------------------------------------

struct SstvGenericRestShareProvider::Impl final
{
    struct Session final
    {
        QString remoteId;
        QString opaqueId;
        QString idempotencyKey;
        QString payloadSha256;
        quint64 totalBytes {0U};
        quint64 committedBytes {0U};
        QString remoteObjectId;
        QDateTime expiresUtc;
        bool completed {false};
        bool cancelled {false};
    };

    struct TerminalRecord final
    {
        QString originalOpaqueId;
        QString idempotencyKey;
        SstvShareUploadHandle result;
        QDateTime expiresUtc;
        quint64 sequence {0U};
        bool cancelled {false};
    };

    SstvGenericRestProviderConfig config;
    bool valid {false};
    bool capabilitiesVerified {false};
    QHash<QString, Session> sessions;
    QHash<QString, QString> handleByIdempotency;
    QHash<QString, TerminalRecord> terminalByOpaqueId;
    QHash<QString, QString> terminalOpaqueByIdempotency;
    QHash<QString, SstvShareIncomingItem> incoming;
    qsizetype pendingCreates {0};
    quint64 nextTerminalSequence {1U};

    void removeTerminal(const QString& opaqueId)
    {
        const auto record = terminalByOpaqueId.find(opaqueId);
        if (record == terminalByOpaqueId.end()) {
            return;
        }
        terminalOpaqueByIdempotency.remove(record->idempotencyKey);
        terminalByOpaqueId.erase(record);
    }

    void removeActive(const QString& opaqueId)
    {
        const auto session = sessions.find(opaqueId);
        if (session == sessions.end()) {
            return;
        }
        handleByIdempotency.remove(session->idempotencyKey);
        sessions.erase(session);
    }

    void trimTerminals()
    {
        while (terminalByOpaqueId.size() > config.maximumTerminalRecords) {
            auto oldest = terminalByOpaqueId.end();
            for (auto record = terminalByOpaqueId.begin();
                 record != terminalByOpaqueId.end(); ++record) {
                if (oldest == terminalByOpaqueId.end()
                    || record->sequence < oldest->sequence) {
                    oldest = record;
                }
            }
            if (oldest == terminalByOpaqueId.end()) {
                break;
            }
            removeTerminal(oldest.key());
        }
    }

    void purgeExpired(const QDateTime& nowUtc)
    {
        QStringList activeExpired;
        for (auto session = sessions.cbegin(); session != sessions.cend();
             ++session) {
            if (session->expiresUtc <= nowUtc) {
                activeExpired.push_back(session.key());
            }
        }
        for (const QString& opaqueId : activeExpired) {
            removeActive(opaqueId);
        }
        QStringList terminalExpired;
        for (auto record = terminalByOpaqueId.cbegin();
             record != terminalByOpaqueId.cend(); ++record) {
            if (record->expiresUtc <= nowUtc) {
                terminalExpired.push_back(record.key());
            }
        }
        for (const QString& opaqueId : terminalExpired) {
            removeTerminal(opaqueId);
        }
    }

    void recordTerminal(const QString& opaqueId,
                        SstvShareUploadHandle result,
                        bool cancelled)
    {
        const auto session = sessions.constFind(opaqueId);
        if (session == sessions.constEnd()) {
            return;
        }
        TerminalRecord record;
        record.originalOpaqueId = session->opaqueId;
        record.idempotencyKey = session->idempotencyKey;
        record.result = std::move(result);
        record.expiresUtc = session->expiresUtc;
        record.sequence = nextTerminalSequence++;
        record.cancelled = cancelled;
        removeActive(opaqueId);
        terminalOpaqueByIdempotency.insert(record.idempotencyKey,
                                           record.originalOpaqueId);
        terminalByOpaqueId.insert(record.originalOpaqueId, std::move(record));
        trimTerminals();
    }

    const TerminalRecord* terminalForIdempotency(
        const QString& idempotencyKey) const
    {
        const auto opaqueId = terminalOpaqueByIdempotency.constFind(
            idempotencyKey);
        if (opaqueId == terminalOpaqueByIdempotency.constEnd()) {
            return nullptr;
        }
        const auto record = terminalByOpaqueId.constFind(*opaqueId);
        return record == terminalByOpaqueId.constEnd() ? nullptr : &*record;
    }
};

SstvGenericRestShareProvider::SstvGenericRestShareProvider(
    SstvGenericRestProviderConfig config,
    std::shared_ptr<SstvShareCredentialSource> credentialSource,
    QObject* parent)
    : SstvHttpShareProvider(config.providerId, restCapabilities(config),
                            config.transport, std::move(credentialSource),
                            config.credentialsRequired, parent)
    , m_rest(std::make_unique<Impl>())
{
    m_rest->config = std::move(config);
    const auto& value = m_rest->config;
    m_rest->valid = isSafeShareIdentifier(value.providerId)
        && endpointAllowed(value.baseUrl)
        && value.baseUrl.query().isEmpty() && !value.baseUrl.hasFragment()
        && validTransportOptions(value.transport)
        && value.maximumChunkBytes > 0U
        && value.maximumChunkBytes <= kMaximumSharedImageBytes
        && validSessionBounds(value.maximumActiveSessions,
                              value.maximumTerminalRecords)
        && validEndpointPath(value.createUploadPath, false)
        && validEndpointPath(value.uploadChunkPathTemplate, true)
        && validEndpointPath(value.queryStatusPathTemplate, true)
        && validEndpointPath(value.completeUploadPathTemplate, true)
        && validEndpointPath(value.cancelUploadPathTemplate, true)
        && validEndpointTemplate(value.capabilitiesPath,
                                 QStringLiteral("{unused}"), false)
        && validEndpointTemplate(value.recipientLookupPathTemplate,
                                 QStringLiteral("{recipientId}"), true)
        && validEndpointTemplate(value.incomingListPath,
                                 QStringLiteral("{unused}"), false)
        && validEndpointTemplate(value.downloadPathTemplate,
                                 QStringLiteral("{incomingId}"), true)
        && validEndpointTemplate(value.acknowledgePathTemplate,
                                 QStringLiteral("{incomingId}"), true)
        && validEndpointTemplate(value.rejectPathTemplate,
                                 QStringLiteral("{incomingId}"), true)
        && validEndpointTemplate(value.deleteIncomingPathTemplate,
                                 QStringLiteral("{incomingId}"), true)
        && validEndpointTemplate(value.blockSenderPathTemplate,
                                 QStringLiteral("{senderId}"), true);
}

SstvGenericRestShareProvider::~SstvGenericRestShareProvider() = default;

bool SstvGenericRestShareProvider::isConfigurationValid() const noexcept
{
    return m_rest->valid;
}

SstvShareOperationId SstvGenericRestShareProvider::refreshCapabilitiesAsync(
    SstvShareProviderCompletion completion)
{
    if (!m_rest->valid) {
        return completeSoon(invalidRequest(QStringLiteral("REST capabilities")),
                            std::move(completion));
    }
    Request request;
    request.method = "GET";
    request.url = endpointWithPath(m_rest->config.baseUrl,
                                   m_rest->config.capabilitiesPath);
    request.headers = {{"Accept", "application/json"}};
    request.acceptedStatusCodes = {200};
    request.responseBody = SstvHttpResponseBody::RequiredJson;
    request.diagnosticLabel = QStringLiteral("REST capability discovery");
    request.useProviderCredentials = m_rest->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::CapabilityDiscovery;
    request.responseHandler = [this](const SstvHttpResponse& response) {
        const auto parsed = parseRestCapabilities(response.json, m_rest->config);
        if (!parsed) {
            m_rest->capabilitiesVerified = false;
            setCapabilities(restCapabilities(m_rest->config));
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("REST provider capabilities were malformed or unsafe"));
        }
        m_rest->capabilitiesVerified = true;
        setCapabilities(*parsed);
        return SstvShareProviderResult::success({}, response.body);
    };
    return startRequest(
        std::move(request),
        [this, completion = std::move(completion)](
            SstvShareProviderResult result) mutable {
            if (!result.ok()) {
                m_rest->capabilitiesVerified = false;
                m_rest->incoming.clear();
                setCapabilities(restCapabilities(m_rest->config));
            }
            if (completion) {
                completion(std::move(result));
            }
        });
}

SstvShareOperationId SstvGenericRestShareProvider::lookupRecipientAsync(
    const QString& stableRecipientId,
    SstvShareRecipientCompletion completion)
{
    if (!m_rest->valid || !m_rest->capabilitiesVerified
        || !capabilities().recipientLookup
        || !isSafeShareIdentifier(stableRecipientId)) {
        return completeRecipientSoon(
            invalidRequest(QStringLiteral("REST recipient lookup")), {},
            std::move(completion));
    }
    auto recipient = std::make_shared<SstvShareRecipientRecord>();
    Request request;
    request.method = "GET";
    request.url = endpointWithPath(
        m_rest->config.baseUrl,
        pathForIdentifier(m_rest->config.recipientLookupPathTemplate,
                          QStringLiteral("{recipientId}"), stableRecipientId));
    request.headers = {{"Accept", "application/json"}};
    request.acceptedStatusCodes = {200};
    request.responseBody = SstvHttpResponseBody::RequiredJson;
    request.diagnosticLabel = QStringLiteral("REST recipient lookup");
    request.useProviderCredentials = m_rest->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::RecipientLookup;
    request.responseHandler = [this, stableRecipientId, recipient](
                                  const SstvHttpResponse& response) {
        if (!jsonHasOnly(response.json,
                         {QStringLiteral("providerId"),
                          QStringLiteral("stableRecipientId"),
                          QStringLiteral("displayCallsign"),
                          QStringLiteral("displayName"),
                          QStringLiteral("publicEncryptionKey"),
                          QStringLiteral("publicKeyFingerprint"),
                          QStringLiteral("verification"),
                          QStringLiteral("trust"),
                          QStringLiteral("lastUsedUtc")})) {
            return invalidRequest(QStringLiteral("REST recipient response"));
        }
        const auto string = [&response](const QString& name)
            -> std::optional<QString> {
            const auto value = response.json.constFind(name);
            if (value == response.json.constEnd() || !value->isString()) {
                return {};
            }
            return value->toString();
        };
        const auto provider = string(QStringLiteral("providerId"));
        const auto stable = string(QStringLiteral("stableRecipientId"));
        const auto callsign = string(QStringLiteral("displayCallsign"));
        const auto name = string(QStringLiteral("displayName"));
        const auto publicKey = string(QStringLiteral("publicEncryptionKey"));
        const auto fingerprint = string(QStringLiteral("publicKeyFingerprint"));
        const auto verification = string(QStringLiteral("verification"));
        const auto trust = string(QStringLiteral("trust"));
        if (!provider || !stable || !callsign || !name || !publicKey
            || !fingerprint || !verification || !trust
            || *provider != providerId() || *stable != stableRecipientId) {
            return invalidRequest(QStringLiteral("REST recipient response"));
        }
        SstvShareRecipientRecord parsed;
        parsed.providerId = *provider;
        parsed.stableRecipientId = *stable;
        parsed.displayCallsign = *callsign;
        parsed.displayName = *name;
        parsed.publicEncryptionKey = *publicKey;
        parsed.publicKeyFingerprint = *fingerprint;
        if (*verification == QStringLiteral("provider-verified")) {
            parsed.verification = SstvShareRecipientVerification::ProviderVerified;
        } else if (*verification == QStringLiteral("user-verified")) {
            parsed.verification = SstvShareRecipientVerification::UserVerified;
        } else if (*verification != QStringLiteral("unknown")) {
            return invalidRequest(QStringLiteral("REST recipient response"));
        }
        if (*trust == QStringLiteral("trusted")) {
            parsed.trust = SstvShareRecipientTrust::Trusted;
        } else if (*trust == QStringLiteral("blocked")) {
            parsed.trust = SstvShareRecipientTrust::Blocked;
        } else if (*trust != QStringLiteral("unknown")) {
            return invalidRequest(QStringLiteral("REST recipient response"));
        }
        if (response.json.contains(QStringLiteral("lastUsedUtc"))) {
            const auto timestamp = exactUtcTimestamp(
                response.json.value(QStringLiteral("lastUsedUtc")));
            if (!timestamp) {
                return invalidRequest(QStringLiteral("REST recipient response"));
            }
            parsed.lastUsedUtc = *timestamp;
        }
        if (!validateShareRecipientRecord(parsed).ok()) {
            return invalidRequest(QStringLiteral("REST recipient response"));
        }
        *recipient = std::move(parsed);
        return SstvShareProviderResult::success();
    };
    return startRequest(
        std::move(request),
        [completion = std::move(completion), recipient](
            SstvShareProviderResult result) mutable {
            if (completion) {
                completion(std::move(result), std::move(*recipient));
            }
        });
}

SstvShareOperationId SstvGenericRestShareProvider::createUploadAsync(
    const SstvShareManifestV1& manifest,
    const QString& idempotencyKey,
    SstvShareProviderCompletion completion)
{
    m_rest->purgeExpired(QDateTime::currentDateTimeUtc());
    if (!m_rest->valid || manifest.providerId != providerId()
        || !manifest.validate(true, QDateTime::currentDateTimeUtc()).ok()
        || !isLowercaseSha256(idempotencyKey)
        || idempotencyKey != SstvShareTransfer::deriveIdempotencyKey(manifest)) {
        return completeSoon(invalidRequest(QStringLiteral("REST create")),
                            std::move(completion));
    }
    const auto existing = m_rest->handleByIdempotency.constFind(idempotencyKey);
    if (existing != m_rest->handleByIdempotency.constEnd()) {
        const auto session = m_rest->sessions.constFind(*existing);
        if (session != m_rest->sessions.constEnd()) {
            return completeSoon(SstvShareProviderResult::success(
                {session->opaqueId, session->committedBytes}),
                std::move(completion));
        }
    }
    if (const auto* terminal = m_rest->terminalForIdempotency(idempotencyKey)) {
        return completeSoon(SstvShareProviderResult::success(terminal->result),
                            std::move(completion));
    }
    if (m_rest->sessions.size() + m_rest->pendingCreates
        >= m_rest->config.maximumActiveSessions) {
        return completeSoon(SstvShareProviderResult::failure(
            SstvShareProviderFailure::ProviderUnavailable,
            QStringLiteral("REST upload session capacity is full")),
            std::move(completion));
    }
    SstvShareValidationError encodingError;
    const QByteArray body = manifest.toCanonicalJson(&encodingError);
    if (!encodingError.ok() || body.isEmpty()) {
        return completeSoon(invalidRequest(QStringLiteral("REST manifest")),
                            std::move(completion));
    }

    Request request;
    request.method = "POST";
    request.url = endpointWithPath(m_rest->config.baseUrl,
                                   m_rest->config.createUploadPath);
    request.body = body;
    request.headers = {
        {"Accept", "application/json"},
        {"Content-Type", "application/json"},
        {"Idempotency-Key", idempotencyKey.toLatin1()},
    };
    request.acceptedStatusCodes = {200, 201};
    request.responseBody = SstvHttpResponseBody::RequiredJson;
    request.diagnosticLabel = QStringLiteral("REST create upload");
    request.useProviderCredentials = m_rest->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::CreateUpload;
    request.responseHandler = [this, idempotencyKey, manifest](
                                  const SstvHttpResponse& response) {
        if (!jsonHasOnly(response.json,
                         {QStringLiteral("uploadId"),
                          QStringLiteral("committedBytes")})
            || !response.json.value(QStringLiteral("uploadId")).isString()) {
            return invalidRequest(QStringLiteral("REST create response"));
        }
        const QString remoteId =
            response.json.value(QStringLiteral("uploadId")).toString();
        const auto committed = jsonUnsigned(response.json,
                                            QStringLiteral("committedBytes"),
                                            false);
        if (!isSafeShareIdentifier(remoteId) || !committed
            || *committed > manifest.byteSize) {
            return invalidRequest(QStringLiteral("REST create response"));
        }
        const auto existing = m_rest->handleByIdempotency.constFind(idempotencyKey);
        if (existing != m_rest->handleByIdempotency.constEnd()) {
            auto session = m_rest->sessions.find(*existing);
            if (session != m_rest->sessions.end()) {
                if (session->remoteId != remoteId
                    || session->totalBytes != manifest.byteSize
                    || session->payloadSha256 != manifest.sha256) {
                    return SstvShareProviderResult::failure(
                        SstvShareProviderFailure::Conflict,
                        QStringLiteral("REST idempotency response changed identity"));
                }
                session->committedBytes = std::max(session->committedBytes,
                                                   *committed);
                return SstvShareProviderResult::success(
                    {session->opaqueId, session->committedBytes});
            }
        }
        Impl::Session session;
        session.remoteId = remoteId;
        session.opaqueId = makeOpaqueUploadId(
            QStringLiteral("rest-upload"), idempotencyKey);
        session.idempotencyKey = idempotencyKey;
        session.payloadSha256 = manifest.sha256;
        session.totalBytes = manifest.byteSize;
        session.committedBytes = *committed;
        session.expiresUtc = manifest.expiresUtc;
        m_rest->handleByIdempotency.insert(idempotencyKey, session.opaqueId);
        m_rest->sessions.insert(session.opaqueId, session);
        return SstvShareProviderResult::success(
            {session.opaqueId, session.committedBytes});
    };
    ++m_rest->pendingCreates;
    auto reservation = std::make_shared<bool>(true);
    const SstvShareOperationId operation = startRequest(
        std::move(request),
        [this, reservation, completion = std::move(completion)](
            SstvShareProviderResult result) mutable {
            if (*reservation) {
                *reservation = false;
                --m_rest->pendingCreates;
            }
            if (completion) {
                completion(std::move(result));
            }
        });
    if (operation == 0U && *reservation) {
        *reservation = false;
        --m_rest->pendingCreates;
    }
    return operation;
}

SstvShareOperationId SstvGenericRestShareProvider::uploadChunkAsync(
    const SstvShareUploadHandle& handle,
    quint64 offset,
    const QByteArray& chunk,
    const QString& chunkSha256,
    SstvShareProgressCallback progress,
    SstvShareProviderCompletion completion)
{
    m_rest->purgeExpired(QDateTime::currentDateTimeUtc());
    auto session = m_rest->sessions.find(handle.opaqueId);
    const QByteArray actualHash = sha256Hex(chunk);
    if (!m_rest->valid || session == m_rest->sessions.end()
        || session->cancelled || session->completed
        || offset != session->committedBytes || handle.committedBytes != offset
        || chunk.isEmpty()
        || static_cast<quint64>(chunk.size()) > m_rest->config.maximumChunkBytes
        || !isLowercaseSha256(chunkSha256)
        || actualHash != chunkSha256.toLatin1()
        || offset > session->totalBytes
        || static_cast<quint64>(chunk.size()) > session->totalBytes - offset) {
        return completeSoon(
            actualHash != chunkSha256.toLatin1()
                ? SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Integrity,
                    QStringLiteral("REST upload chunk hash mismatch"))
                : invalidRequest(QStringLiteral("REST upload chunk")),
            std::move(completion));
    }
    const QString opaqueId = session->opaqueId;
    const quint64 expectedCommitted = offset
        + static_cast<quint64>(chunk.size());
    Request request;
    request.method = "PUT";
    request.url = endpointWithPath(
        m_rest->config.baseUrl,
        pathForUpload(m_rest->config.uploadChunkPathTemplate,
                      session->remoteId));
    request.body = chunk;
    request.headers = {
        {"Accept", "application/json"},
        {"Content-Type", "application/octet-stream"},
        {"Digest", QByteArray("sha-256=") + sha256Base64(chunk)},
        {"X-Content-SHA256", chunkSha256.toLatin1()},
        {"Upload-Offset", QByteArray::number(offset)},
        {"Idempotency-Key", idempotencyForChunk(
             session->idempotencyKey, offset, chunkSha256)},
    };
    request.acceptedStatusCodes = {200, 201, 204};
    request.responseBody = SstvHttpResponseBody::OptionalJson;
    request.diagnosticLabel = QStringLiteral("REST upload chunk");
    request.useProviderCredentials = m_rest->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::UploadContent;
    request.progress = std::move(progress);
    request.responseHandler = [this, opaqueId, expectedCommitted](
                                  const SstvHttpResponse& response) {
        quint64 committed = expectedCommitted;
        if (response.hasJson) {
            if (!jsonHasOnly(response.json,
                             {QStringLiteral("committedBytes")})) {
                return invalidRequest(QStringLiteral("REST upload response"));
            }
            const auto parsed = jsonUnsigned(response.json,
                                             QStringLiteral("committedBytes"),
                                             true);
            if (!parsed || *parsed != expectedCommitted) {
                return SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Integrity,
                    QStringLiteral("REST committed offset was not verified"));
            }
            committed = *parsed;
        }
        auto current = m_rest->sessions.find(opaqueId);
        if (current == m_rest->sessions.end() || committed > current->totalBytes) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::NotFound,
                QStringLiteral("REST upload session disappeared"));
        }
        current->committedBytes = committed;
        return SstvShareProviderResult::success(
            {current->opaqueId, current->committedBytes});
    };
    return startRequest(std::move(request), std::move(completion));
}

SstvShareOperationId SstvGenericRestShareProvider::queryStatusAsync(
    const SstvShareUploadHandle& handle,
    SstvShareProviderCompletion completion)
{
    m_rest->purgeExpired(QDateTime::currentDateTimeUtc());
    const auto session = m_rest->sessions.constFind(handle.opaqueId);
    if (!m_rest->valid) {
        return completeSoon(invalidRequest(QStringLiteral("REST status")),
                            std::move(completion));
    }
    if (session == m_rest->sessions.constEnd()) {
        const auto terminal = m_rest->terminalByOpaqueId.constFind(
            handle.opaqueId);
        if (terminal != m_rest->terminalByOpaqueId.constEnd()
            && !terminal->cancelled) {
            return completeSoon(
                SstvShareProviderResult::success(terminal->result),
                std::move(completion));
        }
        return completeSoon(invalidRequest(QStringLiteral("REST status")),
                            std::move(completion));
    }
    const QString opaqueId = session->opaqueId;
    Request request;
    request.method = "GET";
    request.url = endpointWithPath(
        m_rest->config.baseUrl,
        pathForUpload(m_rest->config.queryStatusPathTemplate,
                      session->remoteId));
    request.headers = {{"Accept", "application/json"}};
    request.acceptedStatusCodes = {200};
    request.responseBody = SstvHttpResponseBody::RequiredJson;
    request.diagnosticLabel = QStringLiteral("REST query status");
    request.useProviderCredentials = m_rest->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::QueryStatus;
    request.responseHandler = [this, opaqueId](const SstvHttpResponse& response) {
        if (!jsonHasOnly(response.json,
                         {QStringLiteral("committedBytes")})) {
            return invalidRequest(QStringLiteral("REST status response"));
        }
        const auto committed = jsonUnsigned(response.json,
                                            QStringLiteral("committedBytes"),
                                            true);
        auto current = m_rest->sessions.find(opaqueId);
        if (!committed || current == m_rest->sessions.end()
            || *committed > current->totalBytes) {
            return invalidRequest(QStringLiteral("REST status response"));
        }
        current->committedBytes = *committed;
        return SstvShareProviderResult::success(
            {current->opaqueId, current->committedBytes});
    };
    return startRequest(std::move(request), std::move(completion));
}

SstvShareOperationId SstvGenericRestShareProvider::resumeUploadAsync(
    const SstvShareUploadHandle& handle,
    SstvShareProviderCompletion completion)
{
    return queryStatusAsync(handle, std::move(completion));
}

SstvShareOperationId SstvGenericRestShareProvider::completeUploadAsync(
    const SstvShareUploadHandle& handle,
    const QString& idempotencyKey,
    SstvShareProviderCompletion completion)
{
    m_rest->purgeExpired(QDateTime::currentDateTimeUtc());
    const auto session = m_rest->sessions.constFind(handle.opaqueId);
    if (!m_rest->valid) {
        return completeSoon(invalidRequest(QStringLiteral("REST completion")),
                            std::move(completion));
    }
    if (session == m_rest->sessions.constEnd()) {
        const auto terminal = m_rest->terminalByOpaqueId.constFind(
            handle.opaqueId);
        if (terminal != m_rest->terminalByOpaqueId.constEnd()
            && !terminal->cancelled
            && terminal->idempotencyKey == idempotencyKey) {
            return completeSoon(
                SstvShareProviderResult::success(terminal->result),
                std::move(completion));
        }
        return completeSoon(invalidRequest(QStringLiteral("REST completion")),
                            std::move(completion));
    }
    if (session->cancelled || idempotencyKey != session->idempotencyKey
        || handle.committedBytes != session->committedBytes) {
        return completeSoon(invalidRequest(QStringLiteral("REST completion")),
                            std::move(completion));
    }
    if (session->completed) {
        return completeSoon(SstvShareProviderResult::success(
            {session->remoteObjectId, session->committedBytes}),
            std::move(completion));
    }
    if (session->committedBytes != session->totalBytes) {
        return completeSoon(SstvShareProviderResult::failure(
            SstvShareProviderFailure::Integrity,
            QStringLiteral("REST completion requires all verified bytes")),
            std::move(completion));
    }
    QJsonObject bodyObject {
        {QStringLiteral("byteSize"), static_cast<double>(session->totalBytes)},
        {QStringLiteral("sha256"), session->payloadSha256},
    };
    SstvShareValidationError encodingError;
    const QByteArray body = canonicalJson(bodyObject, &encodingError);
    if (!encodingError.ok()) {
        return completeSoon(invalidRequest(QStringLiteral("REST completion")),
                            std::move(completion));
    }
    const QString opaqueId = session->opaqueId;
    Request request;
    request.method = "POST";
    request.url = endpointWithPath(
        m_rest->config.baseUrl,
        pathForUpload(m_rest->config.completeUploadPathTemplate,
                      session->remoteId));
    request.body = body;
    request.headers = {
        {"Accept", "application/json"},
        {"Content-Type", "application/json"},
        {"Idempotency-Key", idempotencyKey.toLatin1()},
    };
    request.acceptedStatusCodes = {200, 201, 204};
    request.responseBody = SstvHttpResponseBody::OptionalJson;
    request.diagnosticLabel = QStringLiteral("REST complete upload");
    request.useProviderCredentials = m_rest->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::CompleteUpload;
    request.responseHandler = [this, opaqueId](const SstvHttpResponse& response) {
        auto current = m_rest->sessions.find(opaqueId);
        if (current == m_rest->sessions.end()) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::NotFound,
                QStringLiteral("REST upload session disappeared"));
        }
        if (m_rest->config.requireServerSha256 && !response.hasJson) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::Integrity,
                QStringLiteral("REST server SHA-256 was not reported"));
        }
        QString remoteObjectId;
        if (response.hasJson) {
            if (!jsonHasOnly(response.json,
                             {QStringLiteral("remoteObjectId"),
                              QStringLiteral("byteSize"),
                              QStringLiteral("sha256")})
                || !response.json.value(
                        QStringLiteral("remoteObjectId")).isString()
                || !isSafeShareIdentifier(response.json.value(
                        QStringLiteral("remoteObjectId")).toString())) {
                return invalidRequest(QStringLiteral("REST completion response"));
            }
            remoteObjectId = response.json.value(
                QStringLiteral("remoteObjectId")).toString();
            const bool hasSha256 = response.json.contains(QStringLiteral("sha256"));
            const bool hasByteSize = response.json.contains(QStringLiteral("byteSize"));
            if (hasSha256 != hasByteSize
                || (m_rest->config.requireServerSha256 && !hasSha256)) {
                return SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Integrity,
                    QStringLiteral("REST server integrity metadata is incomplete"));
            }
            if (hasSha256) {
                const auto byteSize = jsonUnsigned(response.json,
                                                   QStringLiteral("byteSize"),
                                                   true);
                if (!response.json.value(QStringLiteral("sha256")).isString()
                    || response.json.value(QStringLiteral("sha256")).toString()
                        != current->payloadSha256
                    || !byteSize || *byteSize != current->totalBytes) {
                    return SstvShareProviderResult::failure(
                        SstvShareProviderFailure::Integrity,
                        QStringLiteral("REST server integrity metadata mismatch"));
                }
            }
        }
        const SstvShareUploadHandle result {
            remoteObjectId, current->committedBytes};
        m_rest->recordTerminal(opaqueId, result, false);
        return SstvShareProviderResult::success(result);
    };
    return startRequest(std::move(request), std::move(completion));
}

SstvShareOperationId SstvGenericRestShareProvider::revokeAsync(
    const SstvShareUploadHandle& handle,
    SstvShareProviderCompletion completion)
{
    if (!m_rest->valid || !m_rest->capabilitiesVerified
        || !capabilities().revocation) {
        return completeSoon(SstvShareProviderResult::failure(
            SstvShareProviderFailure::PermanentProviderFailure,
            QStringLiteral("REST revocation is not a verified capability")),
            std::move(completion));
    }
    return cancelUploadAsync(handle, std::move(completion));
}

SstvShareOperationId SstvGenericRestShareProvider::cancelUploadAsync(
    const SstvShareUploadHandle& handle,
    SstvShareProviderCompletion completion)
{
    m_rest->purgeExpired(QDateTime::currentDateTimeUtc());
    const auto session = m_rest->sessions.constFind(handle.opaqueId);
    if (!m_rest->valid) {
        return completeSoon(invalidRequest(QStringLiteral("REST cancellation")),
                            std::move(completion));
    }
    if (session == m_rest->sessions.constEnd()) {
        const auto terminal = m_rest->terminalByOpaqueId.constFind(
            handle.opaqueId);
        if (terminal != m_rest->terminalByOpaqueId.constEnd()) {
            return completeSoon(
                SstvShareProviderResult::success(terminal->result),
                std::move(completion));
        }
        return completeSoon(invalidRequest(QStringLiteral("REST cancellation")),
                            std::move(completion));
    }
    if (session->cancelled) {
        return completeSoon(SstvShareProviderResult::success(
            {session->opaqueId, session->committedBytes}),
            std::move(completion));
    }
    const QString opaqueId = session->opaqueId;
    Request request;
    request.method = "DELETE";
    request.url = endpointWithPath(
        m_rest->config.baseUrl,
        pathForUpload(m_rest->config.cancelUploadPathTemplate,
                      session->remoteId));
    request.headers = {{"Idempotency-Key", session->idempotencyKey.toLatin1()}};
    request.acceptedStatusCodes = {200, 202, 204, 404};
    request.diagnosticLabel = QStringLiteral("REST cancel upload");
    request.useProviderCredentials = m_rest->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::CancelUpload;
    request.responseHandler = [this, opaqueId](
                                  const SstvHttpResponse& response) {
        if (response.statusCode == 404
            && !m_rest->config.credentialsRequired) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::NotFound,
                QStringLiteral("unauthenticated REST 404 is not idempotent"));
        }
        auto current = m_rest->sessions.find(opaqueId);
        if (current == m_rest->sessions.end()) {
            return SstvShareProviderResult::success();
        }
        const SstvShareUploadHandle result {
            current->opaqueId, current->committedBytes};
        m_rest->recordTerminal(opaqueId, result, true);
        return SstvShareProviderResult::success(result);
    };
    return startRequest(std::move(request), std::move(completion));
}

SstvShareOperationId SstvGenericRestShareProvider::listIncomingAsync(
    qsizetype maximumItems,
    SstvShareIncomingCompletion completion)
{
    if (!m_rest->valid || !m_rest->capabilitiesVerified
        || !capabilities().incomingList || maximumItems <= 0
        || maximumItems > 1'000) {
        return completeIncomingSoon(
            invalidRequest(QStringLiteral("REST inbox list")), {},
            std::move(completion));
    }
    auto parsedItems = std::make_shared<QVector<SstvShareIncomingItem>>();
    QUrl url = endpointWithPath(m_rest->config.baseUrl,
                                m_rest->config.incomingListPath);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("limit"),
                       QString::number(maximumItems));
    url.setQuery(query);
    Request request;
    request.method = "GET";
    request.url = url;
    request.headers = {{"Accept", "application/json"}};
    request.acceptedStatusCodes = {200};
    request.responseBody = SstvHttpResponseBody::RequiredJson;
    request.diagnosticLabel = QStringLiteral("REST inbox list");
    request.useProviderCredentials = m_rest->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::IncomingList;
    request.responseHandler = [this, maximumItems, parsedItems](
                                  const SstvHttpResponse& response) {
        if (!jsonHasOnly(response.json, {QStringLiteral("items")})
            || !response.json.value(QStringLiteral("items")).isArray()) {
            return invalidRequest(QStringLiteral("REST inbox response"));
        }
        const QJsonArray values = response.json.value(
            QStringLiteral("items")).toArray();
        if (values.size() > maximumItems) {
            return invalidRequest(QStringLiteral("REST inbox response"));
        }
        QVector<SstvShareIncomingItem> items;
        items.reserve(values.size());
        QSet<QString> identifiers;
        for (const QJsonValue& value : values) {
            if (!value.isObject()) {
                return invalidRequest(QStringLiteral("REST inbox response"));
            }
            const QJsonObject object = value.toObject();
            if (!jsonHasOnly(object,
                             {QStringLiteral("incomingId"),
                              QStringLiteral("providerId"),
                              QStringLiteral("senderId"),
                              QStringLiteral("manifestSha256"),
                              QStringLiteral("canonicalManifestJson"),
                              QStringLiteral("byteSize"),
                              QStringLiteral("receivedUtc"),
                              QStringLiteral("expiresUtc")})
                || !object.value(QStringLiteral("incomingId")).isString()
                || !object.value(QStringLiteral("providerId")).isString()
                || !object.value(QStringLiteral("senderId")).isString()
                || !object.value(QStringLiteral("manifestSha256")).isString()
                || !object.value(QStringLiteral("canonicalManifestJson")).isString()) {
                return invalidRequest(QStringLiteral("REST inbox response"));
            }
            const auto byteSize = jsonUnsigned(
                object, QStringLiteral("byteSize"), true);
            const auto received = exactUtcTimestamp(
                object.value(QStringLiteral("receivedUtc")));
            const auto expires = exactUtcTimestamp(
                object.value(QStringLiteral("expiresUtc")));
            SstvShareIncomingItem item;
            item.opaqueId = object.value(
                QStringLiteral("incomingId")).toString();
            item.providerId = object.value(
                QStringLiteral("providerId")).toString();
            item.senderId = object.value(
                QStringLiteral("senderId")).toString();
            item.manifestSha256 = object.value(
                QStringLiteral("manifestSha256")).toString();
            item.canonicalManifestJson = object.value(
                QStringLiteral("canonicalManifestJson")).toString().toUtf8();
            item.byteSize = byteSize ? *byteSize : 0U;
            item.receivedUtc = received ? *received : QDateTime {};
            item.expiresUtc = expires ? *expires : QDateTime {};
            if (!byteSize || !received || !expires
                || item.providerId != providerId()
                || identifiers.contains(item.opaqueId)
                || !validateShareIncomingItem(item).ok()) {
                return invalidRequest(QStringLiteral("REST inbox response"));
            }
            identifiers.insert(item.opaqueId);
            items.push_back(std::move(item));
        }
        m_rest->incoming.clear();
        for (const auto& item : items) {
            m_rest->incoming.insert(item.opaqueId, item);
        }
        *parsedItems = std::move(items);
        return SstvShareProviderResult::success();
    };
    return startRequest(
        std::move(request),
        [completion = std::move(completion), parsedItems](
            SstvShareProviderResult result) mutable {
            if (completion) {
                completion(std::move(result), std::move(*parsedItems));
            }
        });
}

SstvShareOperationId SstvGenericRestShareProvider::downloadAsync(
    const QString& opaqueIncomingId,
    quint64 offset,
    quint64 maximumBytes,
    SstvShareProgressCallback progress,
    SstvShareProviderCompletion completion)
{
    const auto incoming = m_rest->incoming.constFind(opaqueIncomingId);
    if (!m_rest->valid || !m_rest->capabilitiesVerified
        || !capabilities().download || !isSafeShareIdentifier(opaqueIncomingId)
        || incoming == m_rest->incoming.constEnd() || maximumBytes == 0U
        || maximumBytes > capabilities().maximumResponseBytes
        || maximumBytes > static_cast<quint64>(
               std::numeric_limits<qsizetype>::max())
        || offset >= incoming->byteSize) {
        return completeSoon(invalidRequest(QStringLiteral("REST download")),
                            std::move(completion));
    }
    const auto manifest = parseSstvShareManifestV1(
        incoming->canonicalManifestJson);
    if (!manifest.ok()) {
        return completeSoon(invalidRequest(QStringLiteral("REST download")),
                            std::move(completion));
    }
    const quint64 requested = std::min(maximumBytes,
                                       incoming->byteSize - offset);
    const quint64 last = offset + requested - 1U;
    Request request;
    request.method = "GET";
    request.url = endpointWithPath(
        m_rest->config.baseUrl,
        pathForIdentifier(m_rest->config.downloadPathTemplate,
                          QStringLiteral("{incomingId}"), opaqueIncomingId));
    request.headers = {
        {"Accept", "application/octet-stream, image/png, image/jpeg"},
        {"Range", QByteArray("bytes=") + QByteArray::number(offset)
             + '-' + QByteArray::number(last)},
    };
    request.acceptedStatusCodes = {200, 206};
    request.maximumResponseBytes = static_cast<qsizetype>(requested);
    request.diagnosticLabel = QStringLiteral("REST inbox download");
    request.useProviderCredentials = m_rest->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::DownloadContent;
    request.progress = std::move(progress);
    request.responseHandler = [incoming = *incoming,
                               mimeType = manifest.manifest->mimeType,
                               fullHash = manifest.manifest->sha256,
                               offset, requested](
                                  const SstvHttpResponse& response) {
        const QByteArray contentType = response.header("content-type")
            .split(';').value(0).trimmed().toLower();
        if ((contentType != "application/octet-stream"
             && contentType != mimeType.toLatin1())
            || !responseChunkIsValid(response, offset, requested,
                                     incoming.byteSize)) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::Validation,
                QStringLiteral("REST download range or MIME was not verified"));
        }
        const bool hasDigest = !response.header("digest").isEmpty()
            || !response.header("x-checksum-sha256").isEmpty()
            || !response.header("x-amz-checksum-sha256").isEmpty();
        const QString chunkHash = QString::fromLatin1(
            QCryptographicHash::hash(response.body,
                                     QCryptographicHash::Sha256).toHex());
        if (hasDigest
            && !responseContainsExpectedSha256(response, chunkHash)
            && !responseContainsExpectedSha256(response, fullHash)) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::Integrity,
                QStringLiteral("REST download response digest did not match"));
        }
        return SstvShareProviderResult::success({}, response.body);
    };
    return startRequest(std::move(request), std::move(completion));
}

SstvShareOperationId SstvGenericRestShareProvider::acknowledgeAsync(
    const QString& opaqueIncomingId,
    SstvShareProviderCompletion completion)
{
    if (!m_rest->valid || !m_rest->capabilitiesVerified
        || !capabilities().acknowledgement
        || !isSafeShareIdentifier(opaqueIncomingId)) {
        return completeSoon(invalidRequest(QStringLiteral("REST acknowledgement")),
                            std::move(completion));
    }
    Request request;
    request.method = "POST";
    request.url = endpointWithPath(
        m_rest->config.baseUrl,
        pathForIdentifier(m_rest->config.acknowledgePathTemplate,
                          QStringLiteral("{incomingId}"), opaqueIncomingId));
    request.headers = {{"Idempotency-Key", idempotencyForIncomingAction(
        providerId(), opaqueIncomingId, QByteArrayLiteral("acknowledge"))}};
    request.acceptedStatusCodes = {200, 204};
    request.diagnosticLabel = QStringLiteral("REST inbox acknowledgement");
    request.useProviderCredentials = m_rest->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::AcknowledgeIncoming;
    request.responseHandler = [](const SstvHttpResponse&) {
        return SstvShareProviderResult::success();
    };
    return startRequest(std::move(request), std::move(completion));
}

SstvShareOperationId SstvGenericRestShareProvider::rejectAsync(
    const QString& opaqueIncomingId,
    SstvShareProviderCompletion completion)
{
    if (!m_rest->valid || !m_rest->capabilitiesVerified
        || !capabilities().rejection
        || !isSafeShareIdentifier(opaqueIncomingId)) {
        return completeSoon(invalidRequest(QStringLiteral("REST rejection")),
                            std::move(completion));
    }
    Request request;
    request.method = "POST";
    request.url = endpointWithPath(
        m_rest->config.baseUrl,
        pathForIdentifier(m_rest->config.rejectPathTemplate,
                          QStringLiteral("{incomingId}"), opaqueIncomingId));
    request.headers = {{"Idempotency-Key", idempotencyForIncomingAction(
        providerId(), opaqueIncomingId, QByteArrayLiteral("reject"))}};
    request.acceptedStatusCodes = {200, 204};
    request.diagnosticLabel = QStringLiteral("REST inbox rejection");
    request.useProviderCredentials = m_rest->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::RejectIncoming;
    request.responseHandler = [](const SstvHttpResponse&) {
        return SstvShareProviderResult::success();
    };
    return startRequest(std::move(request), std::move(completion));
}

SstvShareOperationId SstvGenericRestShareProvider::deleteIncomingAsync(
    const QString& opaqueIncomingId,
    SstvShareProviderCompletion completion)
{
    if (!m_rest->valid || !m_rest->capabilitiesVerified
        || !capabilities().incomingDelete
        || !isSafeShareIdentifier(opaqueIncomingId)
        || !m_rest->incoming.contains(opaqueIncomingId)) {
        return completeSoon(SstvShareProviderResult::failure(
            SstvShareProviderFailure::PermanentProviderFailure,
            QStringLiteral("REST incoming deletion is not a verified capability")),
            std::move(completion));
    }
    Request request;
    request.method = "DELETE";
    request.url = endpointWithPath(
        m_rest->config.baseUrl,
        pathForIdentifier(m_rest->config.deleteIncomingPathTemplate,
                          QStringLiteral("{incomingId}"), opaqueIncomingId));
    request.headers = {{"Idempotency-Key", idempotencyForIncomingAction(
        providerId(), opaqueIncomingId, QByteArrayLiteral("delete"))}};
    request.acceptedStatusCodes = {200, 202, 204, 404};
    request.diagnosticLabel = QStringLiteral("REST inbox deletion");
    request.useProviderCredentials = m_rest->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::DeleteIncoming;
    request.responseHandler = [this, opaqueIncomingId](
                                  const SstvHttpResponse& response) {
        if (response.statusCode == 404
            && !m_rest->config.credentialsRequired) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::NotFound,
                QStringLiteral("unauthenticated REST 404 is not idempotent"));
        }
        m_rest->incoming.remove(opaqueIncomingId);
        return SstvShareProviderResult::success(
            {opaqueIncomingId, 0U});
    };
    return startRequest(std::move(request), std::move(completion));
}

SstvShareOperationId SstvGenericRestShareProvider::blockSenderAsync(
    const QString& senderId,
    SstvShareProviderCompletion completion)
{
    const bool senderWasListed = std::any_of(
        m_rest->incoming.cbegin(), m_rest->incoming.cend(),
        [&senderId](const SstvShareIncomingItem& item) {
            return item.senderId == senderId;
        });
    if (!m_rest->valid || !m_rest->capabilitiesVerified
        || !capabilities().senderBlocking || !isSafeShareIdentifier(senderId)
        || !senderWasListed) {
        return completeSoon(SstvShareProviderResult::failure(
            SstvShareProviderFailure::PermanentProviderFailure,
            QStringLiteral("REST sender blocking is not a verified capability")),
            std::move(completion));
    }
    Request request;
    request.method = "POST";
    request.url = endpointWithPath(
        m_rest->config.baseUrl,
        pathForIdentifier(m_rest->config.blockSenderPathTemplate,
                          QStringLiteral("{senderId}"), senderId));
    request.headers = {{"Idempotency-Key", idempotencyForIncomingAction(
        providerId(), senderId, QByteArrayLiteral("block-sender"))}};
    request.acceptedStatusCodes = {200, 204};
    request.diagnosticLabel = QStringLiteral("REST sender blocking");
    request.useProviderCredentials = m_rest->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::BlockSender;
    request.responseHandler = [](const SstvHttpResponse&) {
        return SstvShareProviderResult::success();
    };
    return startRequest(std::move(request), std::move(completion));
}

// WebDAV HTTPS ---------------------------------------------------------------

struct SstvWebDavShareProvider::Impl final
{
    struct Session final
    {
        QString opaqueId;
        QString idempotencyKey;
        QString sha256;
        QString mimeType;
        QUrl remoteUrl;
        QString remoteObjectId;
        quint64 totalBytes {0U};
        quint64 committedBytes {0U};
        QDateTime expiresUtc;
        bool completed {false};
        bool cancelled {false};
    };

    struct TerminalRecord final
    {
        QString originalOpaqueId;
        QString idempotencyKey;
        SstvShareUploadHandle result;
        QDateTime expiresUtc;
        quint64 sequence {0U};
        bool cancelled {false};
    };

    SstvWebDavProviderConfig config;
    bool valid {false};
    QHash<QString, Session> sessions;
    QHash<QString, QString> handleByIdempotency;
    QHash<QString, TerminalRecord> terminalByOpaqueId;
    QHash<QString, QString> terminalOpaqueByIdempotency;
    qsizetype pendingCreates {0};
    quint64 nextTerminalSequence {1U};

    void removeTerminal(const QString& opaqueId)
    {
        const auto record = terminalByOpaqueId.find(opaqueId);
        if (record == terminalByOpaqueId.end()) {
            return;
        }
        terminalOpaqueByIdempotency.remove(record->idempotencyKey);
        terminalByOpaqueId.erase(record);
    }

    void removeActive(const QString& opaqueId)
    {
        const auto session = sessions.find(opaqueId);
        if (session == sessions.end()) {
            return;
        }
        handleByIdempotency.remove(session->idempotencyKey);
        sessions.erase(session);
    }

    void trimTerminals()
    {
        while (terminalByOpaqueId.size() > config.maximumTerminalRecords) {
            auto oldest = terminalByOpaqueId.end();
            for (auto record = terminalByOpaqueId.begin();
                 record != terminalByOpaqueId.end(); ++record) {
                if (oldest == terminalByOpaqueId.end()
                    || record->sequence < oldest->sequence) {
                    oldest = record;
                }
            }
            if (oldest == terminalByOpaqueId.end()) {
                break;
            }
            removeTerminal(oldest.key());
        }
    }

    void purgeExpired(const QDateTime& nowUtc)
    {
        QStringList activeExpired;
        for (auto session = sessions.cbegin(); session != sessions.cend();
             ++session) {
            if (session->expiresUtc <= nowUtc) {
                activeExpired.push_back(session.key());
            }
        }
        for (const QString& opaqueId : activeExpired) {
            removeActive(opaqueId);
        }
        QStringList terminalExpired;
        for (auto record = terminalByOpaqueId.cbegin();
             record != terminalByOpaqueId.cend(); ++record) {
            if (record->expiresUtc <= nowUtc) {
                terminalExpired.push_back(record.key());
            }
        }
        for (const QString& opaqueId : terminalExpired) {
            removeTerminal(opaqueId);
        }
    }

    void recordTerminal(const QString& opaqueId,
                        SstvShareUploadHandle result,
                        bool cancelled)
    {
        const auto session = sessions.constFind(opaqueId);
        if (session == sessions.constEnd()) {
            return;
        }
        TerminalRecord record;
        record.originalOpaqueId = session->opaqueId;
        record.idempotencyKey = session->idempotencyKey;
        record.result = std::move(result);
        record.expiresUtc = session->expiresUtc;
        record.sequence = nextTerminalSequence++;
        record.cancelled = cancelled;
        removeActive(opaqueId);
        terminalOpaqueByIdempotency.insert(record.idempotencyKey,
                                           record.originalOpaqueId);
        terminalByOpaqueId.insert(record.originalOpaqueId, std::move(record));
        trimTerminals();
    }

    const TerminalRecord* terminalForIdempotency(
        const QString& idempotencyKey) const
    {
        const auto opaqueId = terminalOpaqueByIdempotency.constFind(
            idempotencyKey);
        if (opaqueId == terminalOpaqueByIdempotency.constEnd()) {
            return nullptr;
        }
        const auto record = terminalByOpaqueId.constFind(*opaqueId);
        return record == terminalByOpaqueId.constEnd() ? nullptr : &*record;
    }

    void removeTerminalForRemoteObject(const QString& remoteObjectId)
    {
        QStringList matches;
        for (auto record = terminalByOpaqueId.cbegin();
             record != terminalByOpaqueId.cend(); ++record) {
            if (record->result.opaqueId == remoteObjectId) {
                matches.push_back(record.key());
            }
        }
        for (const QString& opaqueId : matches) {
            removeTerminal(opaqueId);
        }
    }
};

SstvWebDavShareProvider::SstvWebDavShareProvider(
    SstvWebDavProviderConfig config,
    std::shared_ptr<SstvShareCredentialSource> credentialSource,
    QObject* parent)
    : SstvHttpShareProvider(config.providerId, webDavCapabilities(config),
                            config.transport, std::move(credentialSource),
                            config.credentialsRequired, parent)
    , m_webDav(std::make_unique<Impl>())
{
    m_webDav->config = std::move(config);
    const auto& value = m_webDav->config;
    m_webDav->valid = isSafeShareIdentifier(value.providerId)
        && endpointAllowed(value.collectionUrl)
        && value.collectionUrl.query().isEmpty()
        && !value.collectionUrl.path().isEmpty()
        && validTransportOptions(value.transport)
        && validSessionBounds(value.maximumActiveSessions,
                              value.maximumTerminalRecords);
}

SstvWebDavShareProvider::~SstvWebDavShareProvider() = default;

bool SstvWebDavShareProvider::isConfigurationValid() const noexcept
{
    return m_webDav->valid;
}

SstvShareOperationId SstvWebDavShareProvider::createUploadAsync(
    const SstvShareManifestV1& manifest,
    const QString& idempotencyKey,
    SstvShareProviderCompletion completion)
{
    m_webDav->purgeExpired(QDateTime::currentDateTimeUtc());
    if (!m_webDav->valid || manifest.providerId != providerId()
        || manifest.chunkCount != 1U
        || !manifest.validate(true, QDateTime::currentDateTimeUtc()).ok()
        || !isLowercaseSha256(idempotencyKey)
        || idempotencyKey != SstvShareTransfer::deriveIdempotencyKey(manifest)) {
        return completeSoon(invalidRequest(QStringLiteral("WebDAV create")),
                            std::move(completion));
    }
    const auto existing = m_webDav->handleByIdempotency.constFind(idempotencyKey);
    if (existing != m_webDav->handleByIdempotency.constEnd()) {
        const auto session = m_webDav->sessions.constFind(*existing);
        if (session != m_webDav->sessions.constEnd()) {
            return completeSoon(SstvShareProviderResult::success(
                {session->opaqueId, session->committedBytes}),
                std::move(completion));
        }
    }
    if (const auto* terminal = m_webDav->terminalForIdempotency(
            idempotencyKey)) {
        return completeSoon(SstvShareProviderResult::success(terminal->result),
                            std::move(completion));
    }
    if (m_webDav->sessions.size() + m_webDav->pendingCreates
        >= m_webDav->config.maximumActiveSessions) {
        return completeSoon(SstvShareProviderResult::failure(
            SstvShareProviderFailure::ProviderUnavailable,
            QStringLiteral("WebDAV upload session capacity is full")),
            std::move(completion));
    }

    QUrl remoteUrl = m_webDav->config.collectionUrl;
    QString collectionPath = remoteUrl.path();
    if (!collectionPath.endsWith(QLatin1Char('/'))) {
        collectionPath += QLatin1Char('/');
    }
    const QString extension = manifest.mimeType == QStringLiteral("image/png")
        ? QStringLiteral(".png") : QStringLiteral(".jpg");
    const QString remoteObjectId = manifest.transferId.toString(
        QUuid::WithoutBraces) + extension;
    collectionPath += remoteObjectId;
    remoteUrl.setPath(collectionPath);

    Request request;
    request.method = "PROPFIND";
    request.url = m_webDav->config.collectionUrl;
    request.headers = {{"Depth", "0"}};
    request.acceptedStatusCodes = {200, 207};
    request.diagnosticLabel = QStringLiteral("WebDAV collection probe");
    request.useProviderCredentials = m_webDav->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::CreateUpload;
    request.responseHandler = [this, manifest, idempotencyKey, remoteUrl,
                               remoteObjectId](
                                  const SstvHttpResponse&) {
        const auto existing = m_webDav->handleByIdempotency.constFind(
            idempotencyKey);
        if (existing != m_webDav->handleByIdempotency.constEnd()) {
            const auto current = m_webDav->sessions.constFind(*existing);
            if (current != m_webDav->sessions.constEnd()) {
                return SstvShareProviderResult::success(
                    {current->opaqueId, current->committedBytes});
            }
        }
        Impl::Session session;
        session.opaqueId = makeOpaqueUploadId(
            QStringLiteral("webdav-upload"), idempotencyKey);
        session.idempotencyKey = idempotencyKey;
        session.sha256 = manifest.sha256;
        session.mimeType = manifest.mimeType;
        session.remoteUrl = remoteUrl;
        session.remoteObjectId = remoteObjectId;
        session.totalBytes = manifest.byteSize;
        session.expiresUtc = manifest.expiresUtc;
        m_webDav->handleByIdempotency.insert(idempotencyKey, session.opaqueId);
        m_webDav->sessions.insert(session.opaqueId, session);
        return SstvShareProviderResult::success({session.opaqueId, 0U});
    };
    ++m_webDav->pendingCreates;
    auto reservation = std::make_shared<bool>(true);
    const SstvShareOperationId operation = startRequest(
        std::move(request),
        [this, reservation, completion = std::move(completion)](
            SstvShareProviderResult result) mutable {
            if (*reservation) {
                *reservation = false;
                --m_webDav->pendingCreates;
            }
            if (completion) {
                completion(std::move(result));
            }
        });
    if (operation == 0U && *reservation) {
        *reservation = false;
        --m_webDav->pendingCreates;
    }
    return operation;
}

SstvShareOperationId SstvWebDavShareProvider::uploadChunkAsync(
    const SstvShareUploadHandle& handle,
    quint64 offset,
    const QByteArray& chunk,
    const QString& chunkSha256,
    SstvShareProgressCallback progress,
    SstvShareProviderCompletion completion)
{
    m_webDav->purgeExpired(QDateTime::currentDateTimeUtc());
    const auto session = m_webDav->sessions.constFind(handle.opaqueId);
    const QByteArray actualHash = sha256Hex(chunk);
    if (!m_webDav->valid || session == m_webDav->sessions.constEnd()
        || session->cancelled || session->completed || offset != 0U
        || handle.committedBytes != 0U
        || static_cast<quint64>(chunk.size()) != session->totalBytes
        || chunkSha256 != session->sha256 || actualHash != chunkSha256.toLatin1()) {
        return completeSoon(
            actualHash != chunkSha256.toLatin1()
                ? SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Integrity,
                    QStringLiteral("WebDAV payload hash mismatch"))
                : invalidRequest(QStringLiteral("WebDAV PUT")),
            std::move(completion));
    }
    const QString opaqueId = session->opaqueId;
    Request request;
    request.method = "PUT";
    request.url = session->remoteUrl;
    request.body = chunk;
    request.headers = {
        {"Content-Type", session->mimeType.toLatin1()},
        {"Digest", QByteArray("sha-256=") + sha256Base64(chunk)},
        {"X-Content-SHA256", chunkSha256.toLatin1()},
    };
    if (!m_webDav->config.overwriteExisting) {
        request.headers.push_back({"If-None-Match", "*"});
    }
    request.acceptedStatusCodes = {200, 201, 204};
    request.diagnosticLabel = QStringLiteral("WebDAV PUT");
    request.useProviderCredentials = m_webDav->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::UploadContent;
    request.progress = std::move(progress);
    request.responseHandler = [this, opaqueId](const SstvHttpResponse&) {
        auto current = m_webDav->sessions.find(opaqueId);
        if (current == m_webDav->sessions.end()) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::NotFound,
                QStringLiteral("WebDAV session disappeared"));
        }
        current->committedBytes = current->totalBytes;
        return SstvShareProviderResult::success(
            {current->opaqueId, current->committedBytes});
    };
    return startRequest(std::move(request), std::move(completion));
}

SstvShareOperationId SstvWebDavShareProvider::queryStatusAsync(
    const SstvShareUploadHandle& handle,
    SstvShareProviderCompletion completion)
{
    m_webDav->purgeExpired(QDateTime::currentDateTimeUtc());
    const auto session = m_webDav->sessions.constFind(handle.opaqueId);
    if (!m_webDav->valid) {
        return completeSoon(invalidRequest(QStringLiteral("WebDAV status")),
                            std::move(completion));
    }
    if (session == m_webDav->sessions.constEnd()) {
        const auto terminal = m_webDav->terminalByOpaqueId.constFind(
            handle.opaqueId);
        if (terminal != m_webDav->terminalByOpaqueId.constEnd()
            && !terminal->cancelled) {
            return completeSoon(
                SstvShareProviderResult::success(terminal->result),
                std::move(completion));
        }
        return completeSoon(invalidRequest(QStringLiteral("WebDAV status")),
                            std::move(completion));
    }
    const QString opaqueId = session->opaqueId;
    Request request;
    request.method = "HEAD";
    request.url = session->remoteUrl;
    request.acceptedStatusCodes = {200, 204};
    request.diagnosticLabel = QStringLiteral("WebDAV HEAD status");
    request.useProviderCredentials = m_webDav->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::QueryStatus;
    request.responseHandler = [this, opaqueId](const SstvHttpResponse& response) {
        auto current = m_webDav->sessions.find(opaqueId);
        bool ok = false;
        const quint64 length = response.header("content-length").toULongLong(&ok);
        if (current == m_webDav->sessions.end() || !ok
            || length > current->totalBytes) {
            return invalidRequest(QStringLiteral("WebDAV HEAD response"));
        }
        current->committedBytes = length;
        if (length == current->totalBytes && m_webDav->config.requireServerSha256
            && !responseContainsExpectedSha256(response, current->sha256)) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::Integrity,
                QStringLiteral("WebDAV server SHA-256 was not verified"));
        }
        return SstvShareProviderResult::success(
            {current->opaqueId, current->committedBytes});
    };
    return startRequest(std::move(request), std::move(completion));
}

SstvShareOperationId SstvWebDavShareProvider::resumeUploadAsync(
    const SstvShareUploadHandle& handle,
    SstvShareProviderCompletion completion)
{
    return queryStatusAsync(handle, std::move(completion));
}

SstvShareOperationId SstvWebDavShareProvider::completeUploadAsync(
    const SstvShareUploadHandle& handle,
    const QString& idempotencyKey,
    SstvShareProviderCompletion completion)
{
    m_webDav->purgeExpired(QDateTime::currentDateTimeUtc());
    const auto session = m_webDav->sessions.constFind(handle.opaqueId);
    if (!m_webDav->valid) {
        return completeSoon(invalidRequest(QStringLiteral("WebDAV completion")),
                            std::move(completion));
    }
    if (session == m_webDav->sessions.constEnd()) {
        const auto terminal = m_webDav->terminalByOpaqueId.constFind(
            handle.opaqueId);
        if (terminal != m_webDav->terminalByOpaqueId.constEnd()
            && !terminal->cancelled
            && terminal->idempotencyKey == idempotencyKey) {
            return completeSoon(
                SstvShareProviderResult::success(terminal->result),
                std::move(completion));
        }
        return completeSoon(invalidRequest(QStringLiteral("WebDAV completion")),
                            std::move(completion));
    }
    if (session->cancelled || idempotencyKey != session->idempotencyKey
        || handle.committedBytes != session->totalBytes
        || session->committedBytes != session->totalBytes) {
        return completeSoon(invalidRequest(QStringLiteral("WebDAV completion")),
                            std::move(completion));
    }
    if (session->completed) {
        return completeSoon(SstvShareProviderResult::success(
            {session->remoteObjectId, session->committedBytes}),
            std::move(completion));
    }
    const QString opaqueId = session->opaqueId;
    Request request;
    request.method = "HEAD";
    request.url = session->remoteUrl;
    request.acceptedStatusCodes = {200, 204};
    request.diagnosticLabel = QStringLiteral("WebDAV verify upload");
    request.useProviderCredentials = m_webDav->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::CompleteUpload;
    request.responseHandler = [this, opaqueId](const SstvHttpResponse& response) {
        auto current = m_webDav->sessions.find(opaqueId);
        bool ok = false;
        const quint64 length = response.header("content-length").toULongLong(&ok);
        if (current == m_webDav->sessions.end() || !ok
            || length != current->totalBytes) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::Integrity,
                QStringLiteral("WebDAV server length was not verified"));
        }
        if (m_webDav->config.requireServerSha256
            && !responseContainsExpectedSha256(response, current->sha256)) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::Integrity,
                QStringLiteral("WebDAV server SHA-256 was not verified"));
        }
        const SstvShareUploadHandle result {
            current->remoteObjectId, current->committedBytes};
        m_webDav->recordTerminal(opaqueId, result, false);
        return SstvShareProviderResult::success(result);
    };
    return startRequest(std::move(request), std::move(completion));
}

SstvShareOperationId SstvWebDavShareProvider::cancelUploadAsync(
    const SstvShareUploadHandle& handle,
    SstvShareProviderCompletion completion)
{
    m_webDav->purgeExpired(QDateTime::currentDateTimeUtc());
    const auto session = m_webDav->sessions.constFind(handle.opaqueId);
    if (!m_webDav->valid) {
        return completeSoon(invalidRequest(QStringLiteral("WebDAV DELETE")),
                            std::move(completion));
    }
    if (session == m_webDav->sessions.constEnd()) {
        const auto terminal = m_webDav->terminalByOpaqueId.constFind(
            handle.opaqueId);
        if (terminal != m_webDav->terminalByOpaqueId.constEnd()) {
            return completeSoon(
                SstvShareProviderResult::success(terminal->result),
                std::move(completion));
        }
        return completeSoon(invalidRequest(QStringLiteral("WebDAV DELETE")),
                            std::move(completion));
    }
    if (session->cancelled) {
        return completeSoon(SstvShareProviderResult::success(
            {session->opaqueId, session->committedBytes}),
            std::move(completion));
    }
    const QString opaqueId = session->opaqueId;
    Request request;
    request.method = "DELETE";
    request.url = session->remoteUrl;
    request.acceptedStatusCodes = {200, 202, 204, 404};
    request.diagnosticLabel = QStringLiteral("WebDAV DELETE");
    request.useProviderCredentials = m_webDav->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::CancelUpload;
    request.responseHandler = [this, opaqueId](
                                  const SstvHttpResponse& response) {
        if (response.statusCode == 404
            && !m_webDav->config.credentialsRequired) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::NotFound,
                QStringLiteral("unauthenticated WebDAV 404 is not idempotent"));
        }
        auto current = m_webDav->sessions.find(opaqueId);
        if (current == m_webDav->sessions.end()) {
            return SstvShareProviderResult::success();
        }
        const SstvShareUploadHandle result {
            current->opaqueId, current->committedBytes};
        m_webDav->recordTerminal(opaqueId, result, true);
        return SstvShareProviderResult::success(result);
    };
    return startRequest(std::move(request), std::move(completion));
}

SstvShareOperationId SstvWebDavShareProvider::downloadAsync(
    const QString& opaqueIncomingId,
    quint64 offset,
    quint64 maximumBytes,
    SstvShareProgressCallback progress,
    SstvShareProviderCompletion completion)
{
    if (!m_webDav->valid || !isSafeShareIdentifier(opaqueIncomingId)
        || maximumBytes == 0U
        || maximumBytes > capabilities().maximumResponseBytes
        || maximumBytes > static_cast<quint64>(
               std::numeric_limits<qsizetype>::max())
        || offset > kMaximumSharedImageBytes
        || maximumBytes > kMaximumSharedImageBytes - offset) {
        return completeSoon(invalidRequest(QStringLiteral("WebDAV GET")),
                            std::move(completion));
    }
    QUrl remoteUrl = m_webDav->config.collectionUrl;
    QString path = remoteUrl.path();
    if (!path.endsWith(QLatin1Char('/'))) {
        path += QLatin1Char('/');
    }
    path += opaqueIncomingId;
    remoteUrl.setPath(path);
    const quint64 last = offset + maximumBytes - 1U;
    Request request;
    request.method = "GET";
    request.url = remoteUrl;
    request.headers = {
        {"Accept", "application/octet-stream, image/png, image/jpeg"},
        {"Range", QByteArray("bytes=") + QByteArray::number(offset)
             + '-' + QByteArray::number(last)},
    };
    request.acceptedStatusCodes = {200, 206};
    request.maximumResponseBytes = static_cast<qsizetype>(maximumBytes);
    request.diagnosticLabel = QStringLiteral("WebDAV GET");
    request.useProviderCredentials = m_webDav->config.credentialsRequired;
    request.credentialPurpose = SstvShareCredentialPurpose::DownloadContent;
    request.progress = std::move(progress);
    request.responseHandler = [offset, maximumBytes](
                                  const SstvHttpResponse& response) {
        const QByteArray contentType = response.header("content-type")
            .split(';').value(0).trimmed().toLower();
        if (response.body.isEmpty()
            || static_cast<quint64>(response.body.size()) > maximumBytes
            || (contentType != "application/octet-stream"
                && contentType != "image/png"
                && contentType != "image/jpeg")) {
            return invalidRequest(QStringLiteral("WebDAV GET response"));
        }
        if (response.statusCode == 206) {
            const auto range = parseContentRange(
                response.header("content-range"));
            if (!range || range->first != offset
                || range->last - range->first + 1U
                    != static_cast<quint64>(response.body.size())) {
                return invalidRequest(QStringLiteral("WebDAV GET range"));
            }
        } else if (offset != 0U) {
            return invalidRequest(QStringLiteral("WebDAV GET range"));
        }
        return SstvShareProviderResult::success({}, response.body);
    };
    return startRequest(std::move(request), std::move(completion));
}

SstvShareOperationId SstvWebDavShareProvider::deleteRemoteObjectAsync(
    const QString& opaqueId,
    SstvShareProviderCompletion completion)
{
    const qsizetype suffixLength = opaqueId.endsWith(QStringLiteral(".png"))
        ? 4 : (opaqueId.endsWith(QStringLiteral(".jpg")) ? 4 : 0);
    const QString transferId = suffixLength > 0
        ? opaqueId.left(opaqueId.size() - suffixLength) : QString {};
    const QUuid uuid(transferId);
    if (!m_webDav->valid || !isSafeShareIdentifier(opaqueId)
        || suffixLength == 0 || uuid.isNull() || transferId.size() != 36
        || uuid.toString(QUuid::WithoutBraces) != transferId) {
        return completeSoon(invalidRequest(
            QStringLiteral("WebDAV remote object DELETE")),
            std::move(completion));
    }
    QUrl remoteUrl = m_webDav->config.collectionUrl;
    QString path = remoteUrl.path();
    if (!path.endsWith(QLatin1Char('/'))) {
        path += QLatin1Char('/');
    }
    path += opaqueId;
    remoteUrl.setPath(path);
    Request request;
    request.method = "DELETE";
    request.url = remoteUrl;
    // The documented Decodium WebDAV profile authorizes an authenticated 404
    // as idempotent only for this exact, locally-derived completed object ID.
    request.acceptedStatusCodes = {200, 202, 204, 404};
    request.diagnosticLabel = QStringLiteral("WebDAV remote object DELETE");
    request.useProviderCredentials = m_webDav->config.credentialsRequired;
    request.credentialPurpose =
        SstvShareCredentialPurpose::DeleteRemoteObject;
    request.responseHandler = [this, opaqueId](
                                  const SstvHttpResponse& response) {
        if (response.statusCode == 404
            && !m_webDav->config.credentialsRequired) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::NotFound,
                QStringLiteral("unauthenticated WebDAV 404 is not idempotent"));
        }
        QStringList activeMatches;
        for (auto session = m_webDav->sessions.cbegin();
             session != m_webDav->sessions.cend(); ++session) {
            if (session->remoteObjectId == opaqueId) {
                activeMatches.push_back(session.key());
            }
        }
        for (const QString& sessionId : activeMatches) {
            m_webDav->removeActive(sessionId);
        }
        m_webDav->removeTerminalForRemoteObject(opaqueId);
        return SstvShareProviderResult::success({opaqueId, 0U});
    };
    return startRequest(std::move(request), std::move(completion));
}

// Pre-signed PUT -------------------------------------------------------------

struct SstvPresignedPutShareProvider::Impl final
{
    struct Session final
    {
        QString opaqueId;
        QString idempotencyKey;
        QString sha256;
        QString mimeType;
        quint64 totalBytes {0U};
        QDateTime expiresUtc;
        bool uploaded {false};
        bool completed {false};
        bool cancelled {false};
        SstvShareOperationId activeOperation {0U};
        std::shared_ptr<const SstvSharePresignedTargetLease> target;
    };

    struct TerminalRecord final
    {
        QString originalOpaqueId;
        QString idempotencyKey;
        SstvShareUploadHandle result;
        QDateTime expiresUtc;
        quint64 sequence {0U};
        bool cancelled {false};
    };

    SstvPresignedPutProviderConfig config;
    std::shared_ptr<SstvSharePresignedTargetSource> targetSource;
    bool valid {false};
    QHash<QString, Session> sessions;
    QHash<QString, QString> handleByIdempotency;
    QHash<QString, TerminalRecord> terminalByOpaqueId;
    QHash<QString, QString> terminalOpaqueByIdempotency;
    quint64 nextTerminalSequence {1U};

    void removeTerminal(const QString& opaqueId)
    {
        const auto record = terminalByOpaqueId.find(opaqueId);
        if (record == terminalByOpaqueId.end()) {
            return;
        }
        terminalOpaqueByIdempotency.remove(record->idempotencyKey);
        terminalByOpaqueId.erase(record);
    }

    void removeActive(const QString& opaqueId)
    {
        const auto session = sessions.find(opaqueId);
        if (session == sessions.end()) {
            return;
        }
        handleByIdempotency.remove(session->idempotencyKey);
        sessions.erase(session);
    }

    void trimTerminals()
    {
        while (terminalByOpaqueId.size() > config.maximumTerminalRecords) {
            auto oldest = terminalByOpaqueId.end();
            for (auto record = terminalByOpaqueId.begin();
                 record != terminalByOpaqueId.end(); ++record) {
                if (oldest == terminalByOpaqueId.end()
                    || record->sequence < oldest->sequence) {
                    oldest = record;
                }
            }
            if (oldest == terminalByOpaqueId.end()) {
                break;
            }
            removeTerminal(oldest.key());
        }
    }

    void purgeExpired(SstvPresignedPutShareProvider* owner,
                      const QDateTime& nowUtc)
    {
        QStringList activeExpired;
        for (auto session = sessions.cbegin(); session != sessions.cend();
             ++session) {
            if (session->expiresUtc <= nowUtc) {
                activeExpired.push_back(session.key());
            }
        }
        for (const QString& opaqueId : activeExpired) {
            auto session = sessions.find(opaqueId);
            if (session != sessions.end() && session->activeOperation != 0U) {
                owner->cancelOperation(session->activeOperation);
            }
            removeActive(opaqueId);
        }
        QStringList terminalExpired;
        for (auto record = terminalByOpaqueId.cbegin();
             record != terminalByOpaqueId.cend(); ++record) {
            if (record->expiresUtc <= nowUtc) {
                terminalExpired.push_back(record.key());
            }
        }
        for (const QString& opaqueId : terminalExpired) {
            removeTerminal(opaqueId);
        }
    }

    void recordTerminal(const QString& opaqueId,
                        SstvShareUploadHandle result,
                        bool cancelled)
    {
        const auto session = sessions.constFind(opaqueId);
        if (session == sessions.constEnd()) {
            return;
        }
        TerminalRecord record;
        record.originalOpaqueId = session->opaqueId;
        record.idempotencyKey = session->idempotencyKey;
        record.result = std::move(result);
        record.expiresUtc = session->expiresUtc;
        record.sequence = nextTerminalSequence++;
        record.cancelled = cancelled;
        removeActive(opaqueId);
        terminalOpaqueByIdempotency.insert(record.idempotencyKey,
                                           record.originalOpaqueId);
        terminalByOpaqueId.insert(record.originalOpaqueId, std::move(record));
        trimTerminals();
    }

    const TerminalRecord* terminalForIdempotency(
        const QString& idempotencyKey) const
    {
        const auto opaqueId = terminalOpaqueByIdempotency.constFind(
            idempotencyKey);
        if (opaqueId == terminalOpaqueByIdempotency.constEnd()) {
            return nullptr;
        }
        const auto record = terminalByOpaqueId.constFind(*opaqueId);
        return record == terminalByOpaqueId.constEnd() ? nullptr : &*record;
    }
};

SstvPresignedPutShareProvider::SstvPresignedPutShareProvider(
    SstvPresignedPutProviderConfig config,
    std::shared_ptr<SstvSharePresignedTargetSource> targetSource,
    QObject* parent)
    : SstvHttpShareProvider(config.providerId, presignedCapabilities(config),
                            config.transport, {}, false, parent)
    , m_presigned(std::make_unique<Impl>())
{
    m_presigned->config = std::move(config);
    m_presigned->targetSource = std::move(targetSource);
    m_presigned->valid = isSafeShareIdentifier(m_presigned->config.providerId)
        && validTransportOptions(m_presigned->config.transport)
        && validSessionBounds(m_presigned->config.maximumActiveSessions,
                              m_presigned->config.maximumTerminalRecords)
        && static_cast<bool>(m_presigned->targetSource);
}

SstvPresignedPutShareProvider::~SstvPresignedPutShareProvider() = default;

bool SstvPresignedPutShareProvider::isConfigurationValid() const noexcept
{
    return m_presigned->valid;
}

SstvShareAuthenticationStatus
SstvPresignedPutShareProvider::authenticationStatus() const
{
    return m_presigned->targetSource
        ? m_presigned->targetSource->status()
        : SstvShareAuthenticationStatus::Unavailable;
}

SstvShareOperationId SstvPresignedPutShareProvider::createUploadAsync(
    const SstvShareManifestV1& manifest,
    const QString& idempotencyKey,
    SstvShareProviderCompletion completion)
{
    m_presigned->purgeExpired(this, QDateTime::currentDateTimeUtc());
    if (!m_presigned->valid || manifest.providerId != providerId()
        || manifest.chunkCount != 1U
        || !manifest.validate(true, QDateTime::currentDateTimeUtc()).ok()
        || !isLowercaseSha256(idempotencyKey)
        || idempotencyKey != SstvShareTransfer::deriveIdempotencyKey(manifest)) {
        return completeSoon(invalidRequest(QStringLiteral("pre-signed create")),
                            std::move(completion));
    }
    const auto existing = m_presigned->handleByIdempotency.constFind(
        idempotencyKey);
    if (existing != m_presigned->handleByIdempotency.constEnd()) {
        const auto session = m_presigned->sessions.constFind(*existing);
        if (session != m_presigned->sessions.constEnd()) {
            return completeSoon(SstvShareProviderResult::success(
                {session->opaqueId, session->uploaded ? session->totalBytes : 0U}),
                std::move(completion));
        }
    }
    if (const auto* terminal = m_presigned->terminalForIdempotency(
            idempotencyKey)) {
        return completeSoon(SstvShareProviderResult::success(terminal->result),
                            std::move(completion));
    }
    if (m_presigned->sessions.size()
        >= m_presigned->config.maximumActiveSessions) {
        return completeSoon(SstvShareProviderResult::failure(
            SstvShareProviderFailure::ProviderUnavailable,
            QStringLiteral("pre-signed upload session capacity is full")),
            std::move(completion));
    }
    auto target = m_presigned->targetSource->acquireTarget(
        providerId(), manifest.transferId, manifest.byteSize, manifest.sha256);
    if (!target) {
        return completeSoon(SstvShareProviderResult::failure(
            SstvShareProviderFailure::Authentication,
            QStringLiteral("pre-signed target lease is unavailable")),
            std::move(completion));
    }
    const QUrl targetUrl = target->targetUrl();
    if (!endpointAllowed(targetUrl)) {
        return completeSoon(SstvShareProviderResult::failure(
            SstvShareProviderFailure::TlsValidation,
            QStringLiteral("pre-signed target violates transport policy")),
            std::move(completion));
    }
    Impl::Session session;
    session.opaqueId = makeOpaqueUploadId(
        QStringLiteral("presigned-upload"), idempotencyKey);
    session.idempotencyKey = idempotencyKey;
    session.sha256 = manifest.sha256;
    session.mimeType = manifest.mimeType;
    session.totalBytes = manifest.byteSize;
    session.expiresUtc = manifest.expiresUtc;
    session.target = std::move(target);
    m_presigned->handleByIdempotency.insert(idempotencyKey, session.opaqueId);
    m_presigned->sessions.insert(session.opaqueId, session);
    return completeSoon(SstvShareProviderResult::success(
        {session.opaqueId, 0U}), std::move(completion));
}

SstvShareOperationId SstvPresignedPutShareProvider::uploadChunkAsync(
    const SstvShareUploadHandle& handle,
    quint64 offset,
    const QByteArray& chunk,
    const QString& chunkSha256,
    SstvShareProgressCallback progress,
    SstvShareProviderCompletion completion)
{
    m_presigned->purgeExpired(this, QDateTime::currentDateTimeUtc());
    auto session = m_presigned->sessions.find(handle.opaqueId);
    const QByteArray actualHash = sha256Hex(chunk);
    if (!m_presigned->valid || session == m_presigned->sessions.end()
        || session->cancelled || session->completed || session->activeOperation != 0U
        || offset != 0U || handle.committedBytes != 0U
        || static_cast<quint64>(chunk.size()) != session->totalBytes
        || chunkSha256 != session->sha256 || actualHash != chunkSha256.toLatin1()) {
        return completeSoon(
            actualHash != chunkSha256.toLatin1()
                ? SstvShareProviderResult::failure(
                    SstvShareProviderFailure::Integrity,
                    QStringLiteral("pre-signed PUT payload hash mismatch"))
                : invalidRequest(QStringLiteral("pre-signed PUT")),
            std::move(completion));
    }
    if (session->uploaded) {
        return completeSoon(SstvShareProviderResult::success(
            {session->opaqueId, session->totalBytes}), std::move(completion));
    }
    const QString opaqueId = session->opaqueId;
    Request request;
    request.method = "PUT";
    request.url = session->target->targetUrl();
    request.body = chunk;
    request.headers = {
        {"Content-Type", session->mimeType.toLatin1()},
        {"Digest", QByteArray("sha-256=") + sha256Base64(chunk)},
        {"X-Amz-Checksum-Sha256", sha256Base64(chunk)},
    };
    request.acceptedStatusCodes = {200, 201, 204};
    request.diagnosticLabel = QStringLiteral("pre-signed PUT");
    request.explicitLease = session->target;
    request.progress = std::move(progress);
    request.responseHandler = [this, opaqueId](const SstvHttpResponse& response) {
        auto current = m_presigned->sessions.find(opaqueId);
        if (current == m_presigned->sessions.end()) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::NotFound,
                QStringLiteral("pre-signed session disappeared"));
        }
        current->activeOperation = 0U;
        if (m_presigned->config.requireServerSha256
            && !responseContainsExpectedSha256(response, current->sha256)) {
            return SstvShareProviderResult::failure(
                SstvShareProviderFailure::Integrity,
                QStringLiteral("pre-signed server SHA-256 was not verified"));
        }
        current->uploaded = true;
        return SstvShareProviderResult::success(
            {current->opaqueId, current->totalBytes});
    };
    const SstvShareOperationId operation = startRequest(
        std::move(request),
        [this, opaqueId, completion = std::move(completion)](
            SstvShareProviderResult result) mutable {
            auto current = m_presigned->sessions.find(opaqueId);
            if (current != m_presigned->sessions.end()) {
                current->activeOperation = 0U;
            }
            if (completion) {
                completion(std::move(result));
            }
        });
    session = m_presigned->sessions.find(opaqueId);
    if (session != m_presigned->sessions.end()) {
        session->activeOperation = operation;
    }
    return operation;
}

SstvShareOperationId SstvPresignedPutShareProvider::completeUploadAsync(
    const SstvShareUploadHandle& handle,
    const QString& idempotencyKey,
    SstvShareProviderCompletion completion)
{
    m_presigned->purgeExpired(this, QDateTime::currentDateTimeUtc());
    auto session = m_presigned->sessions.find(handle.opaqueId);
    if (!m_presigned->valid) {
        return completeSoon(invalidRequest(QStringLiteral("pre-signed completion")),
                            std::move(completion));
    }
    if (session == m_presigned->sessions.end()) {
        const auto terminal = m_presigned->terminalByOpaqueId.constFind(
            handle.opaqueId);
        if (terminal != m_presigned->terminalByOpaqueId.constEnd()
            && !terminal->cancelled
            && terminal->idempotencyKey == idempotencyKey) {
            return completeSoon(
                SstvShareProviderResult::success(terminal->result),
                std::move(completion));
        }
        return completeSoon(invalidRequest(QStringLiteral("pre-signed completion")),
                            std::move(completion));
    }
    if (session->cancelled || idempotencyKey != session->idempotencyKey
        || !session->uploaded || handle.committedBytes != session->totalBytes) {
        return completeSoon(invalidRequest(QStringLiteral("pre-signed completion")),
                            std::move(completion));
    }
    const SstvShareUploadHandle result {
        session->opaqueId, session->totalBytes};
    m_presigned->recordTerminal(handle.opaqueId, result, false);
    return completeSoon(SstvShareProviderResult::success(result),
                        std::move(completion));
}

SstvShareOperationId SstvPresignedPutShareProvider::cancelUploadAsync(
    const SstvShareUploadHandle& handle,
    SstvShareProviderCompletion completion)
{
    m_presigned->purgeExpired(this, QDateTime::currentDateTimeUtc());
    auto session = m_presigned->sessions.find(handle.opaqueId);
    if (!m_presigned->valid) {
        return completeSoon(invalidRequest(QStringLiteral("pre-signed cancel")),
                            std::move(completion));
    }
    if (session == m_presigned->sessions.end()) {
        const auto terminal = m_presigned->terminalByOpaqueId.constFind(
            handle.opaqueId);
        if (terminal != m_presigned->terminalByOpaqueId.constEnd()) {
            return completeSoon(
                SstvShareProviderResult::success(terminal->result),
                std::move(completion));
        }
        return completeSoon(invalidRequest(QStringLiteral("pre-signed cancel")),
                            std::move(completion));
    }
    if (session->activeOperation != 0U) {
        cancelOperation(session->activeOperation);
        session = m_presigned->sessions.find(handle.opaqueId);
    }
    if (session != m_presigned->sessions.end()) {
        const SstvShareUploadHandle result {
            session->opaqueId,
            session->uploaded ? session->totalBytes : handle.committedBytes};
        m_presigned->recordTerminal(handle.opaqueId, result, true);
    }
    return completeSoon(SstvShareProviderResult::success(handle),
                        std::move(completion));
}

SstvShareOperationId SstvPresignedPutShareProvider::queryStatusAsync(
    const SstvShareUploadHandle& handle,
    SstvShareProviderCompletion completion)
{
    m_presigned->purgeExpired(this, QDateTime::currentDateTimeUtc());
    const auto session = m_presigned->sessions.constFind(handle.opaqueId);
    if (!m_presigned->valid) {
        return completeSoon(invalidRequest(QStringLiteral("pre-signed status")),
                            std::move(completion));
    }
    if (session == m_presigned->sessions.constEnd()) {
        const auto terminal = m_presigned->terminalByOpaqueId.constFind(
            handle.opaqueId);
        if (terminal != m_presigned->terminalByOpaqueId.constEnd()
            && !terminal->cancelled) {
            return completeSoon(
                SstvShareProviderResult::success(terminal->result),
                std::move(completion));
        }
        return completeSoon(invalidRequest(QStringLiteral("pre-signed status")),
                            std::move(completion));
    }
    return completeSoon(SstvShareProviderResult::success(
        {session->opaqueId, session->uploaded ? session->totalBytes : 0U}),
        std::move(completion));
}

} // namespace decodium::sstv::sharing
