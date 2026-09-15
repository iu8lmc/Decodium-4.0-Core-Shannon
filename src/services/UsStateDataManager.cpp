#include "UsStateDataManager.h"

#include <QBuffer>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>

namespace {
constexpr int kRefreshAfterDays = 30;
constexpr int kMaxReasonableStateRows = 1000000;
constexpr int kMaxReasonableGridRows = 2000000;

const QUrl kGridDataUrl(QStringLiteral("https://downloads.sourceforge.net/project/jtdx/grid_data.bin"));
const QUrl kStateDataUrl(QStringLiteral("https://downloads.sourceforge.net/project/jtdx/state_data.bin"));

bool looksLikeCallsignPart(const QString& part)
{
    static const QRegularExpression re(QStringLiteral("^[A-Z0-9]{2,4}[0-9][A-Z0-9]{1,4}$"));
    return re.match(part).hasMatch();
}

bool fileLooksStale(const QString& path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        return true;
    }
    return info.lastModified().daysTo(QDateTime::currentDateTimeUtc()) > kRefreshAfterDays;
}

bool looksLikeQtCompressedMap(const QByteArray& data)
{
    return data.size() > 8
        && static_cast<unsigned char>(data.at(4)) == 0x78;
}

bool readQtCompressedMap(const QString& path,
                         QHash<QString, QString>& output,
                         bool keysAreCalls,
                         int maxRows,
                         QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("cannot open %1: %2").arg(path, file.errorString());
        }
        return false;
    }

    const QByteArray raw = file.readAll();
    if (!looksLikeQtCompressedMap(raw)) {
        if (error) {
            *error = QStringLiteral("invalid binary signature in %1").arg(path);
        }
        return false;
    }
    const QByteArray decompressed = qUncompress(raw);
    if (decompressed.isEmpty()) {
        if (error) {
            *error = QStringLiteral("cannot decompress %1").arg(path);
        }
        return false;
    }

    QBuffer buffer;
    buffer.setData(decompressed);
    if (!buffer.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("cannot open decompressed buffer for %1").arg(path);
        }
        return false;
    }

    QDataStream in(&buffer);
    in.setByteOrder(QDataStream::BigEndian);

    quint32 rowCount = 0;
    in >> rowCount;
    if (in.status() != QDataStream::Ok || rowCount > static_cast<quint32>(maxRows)) {
        if (error) {
            *error = QStringLiteral("invalid row count in %1: %2").arg(path).arg(rowCount);
        }
        return false;
    }

    output.reserve(static_cast<int>(rowCount));
    for (quint32 i = 0; i < rowCount; ++i) {
        QString key;
        QString value;
        in >> key >> value;
        if (in.status() != QDataStream::Ok) {
            if (error) {
                *error = QStringLiteral("truncated data in %1 at row %2").arg(path).arg(i);
            }
            return false;
        }

        key = keysAreCalls ? UsStateDataManager::normalizeCall(key)
                           : UsStateDataManager::normalizeGrid(key);
        value = keysAreCalls ? UsStateDataManager::normalizeGrid(value)
                             : value.trimmed().toUpper();
        if (!key.isEmpty() && !value.isEmpty()) {
            output.insert(key, value);
        }
    }
    return true;
}
} // namespace

UsStateDataManager::UsStateDataManager(QObject* parent)
    : QObject(parent)
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/.decodium");
    }
    m_cacheDir = QDir(base).filePath(QStringLiteral("jtdx-state-data"));
    m_gridPath = QDir(m_cacheDir).filePath(QStringLiteral("grid_data.bin"));
    m_statePath = QDir(m_cacheDir).filePath(QStringLiteral("state_data.bin"));
    m_net = new QNetworkAccessManager(this);
    connect(m_net, &QNetworkAccessManager::finished,
            this, &UsStateDataManager::handleDownloadFinished);
}

UsStateDataManager::~UsStateDataManager()
{
    if (m_parseWorker && m_parseWorker->isRunning()) {
        m_parseWorker->wait();
    }
}

void UsStateDataManager::ensureLoadedAsync()
{
    QDir().mkpath(m_cacheDir);
    const bool haveGrid = QFileInfo::exists(m_gridPath);
    const bool haveState = QFileInfo::exists(m_statePath);
    if (haveGrid && haveState) {
        if (!m_ready && !m_parseInProgress) {
            parseAsync();
        }
        if ((fileLooksStale(m_gridPath) || fileLooksStale(m_statePath)) && m_pendingDownloads == 0) {
            startDownload(false);
        }
        return;
    }
    if (m_pendingDownloads == 0) {
        startDownload(false);
    }
}

void UsStateDataManager::updateNow()
{
    QDir().mkpath(m_cacheDir);
    startDownload(true);
}

void UsStateDataManager::setUpdating(bool updating)
{
    if (m_updating == updating) {
        return;
    }
    m_updating = updating;
    emit updatingChanged();
}

void UsStateDataManager::refreshUpdating()
{
    setUpdating(m_parseInProgress || m_pendingDownloads > 0);
}

void UsStateDataManager::startDownload(bool force)
{
    if (m_pendingDownloads > 0) {
        return;
    }
    if (!force && QFileInfo::exists(m_gridPath) && QFileInfo::exists(m_statePath)
        && !fileLooksStale(m_gridPath) && !fileLooksStale(m_statePath)) {
        return;
    }

    m_downloadFailed = false;
    auto startOne = [this](const QUrl& url, const QString& path) {
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("Decodium/4.0"));
        request.setRawHeader("Accept", "application/octet-stream,*/*;q=0.8");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* reply = m_net->get(request);
        reply->setProperty("targetPath", path);
        ++m_pendingDownloads;
    };

    startOne(kGridDataUrl, m_gridPath);
    startOne(kStateDataUrl, m_statePath);
    refreshUpdating();
    emit statusMessage(QStringLiteral("US state data: download started"));
}

void UsStateDataManager::handleDownloadFinished(QNetworkReply* reply)
{
    const QString path = reply->property("targetPath").toString();
    bool ok = false;
    if (reply->error() == QNetworkReply::NoError && !path.isEmpty()) {
        const QByteArray payload = reply->readAll();
        if (!looksLikeQtCompressedMap(payload)) {
            emit statusMessage(QStringLiteral("US state data: rejected non-binary response for %1").arg(path));
        } else {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QSaveFile file(path);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(payload);
                ok = file.commit();
            }
        }
    }
    if (!ok) {
        m_downloadFailed = true;
        emit statusMessage(QStringLiteral("US state data: download failed for %1").arg(path));
    }

    reply->deleteLater();
    m_pendingDownloads = qMax(0, m_pendingDownloads - 1);
    if (m_pendingDownloads == 0) {
        if (!m_downloadFailed && QFileInfo::exists(m_gridPath) && QFileInfo::exists(m_statePath)) {
            parseAsync();
        } else {
            refreshUpdating();
        }
    } else {
        refreshUpdating();
    }
}

void UsStateDataManager::parseAsync()
{
    if (m_parseInProgress || !QFileInfo::exists(m_gridPath) || !QFileInfo::exists(m_statePath)) {
        return;
    }
    m_parseInProgress = true;
    refreshUpdating();

    const QString gridPath = m_gridPath;
    const QString statePath = m_statePath;
    QPointer<UsStateDataManager> self(this);
    QThread* worker = QThread::create([self, gridPath, statePath]() {
        ParsedData data = parseFiles(gridPath, statePath);
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, data]() {
            if (self) {
                self->applyParsedData(data);
            }
        }, Qt::QueuedConnection);
    });
    m_parseWorker = worker;
    connect(worker, &QThread::finished, this, [this, worker]() {
        if (m_parseWorker == worker) {
            m_parseWorker = nullptr;
        }
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void UsStateDataManager::applyParsedData(const ParsedData& data)
{
    m_parseInProgress = false;
    if (!data.error.isEmpty()) {
        emit statusMessage(QStringLiteral("US state data: %1").arg(data.error));
        refreshUpdating();
        if (!m_redownloadAfterParseFailureAttempted) {
            m_redownloadAfterParseFailureAttempted = true;
            startDownload(true);
        }
        return;
    }

    m_callToGrid = data.callToGrid;
    m_grid6ToState = data.grid6ToState;
    m_grid4ToState = data.grid4ToState;
    m_ready = !m_callToGrid.isEmpty() && !m_grid6ToState.isEmpty();
    m_redownloadAfterParseFailureAttempted = false;
    refreshUpdating();
    emit dataChanged();
    emit statusMessage(QStringLiteral("US state data: loaded %1 calls, %2 locators")
                       .arg(m_callToGrid.size())
                       .arg(m_grid6ToState.size()));
}

UsStateDataManager::ParsedData UsStateDataManager::parseFiles(const QString& gridPath,
                                                              const QString& statePath)
{
    ParsedData data;
    QString error;
    if (!readQtCompressedMap(gridPath, data.callToGrid, true, kMaxReasonableGridRows, &error)) {
        data.error = error;
        return data;
    }

    if (!readQtCompressedMap(statePath, data.grid6ToState, false, kMaxReasonableStateRows, &error)) {
        data.error = error;
        return data;
    }

    QHash<QString, QString> grid4Candidate;
    QSet<QString> ambiguousGrid4;
    for (auto it = data.grid6ToState.cbegin(); it != data.grid6ToState.cend(); ++it) {
        const QString grid6 = normalizeGrid(it.key());
        const QString state = it.value().trimmed().toUpper();
        if (grid6.size() < 6 || state.size() != 2) {
            continue;
        }
        const QString grid4 = grid6.left(4);
        const QString previous = grid4Candidate.value(grid4);
        if (previous.isEmpty()) {
            grid4Candidate.insert(grid4, state);
        } else if (previous != state) {
            ambiguousGrid4.insert(grid4);
        }
    }

    for (auto it = grid4Candidate.cbegin(); it != grid4Candidate.cend(); ++it) {
        if (!ambiguousGrid4.contains(it.key())) {
            data.grid4ToState.insert(it.key(), it.value());
        }
    }
    return data;
}

QString UsStateDataManager::normalizeCall(const QString& call)
{
    QString text = call.trimmed().toUpper();
    text.remove(QLatin1Char('<'));
    text.remove(QLatin1Char('>'));
    text.remove(QLatin1Char(','));
    text.remove(QLatin1Char(';'));
    if (text.isEmpty()) {
        return {};
    }

    const QStringList parts = text.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString best;
    for (const QString& rawPart : parts) {
        const QString part = rawPart.trimmed();
        if (looksLikeCallsignPart(part) && part.size() > best.size()) {
            best = part;
        }
    }
    return best.isEmpty() ? text : best;
}

QString UsStateDataManager::normalizeGrid(const QString& grid)
{
    QString text;
    text.reserve(grid.size());
    for (QChar ch : grid.trimmed().toUpper()) {
        if ((ch >= QLatin1Char('A') && ch <= QLatin1Char('Z'))
            || (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))) {
            text.append(ch);
        }
    }
    if (text.size() < 4) {
        return {};
    }
    return text;
}

QString UsStateDataManager::lookupStateForGrid(const QString& grid) const
{
    const QString normalized = normalizeGrid(grid);
    const QString exactState = m_grid6ToState.value(normalized);
    if (!exactState.isEmpty()) {
        return exactState;
    }
    if (normalized.size() >= 6) {
        const QString state = m_grid6ToState.value(normalized.left(6));
        if (!state.isEmpty()) {
            return state;
        }
    }
    if (normalized.size() >= 4) {
        const QString grid4 = normalized.left(4);
        const QString directState = m_grid6ToState.value(grid4);
        if (!directState.isEmpty()) {
            return directState;
        }
        return m_grid4ToState.value(grid4);
    }
    return {};
}

QString UsStateDataManager::stateForCall(const QString& call, const QString& gridHint) const
{
    if (!m_ready) {
        return {};
    }

    const QString key = normalizeCall(call);
    if (!key.isEmpty()) {
        const QString callState = lookupStateForGrid(m_callToGrid.value(key));
        if (!callState.isEmpty()) {
            return callState;
        }
    }

    return lookupStateForGrid(gridHint);
}
