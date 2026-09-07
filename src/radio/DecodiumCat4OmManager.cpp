#include "DecodiumCat4OmManager.h"

#include "DecodiumProfileSettings.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QtWebSockets/QWebSocket>

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kManagementPort = 5000;
constexpr int kControlPort = 5001;
constexpr int kHandshakeTimeoutMs = 10000;
constexpr int kRequestTimeoutMs = 10000;
constexpr int kSafetyCloseTimeoutMs = 800;

QString requestId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QStringList stringArray(QJsonValue const& value)
{
    QStringList result;
    QJsonArray const array = value.toArray();
    result.reserve(array.size());
    for (QJsonValue const& item : array) {
        QString const text = item.toString().trimmed();
        if (!text.isEmpty() && !result.contains(text, Qt::CaseInsensitive)) {
            result.append(text);
        }
    }
    return result;
}

bool fuzzyDifferent(double lhs, double rhs)
{
    return std::abs(lhs - rhs) > 0.01;
}

QString capabilityForAction(QString const& action)
{
    if (action == QStringLiteral("setFrequency")) return QStringLiteral("SetFrequency");
    if (action == QStringLiteral("setMode")) return QStringLiteral("SetMode");
    if (action == QStringLiteral("setPtt")) return QStringLiteral("SetPtt");
    if (action == QStringLiteral("setVfo")) return QStringLiteral("SetVfo");
    if (action == QStringLiteral("setSplit")) return QStringLiteral("SetSplit");
    if (action == QStringLiteral("setRit")) return QStringLiteral("SetRit");
    if (action == QStringLiteral("setXit")) return QStringLiteral("SetXit");
    return {};
}
}

DecodiumCat4OmManager::DecodiumCat4OmManager(QObject* parent)
    : QObject(parent)
    , m_managementSocket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_controlSocket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_connectTimeout(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
    , m_requestTimer(new QTimer(this))
    , m_safetyCloseTimer(new QTimer(this))
{
    setObjectName(QStringLiteral("decodiumCat4OmManager"));
    m_managementSocket->setObjectName(QStringLiteral("cat4omManagementSocket"));
    m_controlSocket->setObjectName(QStringLiteral("cat4omControlSocket"));
    m_connectTimeout->setObjectName(QStringLiteral("cat4omConnectTimeout"));
    m_reconnectTimer->setObjectName(QStringLiteral("cat4omReconnectTimer"));
    m_requestTimer->setObjectName(QStringLiteral("cat4omRequestTimeoutTimer"));
    m_safetyCloseTimer->setObjectName(QStringLiteral("cat4omSafetyCloseTimer"));
    m_requestClock.start();
    setupSockets();

    m_connectTimeout->setSingleShot(true);
    connect(m_connectTimeout, &QTimer::timeout, this, [this]() {
        if (m_controlReady) {
            return;
        }
        failConnection(tr("CAT4OM: connection handshake timed out."), !m_managementComplete);
    });

    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (m_shouldReconnect && !m_connected && !m_connecting) {
            connectRig();
        }
    });

    m_requestTimer->setInterval(500);
    connect(m_requestTimer, &QTimer::timeout,
            this, &DecodiumCat4OmManager::checkRequestTimeouts);
    m_requestTimer->start();

    m_safetyCloseTimer->setSingleShot(true);
    connect(m_safetyCloseTimer, &QTimer::timeout, this, [this]() {
        if (!m_closeAfterPttOff) {
            return;
        }
        m_closeAfterPttOff = false;
        closeSockets(false);
    });
}

DecodiumCat4OmManager::~DecodiumCat4OmManager()
{
    m_shouldReconnect = false;
    closeSockets(true);
}

void DecodiumCat4OmManager::setupSockets()
{
    connect(m_managementSocket, &QWebSocket::connected, this, [this]() {
        setConnectionDetail(tr("CAT4OM management handshake..."));
        sendHello(m_managementSocket);
    });
    connect(m_managementSocket, &QWebSocket::textMessageReceived,
            this, &DecodiumCat4OmManager::handleManagementMessage);
    connect(m_managementSocket, &QWebSocket::disconnected, this, [this]() {
        if (m_connecting && !m_managementComplete && !m_directFallbackAttempted) {
            failConnection(tr("CAT4OM: management connection closed before discovery."), true);
        }
    });
    connect(m_managementSocket, &QWebSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
        if (m_connecting && !m_managementComplete) {
            failConnection(tr("CAT4OM management: %1").arg(m_managementSocket->errorString()), true);
        }
    });

    connect(m_controlSocket, &QWebSocket::connected, this, [this]() {
        setConnectionDetail(tr("CAT4OM control handshake..."));
        sendHello(m_controlSocket);
    });
    connect(m_controlSocket, &QWebSocket::textMessageReceived,
            this, &DecodiumCat4OmManager::handleControlMessage);
    connect(m_controlSocket, &QWebSocket::disconnected, this, [this]() {
        bool const reconnect = m_shouldReconnect && !m_userDisconnect;
        bool const hadSession = m_controlReady || m_connected;
        resetSessionState();
        setConnecting(false);
        if (reconnect) {
            scheduleReconnect(hadSession
                                  ? tr("control connection dropped")
                                  : tr("control connection failed"));
        } else {
            setConnectionDetail(tr("Disconnected"));
        }
    });
    connect(m_controlSocket, &QWebSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) {
        if (m_userDisconnect) {
            return;
        }
        QString const message = tr("CAT4OM control: %1").arg(m_controlSocket->errorString());
        emit errorOccurred(message);
        setConnectionDetail(message);
    });
}

QUrl DecodiumCat4OmManager::endpointUrl(QString const& endpoint, int defaultPort)
{
    QString value = endpoint.trimmed();
    if (value.isEmpty()) {
        value = QStringLiteral("127.0.0.1:%1").arg(defaultPort);
    }
    if (!value.contains(QStringLiteral("://"))) {
        value.prepend(QStringLiteral("ws://"));
    }

    QUrl url(value);
    if (url.scheme().isEmpty()) {
        url.setScheme(QStringLiteral("ws"));
    }
    if (url.host().isEmpty()) {
        url.setHost(QStringLiteral("127.0.0.1"));
    }
    if (url.port() <= 0) {
        url.setPort(defaultPort);
    }
    if (url.path().isEmpty()) {
        url.setPath(QStringLiteral("/"));
    }
    return url;
}

QString DecodiumCat4OmManager::normalizedEndpoint(QString const& endpoint, int defaultPort)
{
    QUrl const url = endpointUrl(endpoint, defaultPort);
    QString const raw = endpoint.trimmed();
    if (raw.startsWith(QStringLiteral("ws://"), Qt::CaseInsensitive)
        || raw.startsWith(QStringLiteral("wss://"), Qt::CaseInsensitive)) {
        return url.toString(QUrl::RemoveUserInfo | QUrl::StripTrailingSlash);
    }
    return QStringLiteral("%1:%2").arg(url.host()).arg(url.port(defaultPort));
}

void DecodiumCat4OmManager::setManagementEndpoint(QString const& value)
{
    QString const normalized = normalizedEndpoint(value, kManagementPort);
    if (m_managementEndpoint == normalized) {
        return;
    }
    m_managementEndpoint = normalized;
    emit managementEndpointChanged();
}

void DecodiumCat4OmManager::setControlEndpoint(QString const& value)
{
    QString const normalized = normalizedEndpoint(value, kControlPort);
    if (m_controlEndpoint == normalized) {
        return;
    }
    m_controlEndpoint = normalized;
    emit controlEndpointChanged();
    emit networkPortChanged();
}

void DecodiumCat4OmManager::setGroupId(QString const& value)
{
    QString const normalized = value.trimmed();
    if (m_groupId == normalized) {
        return;
    }
    m_groupId = normalized;
    emit groupIdChanged();
}

void DecodiumCat4OmManager::setRadioId(QString const& value)
{
    QString normalized = value.trimmed();
    int const separator = normalized.lastIndexOf(QStringLiteral(" ["));
    if (separator >= 0 && normalized.endsWith(QLatin1Char(']'))) {
        normalized = normalized.mid(separator + 2, normalized.size() - separator - 3).trimmed();
    }
    if (m_radioId == normalized) {
        return;
    }
    m_radioId = normalized;
    emit radioIdChanged();
    emit rigNameChanged();
    applySelectedRadioState();
}

void DecodiumCat4OmManager::setAutoRequestOwnership(bool value)
{
    if (m_autoRequestOwnership == value) {
        return;
    }
    m_autoRequestOwnership = value;
    emit autoRequestOwnershipChanged();
    if (value && m_controlReady && !isMaster()) {
        requestOwnership();
    }
}

void DecodiumCat4OmManager::setSplitMode(QString const& value)
{
    QString normalized = value.trimmed().toLower();
    if (normalized != QStringLiteral("rig") && normalized != QStringLiteral("emulate")) {
        normalized = QStringLiteral("none");
    }
    if (m_splitMode == normalized) {
        return;
    }
    m_splitMode = normalized;
    emit splitModeChanged();
}

void DecodiumCat4OmManager::setCatAutoConnect(bool value)
{
    if (m_catAutoConnect == value) {
        return;
    }
    m_catAutoConnect = value;
    emit catAutoConnectChanged();
}

void DecodiumCat4OmManager::setAudioAutoStart(bool value)
{
    if (m_audioAutoStart == value) {
        return;
    }
    m_audioAutoStart = value;
    emit audioAutoStartChanged();
}

void DecodiumCat4OmManager::setConnected(bool value)
{
    if (m_connected == value) {
        return;
    }
    m_connected = value;
    emit connectedChanged();
}

void DecodiumCat4OmManager::setConnecting(bool value)
{
    if (m_connecting == value) {
        return;
    }
    m_connecting = value;
    emit connectingChanged();
}

void DecodiumCat4OmManager::setOwnershipState(QString const& value)
{
    if (m_ownershipState == value) {
        return;
    }
    m_ownershipState = value;
    emit ownershipStateChanged();
}

void DecodiumCat4OmManager::setConnectionDetail(QString const& value)
{
    if (m_connectionDetail == value) {
        return;
    }
    m_connectionDetail = value;
    emit connectionDetailChanged();
}

void DecodiumCat4OmManager::connectRig()
{
    if (m_connected || m_connecting) {
        return;
    }

    bool const freshRequest = !m_shouldReconnect;
    m_shouldReconnect = true;
    m_userDisconnect = false;
    m_directFallbackAttempted = false;
    m_managementComplete = false;
    m_closeAfterPttOff = false;
    if (freshRequest) {
        m_reconnectAttempt = 0;
    }
    m_reconnectTimer->stop();
    m_safetyCloseTimer->stop();
    resetSessionState();
    setConnecting(true);
    openManagement();
}

void DecodiumCat4OmManager::discover()
{
    if (m_connected || m_connecting) {
        emit statusUpdate(tr("CAT4OM: disconnect before running discovery again."));
        return;
    }
    connectRig();
}

void DecodiumCat4OmManager::disconnectRig()
{
    m_shouldReconnect = false;
    m_userDisconnect = true;
    m_reconnectTimer->stop();
    m_connectTimeout->stop();
    setConnecting(false);
    clearQueuedWrites(tr("disconnect requested"));

    if (m_controlReady && m_pttActive && isMaster()
        && m_controlSocket->state() == QAbstractSocket::ConnectedState) {
        m_closeAfterPttOff = true;
        sendControlRequestNow(QStringLiteral("setPtt"), m_radioId,
                              QJsonObject{{QStringLiteral("enabled"), false}});
        m_safetyCloseTimer->start(kSafetyCloseTimeoutMs);
        setConnectionDetail(tr("CAT4OM: releasing PTT before disconnect..."));
        return;
    }

    closeSockets(false);
    resetSessionState();
    setConnectionDetail(tr("Disconnected"));
}

void DecodiumCat4OmManager::openManagement()
{
    if (m_managementSocket->state() != QAbstractSocket::UnconnectedState) {
        m_managementSocket->abort();
    }
    setConnectionDetail(tr("Connecting to CAT4OM management at %1...").arg(m_managementEndpoint));
    m_connectTimeout->start(kHandshakeTimeoutMs);
    m_managementSocket->open(endpointUrl(m_managementEndpoint, kManagementPort));
}

void DecodiumCat4OmManager::openControl(bool directFallback)
{
    if (m_controlSocket->state() != QAbstractSocket::UnconnectedState) {
        m_controlSocket->abort();
    }
    setConnectionDetail(directFallback
                            ? tr("CAT4OM management unavailable; trying control at %1...").arg(m_controlEndpoint)
                            : tr("Connecting to CAT4OM control at %1...").arg(m_controlEndpoint));
    m_connectTimeout->start(kHandshakeTimeoutMs);
    m_controlSocket->open(endpointUrl(m_controlEndpoint, kControlPort));
}

void DecodiumCat4OmManager::closeSockets(bool abortImmediately)
{
    auto closeOne = [abortImmediately](QWebSocket* socket) {
        if (!socket || socket->state() == QAbstractSocket::UnconnectedState) {
            return;
        }
        if (abortImmediately) {
            socket->abort();
        } else {
            socket->close(QWebSocketProtocol::CloseCodeNormal, QStringLiteral("Decodium disconnect"));
        }
    };
    closeOne(m_managementSocket);
    closeOne(m_controlSocket);
}

void DecodiumCat4OmManager::resetSessionState()
{
    m_connectTimeout->stop();
    m_managementRequests.clear();
    m_controlRequests.clear();
    m_managementClientId.clear();
    m_controlClientId.clear();
    m_masterId.clear();
    m_role.clear();
    m_controlReady = false;
    m_ownershipRequestPending = false;
    m_closeAfterPttOff = false;
    clearFrequencySequencePending();
    m_safetyCloseTimer->stop();
    m_lastRadios = {};
    setConnected(false);
    setOwnershipState(QStringLiteral("disconnected"));

    if (m_frequency != 0.0) { m_frequency = 0.0; emit frequencyChanged(); }
    if (m_txFrequency != 0.0) { m_txFrequency = 0.0; emit txFrequencyChanged(); }
    if (!m_mode.isEmpty()) { m_mode.clear(); emit modeChanged(); }
    if (m_pttActive) { m_pttActive = false; emit pttActiveChanged(); }
    if (m_split) { m_split = false; emit splitChanged(); }
    if (m_powerWatts != 0.0) { m_powerWatts = 0.0; emit powerWattsChanged(); }
    if (m_swr != 0.0) { m_swr = 0.0; emit swrChanged(); }
    if (m_alc != 0.0 || m_alcValid) { m_alc = 0.0; m_alcValid = false; emit alcChanged(); }
    if (!m_availableCommands.isEmpty() || !m_supportedModes.isEmpty()) {
        m_availableCommands.clear();
        m_supportedModes.clear();
        emit capabilitiesChanged();
    }
}

void DecodiumCat4OmManager::scheduleReconnect(QString const& reason)
{
    if (!m_shouldReconnect || m_userDisconnect || m_reconnectTimer->isActive()) {
        return;
    }
    static int const delaysMs[] = {1000, 2500, 5000, 10000, 20000, 30000};
    int const index = std::min(m_reconnectAttempt,
                               int(sizeof(delaysMs) / sizeof(delaysMs[0])) - 1);
    int const delay = delaysMs[index];
    ++m_reconnectAttempt;
    setConnectionDetail(tr("CAT4OM: %1; retry in %2 s.")
                            .arg(reason)
                            .arg(double(delay) / 1000.0, 0, 'f', delay < 1000 ? 1 : 0));
    emit statusUpdate(m_connectionDetail);
    m_reconnectTimer->start(delay);
}

void DecodiumCat4OmManager::failConnection(QString const& message,
                                            bool allowDirectControlFallback)
{
    m_connectTimeout->stop();
    if (allowDirectControlFallback && !m_directFallbackAttempted
        && !m_controlEndpoint.trimmed().isEmpty()) {
        m_directFallbackAttempted = true;
        m_managementComplete = true;
        emit statusUpdate(message + QLatin1Char(' ') +
                          tr("Trying the configured control endpoint."));
        openControl(true);
        return;
    }

    emit errorOccurred(message);
    setConnectionDetail(message);
    setConnecting(false);
    setConnected(false);
    scheduleReconnect(message);
}

void DecodiumCat4OmManager::sendHello(QWebSocket* socket)
{
    QString appVersion = QCoreApplication::applicationVersion().trimmed();
    if (appVersion.isEmpty()) {
        appVersion = QStringLiteral("unknown");
    }
    sendJson(socket, QJsonObject{
                         {QStringLiteral("type"), QStringLiteral("hello")},
                         {QStringLiteral("protocolVersion"), protocolVersion()},
                         {QStringLiteral("appName"), QStringLiteral("Decodium")},
                         {QStringLiteral("appVersion"), appVersion},
                     });
}

void DecodiumCat4OmManager::sendJson(QWebSocket* socket, QJsonObject const& object)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }
    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
}

QString DecodiumCat4OmManager::sendManagementRequest(QString const& action,
                                                      QJsonObject const& params)
{
    if (m_managementClientId.isEmpty()
        || m_managementSocket->state() != QAbstractSocket::ConnectedState) {
        return {};
    }
    QString const id = requestId();
    QJsonObject request{
        {QStringLiteral("type"), QStringLiteral("request")},
        {QStringLiteral("id"), id},
        {QStringLiteral("clientId"), m_managementClientId},
        {QStringLiteral("action"), action},
        {QStringLiteral("params"), params},
    };
    m_managementRequests.insert(id, PendingRequest{action, m_requestClock.elapsed() + kRequestTimeoutMs});
    sendJson(m_managementSocket, request);
    return id;
}

QString DecodiumCat4OmManager::sendControlRequestNow(QString const& action,
                                                     QString const& radioId,
                                                     QJsonObject const& params)
{
    if (!m_controlReady || m_controlClientId.isEmpty()
        || m_controlSocket->state() != QAbstractSocket::ConnectedState) {
        return {};
    }
    QString const id = requestId();
    QJsonObject request{
        {QStringLiteral("type"), QStringLiteral("request")},
        {QStringLiteral("id"), id},
        {QStringLiteral("clientId"), m_controlClientId},
        {QStringLiteral("action"), action},
        {QStringLiteral("params"), params},
    };
    if (!radioId.trimmed().isEmpty()) {
        request.insert(QStringLiteral("radioId"), radioId.trimmed());
    }
    m_controlRequests.insert(id, PendingRequest{action, m_requestClock.elapsed() + kRequestTimeoutMs});
    sendJson(m_controlSocket, request);
    return id;
}

QString DecodiumCat4OmManager::errorText(QJsonObject const& response)
{
    QJsonObject const error = response.value(QStringLiteral("error")).toObject();
    QString const code = error.value(QStringLiteral("code")).toString().trimmed();
    QString const message = error.value(QStringLiteral("message")).toString().trimmed();
    if (!code.isEmpty() && !message.isEmpty()) {
        return QStringLiteral("%1: %2").arg(code, message);
    }
    if (!message.isEmpty()) {
        return message;
    }
    return code.isEmpty() ? QObject::tr("unknown protocol error") : code;
}

void DecodiumCat4OmManager::handleManagementMessage(QString const& text)
{
    QJsonParseError parseError;
    QJsonDocument const document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        failConnection(tr("CAT4OM management sent invalid JSON: %1").arg(parseError.errorString()), true);
        return;
    }

    QJsonObject const message = document.object();
    QString const type = message.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("managementWelcome")) {
        if (message.value(QStringLiteral("endpoint")).toString() != QStringLiteral("management")
            || message.value(QStringLiteral("protocolVersion")).toString() != protocolVersion()) {
            failConnection(tr("CAT4OM management protocol mismatch."), true);
            return;
        }
        m_managementClientId = message.value(QStringLiteral("clientId")).toString();
        if (m_managementClientId.isEmpty()) {
            failConnection(tr("CAT4OM management welcome did not provide a client ID."), true);
            return;
        }
        setConnectionDetail(tr("Discovering CAT4OM groups..."));
        sendManagementRequest(QStringLiteral("getServiceStatus"));
        return;
    }

    if (type == QStringLiteral("error")) {
        failConnection(tr("CAT4OM management rejected the connection: %1")
                           .arg(errorText(message)), true);
        return;
    }

    if (type == QStringLiteral("serviceStatus")) {
        handleServiceStatus(message.value(QStringLiteral("status")).toObject());
        return;
    }

    if (type != QStringLiteral("response")) {
        return;
    }

    QString const id = message.value(QStringLiteral("id")).toString();
    auto const it = m_managementRequests.find(id);
    if (it == m_managementRequests.end()) {
        return;
    }
    QString const action = it->action;
    m_managementRequests.erase(it);
    if (!message.value(QStringLiteral("success")).toBool()) {
        failConnection(tr("CAT4OM management %1 failed: %2")
                           .arg(action, errorText(message)), true);
        return;
    }

    if (action == QStringLiteral("getServiceStatus")) {
        handleServiceStatus(message.value(QStringLiteral("result")).toObject());
    } else if (action == QStringLiteral("startGroup")) {
        setConnectionDetail(tr("CAT4OM group started; refreshing status..."));
        QTimer::singleShot(250, this, [this]() {
            if (m_managementSocket->state() == QAbstractSocket::ConnectedState) {
                sendManagementRequest(QStringLiteral("getServiceStatus"));
            }
        });
    }
}

void DecodiumCat4OmManager::handleServiceStatus(QJsonObject const& status)
{
    QJsonArray const groups = status.value(QStringLiteral("groups")).toArray();
    QStringList groupIds;
    QJsonObject selected;
    QJsonObject firstRunning;
    for (QJsonValue const& value : groups) {
        QJsonObject const group = value.toObject();
        QString const id = group.value(QStringLiteral("id")).toString().trimmed();
        if (id.isEmpty()) {
            continue;
        }
        groupIds.append(id);
        if (id.compare(m_groupId, Qt::CaseInsensitive) == 0) {
            selected = group;
        }
        if (firstRunning.isEmpty() && group.value(QStringLiteral("isRunning")).toBool()) {
            firstRunning = group;
        }
    }
    if (m_groupList != groupIds) {
        m_groupList = groupIds;
        emit groupListChanged();
    }
    if (selected.isEmpty()) {
        selected = !firstRunning.isEmpty() ? firstRunning
                                            : (groups.isEmpty() ? QJsonObject{} : groups.first().toObject());
    }
    if (selected.isEmpty()) {
        failConnection(tr("CAT4OM has no configured radio groups."), true);
        return;
    }

    setGroupId(selected.value(QStringLiteral("id")).toString());
    QJsonArray const radios = selected.value(QStringLiteral("radios")).toArray();
    QStringList discoveredRadios;
    for (QJsonValue const& value : radios) {
        QString const id = value.toObject().value(QStringLiteral("radioId")).toString().trimmed();
        if (!id.isEmpty()) {
            discoveredRadios.append(id);
        }
    }
    if (m_radioList != discoveredRadios) {
        m_radioList = discoveredRadios;
        emit radioListChanged();
        emit rigListChanged();
    }
    if ((m_radioId.isEmpty() || !m_radioList.contains(m_radioId, Qt::CaseInsensitive))
        && !m_radioList.isEmpty()) {
        setRadioId(m_radioList.first());
    }

    if (!selected.value(QStringLiteral("isRunning")).toBool()) {
        setConnectionDetail(tr("Starting CAT4OM group %1...").arg(m_groupId));
        sendManagementRequest(QStringLiteral("startGroup"),
                              QJsonObject{{QStringLiteral("groupId"), m_groupId}});
        return;
    }

    int const port = selected.value(QStringLiteral("controlPort")).toInt(kControlPort);
    QUrl controlUrl = endpointUrl(m_managementEndpoint, kManagementPort);
    controlUrl.setPort(port > 0 ? port : kControlPort);
    controlUrl.setPath(QStringLiteral("/"));
    setControlEndpoint(controlUrl.toString(QUrl::RemoveUserInfo | QUrl::StripTrailingSlash));
    m_managementComplete = true;
    openControl(false);
}

void DecodiumCat4OmManager::handleControlMessage(QString const& text)
{
    QJsonParseError parseError;
    QJsonDocument const document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit errorOccurred(tr("CAT4OM control sent invalid JSON: %1").arg(parseError.errorString()));
        return;
    }

    QJsonObject const message = document.object();
    QString const type = message.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("welcome")) {
        handleControlWelcome(message);
        return;
    }
    if (type == QStringLiteral("error")) {
        failConnection(tr("CAT4OM control rejected the connection: %1")
                           .arg(errorText(message)), false);
        return;
    }
    if (type == QStringLiteral("stateUpdate")) {
        QString const master = message.value(QStringLiteral("masterId")).toString();
        if (m_masterId != master) {
            m_masterId = master;
            m_role = (!m_controlClientId.isEmpty() && m_masterId == m_controlClientId)
                ? QStringLiteral("master") : QStringLiteral("slave");
            setOwnershipState(m_role);
        }
        handleRadioSnapshots(message.value(QStringLiteral("radios")).toArray());
        return;
    }
    if (type == QStringLiteral("event")) {
        if (message.value(QStringLiteral("event")).toString()
            == QStringLiteral("ownershipChanged")) {
            m_masterId = message.value(QStringLiteral("details")).toObject()
                             .value(QStringLiteral("masterId")).toString();
            m_role = (!m_controlClientId.isEmpty() && m_masterId == m_controlClientId)
                ? QStringLiteral("master") : QStringLiteral("slave");
            setOwnershipState(m_role);
            if (isMaster()) {
                m_ownershipRequestPending = false;
                flushQueuedWrites();
            }
        }
        return;
    }
    if (type != QStringLiteral("response")) {
        return;
    }

    QString const id = message.value(QStringLiteral("id")).toString();
    auto const it = m_controlRequests.find(id);
    if (it == m_controlRequests.end()) {
        return;
    }
    QString const action = it->action;
    m_controlRequests.erase(it);
    bool const success = message.value(QStringLiteral("success")).toBool();
    if (!success) {
        QString const reason = errorText(message);
        if (action == QStringLiteral("getOwnership")) {
            m_ownershipRequestPending = false;
            setOwnershipState(QStringLiteral("slave"));
            clearQueuedWrites(reason);
        }
        if (action == QStringLiteral("setVfo")
            || action == QStringLiteral("setFrequency")
            || action == QStringLiteral("setSplit")) {
            clearFrequencySequencePending();
        }
        emit errorOccurred(tr("CAT4OM %1 failed: %2").arg(action, reason));
        if (m_closeAfterPttOff && action == QStringLiteral("setPtt")) {
            m_closeAfterPttOff = false;
            closeSockets(false);
        }
        return;
    }

    if (action == QStringLiteral("getOwnership")) {
        m_ownershipRequestPending = false;
        m_role = message.value(QStringLiteral("result")).toObject()
                     .value(QStringLiteral("role")).toString(QStringLiteral("master"));
        if (m_role == QStringLiteral("master")) {
            m_masterId = m_controlClientId;
        }
        setOwnershipState(isMaster() ? QStringLiteral("master") : m_role);
        if (isMaster()) {
            flushQueuedWrites();
        }
    }

    if (m_closeAfterPttOff && action == QStringLiteral("setPtt")) {
        m_closeAfterPttOff = false;
        m_safetyCloseTimer->stop();
        closeSockets(false);
    }
}

void DecodiumCat4OmManager::handleControlWelcome(QJsonObject const& message)
{
    if (message.value(QStringLiteral("endpoint")).toString() != QStringLiteral("control")
        || message.value(QStringLiteral("protocolVersion")).toString() != protocolVersion()) {
        failConnection(tr("CAT4OM control protocol mismatch."), false);
        return;
    }

    m_controlClientId = message.value(QStringLiteral("clientId")).toString();
    if (m_controlClientId.isEmpty()) {
        failConnection(tr("CAT4OM control welcome did not provide a client ID."), false);
        return;
    }
    QString const welcomedGroup = message.value(QStringLiteral("groupId")).toString().trimmed();
    if (!welcomedGroup.isEmpty()) {
        setGroupId(welcomedGroup);
    }
    m_role = message.value(QStringLiteral("role")).toString(QStringLiteral("slave"));
    m_masterId = m_role == QStringLiteral("master") ? m_controlClientId : QString{};
    m_controlReady = true;
    m_connectTimeout->stop();
    setConnecting(false);
    m_reconnectAttempt = 0;
    setOwnershipState(isMaster() ? QStringLiteral("master") : QStringLiteral("slave"));
    handleRadioSnapshots(message.value(QStringLiteral("radios")).toArray());

    if (m_managementSocket->state() != QAbstractSocket::UnconnectedState) {
        m_managementSocket->close(QWebSocketProtocol::CloseCodeNormal,
                                  QStringLiteral("Discovery complete"));
    }
    if (m_autoRequestOwnership && !isMaster()) {
        requestOwnership();
    }
    emit statusUpdate(tr("CAT4OM connected to group %1, radio %2 (%3).")
                          .arg(m_groupId,
                               m_radioId.isEmpty() ? tr("not selected") : m_radioId,
                               m_ownershipState));
}

void DecodiumCat4OmManager::handleRadioSnapshots(QJsonArray const& radios)
{
    m_lastRadios = radios;
    QStringList radioIds;
    for (QJsonValue const& value : radios) {
        QString const id = value.toObject().value(QStringLiteral("radioId")).toString().trimmed();
        if (!id.isEmpty()) {
            radioIds.append(id);
        }
    }
    if (m_radioList != radioIds) {
        m_radioList = radioIds;
        emit radioListChanged();
        emit rigListChanged();
    }
    if ((m_radioId.isEmpty() || !m_radioList.contains(m_radioId, Qt::CaseInsensitive))
        && !m_radioList.isEmpty()) {
        setRadioId(m_radioList.first());
    } else {
        applySelectedRadioState();
    }
}

void DecodiumCat4OmManager::applySelectedRadioState()
{
    QJsonObject selected;
    for (QJsonValue const& value : m_lastRadios) {
        QJsonObject const radio = value.toObject();
        if (radio.value(QStringLiteral("radioId")).toString()
                .compare(m_radioId, Qt::CaseInsensitive) == 0) {
            selected = radio;
            break;
        }
    }
    if (selected.isEmpty()) {
        setConnected(false);
        return;
    }

    QString const displayName = selected.value(QStringLiteral("radioName")).toString().trimmed();
    if (m_radioDisplayName != displayName) {
        m_radioDisplayName = displayName;
        emit radioDisplayNameChanged();
    }

    m_radioConnectionStatus = selected.value(QStringLiteral("connectionStatus")).toString();
    if (m_radioConnectionStatus.isEmpty()) {
        m_radioConnectionStatus = selected.value(QStringLiteral("connectionState")).toString();
    }
    m_availableVfos = stringArray(selected.value(QStringLiteral("availableVfos")));
    bool const split = selected.value(QStringLiteral("split")).toBool();
    m_activeVfo = selected.value(QStringLiteral("activeVfo")).toString();
    m_txVfo = selected.value(QStringLiteral("txVfo")).toString();
    QJsonObject const vfos = selected.value(QStringLiteral("vfos")).toObject();
    if (m_activeVfo.isEmpty() && !vfos.isEmpty()) {
        m_activeVfo = vfos.constBegin().key();
    }
    if (m_txVfo.isEmpty()) {
        m_txVfo = m_activeVfo;
    }

    // Some CAT4OM radio handbooks (including the IC-7300 split model) report
    // the selected VFO as txVfo even though split TX is carried by the other
    // VFO.  When split is enabled and both values are equal, infer the other
    // available VFO so Decodium does not repeatedly re-enter the A/B switch
    // sequence on every state update.
    if (split && m_txVfo.compare(m_activeVfo, Qt::CaseInsensitive) == 0) {
        for (QString const& candidate : m_availableVfos) {
            if (candidate.compare(m_activeVfo, Qt::CaseInsensitive) != 0) {
                m_txVfo = candidate;
                break;
            }
        }
    }

    QJsonObject const activeState = vfos.value(m_activeVfo).toObject();
    double const frequency = activeState.value(QStringLiteral("frequency")).toDouble();
    QString const mode = activeState.value(QStringLiteral("mode")).toString();
    double const txFrequency = split
        ? vfos.value(m_txVfo).toObject().value(QStringLiteral("frequency")).toDouble()
        : 0.0;
    bool const ptt = selected.value(QStringLiteral("ptt")).toBool();

    if (fuzzyDifferent(m_frequency, frequency)) { m_frequency = frequency; emit frequencyChanged(); }
    if (fuzzyDifferent(m_txFrequency, txFrequency)) { m_txFrequency = txFrequency; emit txFrequencyChanged(); }
    if (m_mode != mode) { m_mode = mode; emit modeChanged(); }
    if (m_split != split) { m_split = split; emit splitChanged(); }
    if (m_pttActive != ptt) { m_pttActive = ptt; emit pttActiveChanged(); }

    QJsonObject const metering = selected.value(QStringLiteral("metering")).toObject();
    double const power = metering.value(QStringLiteral("power")).toDouble();
    double const swr = metering.value(QStringLiteral("swr")).toDouble();
    bool const alcValid = metering.contains(QStringLiteral("alc"))
        && !metering.value(QStringLiteral("alc")).isNull();
    double const alc = alcValid ? metering.value(QStringLiteral("alc")).toDouble() : 0.0;
    if (fuzzyDifferent(m_powerWatts, power)) { m_powerWatts = power; emit powerWattsChanged(); }
    if (fuzzyDifferent(m_swr, swr)) { m_swr = swr; emit swrChanged(); }
    if (fuzzyDifferent(m_alc, alc) || m_alcValid != alcValid) {
        m_alc = alc;
        m_alcValid = alcValid;
        emit alcChanged();
    }

    // A split-frequency update may temporarily select the TX VFO so CAT4OM
    // can program it, then restore the original RX VFO.  Do not let the
    // intermediate state trigger another sync in the opposite direction.
    if (m_frequencySequencePending
        && !fuzzyDifferent(m_frequencySequenceTargetHz, txFrequency)
        && m_txVfo.compare(m_frequencySequenceTargetVfo, Qt::CaseInsensitive) == 0
        && m_activeVfo.compare(m_frequencySequenceRestoreVfo, Qt::CaseInsensitive) == 0
        && split) {
        clearFrequencySequencePending();
    }

    QStringList const commands = stringArray(selected.value(QStringLiteral("availableCommands")));
    QStringList const modes = stringArray(selected.value(QStringLiteral("supportedModes")));
    if (m_availableCommands != commands || m_supportedModes != modes) {
        m_availableCommands = commands;
        m_supportedModes = modes;
        emit capabilitiesChanged();
    }

    bool const radioConnected = m_controlReady
        && m_radioConnectionStatus.compare(QStringLiteral("connected"), Qt::CaseInsensitive) == 0;
    setConnected(radioConnected);
    if (radioConnected) {
        setConnectionDetail(tr("Connected — %1/%2 — %3")
                                .arg(m_groupId, m_radioId, m_ownershipState));
    } else {
        setConnectionDetail(tr("CAT4OM radio %1: %2")
                                .arg(m_radioId,
                                     m_radioConnectionStatus.isEmpty()
                                         ? tr("state unknown") : m_radioConnectionStatus));
    }

    if (m_closeAfterPttOff && !m_pttActive) {
        m_closeAfterPttOff = false;
        m_safetyCloseTimer->stop();
        closeSockets(false);
    }
}

bool DecodiumCat4OmManager::isMaster() const
{
    return !m_controlClientId.isEmpty()
        && (m_role.compare(QStringLiteral("master"), Qt::CaseInsensitive) == 0
            || m_masterId == m_controlClientId);
}

bool DecodiumCat4OmManager::commandAvailable(QString const& action) const
{
    QString const capability = capabilityForAction(action);
    return capability.isEmpty() || m_availableCommands.isEmpty()
        || m_availableCommands.contains(capability, Qt::CaseInsensitive);
}

bool DecodiumCat4OmManager::canPtt() const
{
    return m_connected && commandAvailable(QStringLiteral("setPtt"))
        && (isMaster() || m_autoRequestOwnership);
}

void DecodiumCat4OmManager::requestOwnership()
{
    if (!m_controlReady || isMaster() || m_ownershipRequestPending) {
        return;
    }
    m_ownershipRequestPending = true;
    setOwnershipState(QStringLiteral("requesting"));
    sendControlRequestNow(QStringLiteral("getOwnership"), QString{});
}

void DecodiumCat4OmManager::queueOrSendWrite(QString const& action,
                                             QJsonObject const& params)
{
    if (!m_controlReady || !m_connected) {
        emit errorOccurred(tr("CAT4OM: radio %1 is not connected.").arg(m_radioId));
        return;
    }
    if (!commandAvailable(action)) {
        emit errorOccurred(tr("CAT4OM: radio %1 does not expose %2.")
                               .arg(m_radioId, capabilityForAction(action)));
        return;
    }
    if (isMaster()) {
        sendControlRequestNow(action, m_radioId, params);
        return;
    }
    if (!m_autoRequestOwnership) {
        emit errorOccurred(tr("CAT4OM: Decodium is read-only; control ownership is held by another client."));
        return;
    }

    // Keep only the most recent pending value for the same action/VFO. This
    // prevents UI frequency changes from growing an unbounded pre-ownership
    // queue while preserving the order of split/PTT operations.
    QString const vfo = params.value(QStringLiteral("vfo")).toString();
    for (int i = m_queuedWrites.size() - 1; i >= 0; --i) {
        QueuedWrite const& queued = m_queuedWrites.at(i);
        if (queued.action == action
            && queued.params.value(QStringLiteral("vfo")).toString() == vfo) {
            m_queuedWrites.removeAt(i);
        }
    }
    while (m_queuedWrites.size() >= 64) {
        m_queuedWrites.dequeue();
    }
    m_queuedWrites.enqueue(QueuedWrite{action, m_radioId, params});
    requestOwnership();
}

void DecodiumCat4OmManager::flushQueuedWrites()
{
    if (!isMaster()) {
        return;
    }
    while (!m_queuedWrites.isEmpty()) {
        QueuedWrite const queued = m_queuedWrites.dequeue();
        sendControlRequestNow(queued.action, queued.radioId, queued.params);
    }
}

void DecodiumCat4OmManager::clearQueuedWrites(QString const& reason)
{
    if (m_queuedWrites.isEmpty()) {
        return;
    }
    int const count = m_queuedWrites.size();
    m_queuedWrites.clear();
    emit statusUpdate(tr("CAT4OM: discarded %1 queued command(s): %2")
                          .arg(count).arg(reason));
}

QString DecodiumCat4OmManager::controlTargetVfo(bool transmit) const
{
    QString vfo = transmit ? m_txVfo : m_activeVfo;
    if (vfo.isEmpty() && !m_availableVfos.isEmpty()) {
        vfo = m_availableVfos.first();
    }
    return vfo;
}

void DecodiumCat4OmManager::clearFrequencySequencePending()
{
    m_frequencySequencePending = false;
    m_frequencySequenceTargetHz = 0.0;
    m_frequencySequenceTargetVfo.clear();
    m_frequencySequenceRestoreVfo.clear();
}

void DecodiumCat4OmManager::queueFrequencyOnVfo(double hz, QString const& targetVfo)
{
    if (hz <= 0.0) {
        return;
    }

    QString const target = targetVfo.trimmed();
    QString const active = m_activeVfo.trimmed();

    // CAT4OM radios expose multiple logical VFOs, but many CAT drivers can
    // change frequency only on the currently selected VFO.  In that case a
    // direct setFrequency(vfo=VFOB) is rejected with INVALID_TARGET_VFO.
    // Select the target first, apply the frequency, then restore the VFO that
    // was active so RX and the user's radio state are not left on the TX VFO.
    if (!target.isEmpty()
        && !active.isEmpty()
        && target.compare(active, Qt::CaseInsensitive) != 0
        && commandAvailable(QStringLiteral("setVfo"))) {
        queueOrSendWrite(QStringLiteral("setVfo"),
                         QJsonObject{{QStringLiteral("vfo"), target}});
    }

    QJsonObject params{{QStringLiteral("frequency"), qRound64(hz)}};
    // Omitting vfo is intentional for the active VFO.  CAT4OM then uses the
    // currently selected VFO, which also covers radios that reject an
    // explicit target even when it names the active VFO.
    if (!target.isEmpty()
        && (active.isEmpty()
            || target.compare(active, Qt::CaseInsensitive) != 0)
        && !commandAvailable(QStringLiteral("setVfo"))) {
        params.insert(QStringLiteral("vfo"), target);
    }
    queueOrSendWrite(QStringLiteral("setFrequency"), params);

    if (!target.isEmpty()
        && !active.isEmpty()
        && target.compare(active, Qt::CaseInsensitive) != 0
        && commandAvailable(QStringLiteral("setVfo"))) {
        queueOrSendWrite(QStringLiteral("setVfo"),
                         QJsonObject{{QStringLiteral("vfo"), active}});
    }
}

void DecodiumCat4OmManager::setRigFrequency(double hz)
{
    if (hz <= 0.0) {
        return;
    }
    queueFrequencyOnVfo(hz, controlTargetVfo(false));
}

void DecodiumCat4OmManager::setRigTxFrequency(double hz)
{
    // "Fake It" is an audio-side split.  It must never change the physical
    // IC-7300 split state; only the explicit "Rig" mode is allowed to send
    // setSplit(true) and program the second VFO.
    if (m_splitMode == QStringLiteral("emulate")) {
        clearFrequencySequencePending();
        if (m_split || m_txFrequency > 0.0) {
            queueOrSendWrite(QStringLiteral("setSplit"),
                             QJsonObject{{QStringLiteral("enabled"), false}});
        }
        return;
    }

    if (m_splitMode == QStringLiteral("none") || hz <= 0.0) {
        clearFrequencySequencePending();
        if (m_split || m_txFrequency > 0.0) {
            queueOrSendWrite(QStringLiteral("setSplit"),
                             QJsonObject{{QStringLiteral("enabled"), false}});
        }
        return;
    }

    QString txVfo = controlTargetVfo(true);
    if (txVfo.isEmpty() || txVfo.compare(m_activeVfo, Qt::CaseInsensitive) == 0) {
        for (QString const& candidate : m_availableVfos) {
            if (candidate.compare(m_activeVfo, Qt::CaseInsensitive) != 0) {
                txVfo = candidate;
                break;
            }
        }
    }
    if (txVfo.isEmpty()) {
        txVfo = m_activeVfo;
    }

    // State updates emitted while the VFO programming sequence is in flight
    // are informational only.  Re-entering here would alternate VFO A/B.
    if (m_frequencySequencePending) {
        return;
    }

    // The requested split state is already applied.  Avoid sending the same
    // CAT4OM command again on every bridge synchronization tick.
    if (m_split
        && !fuzzyDifferent(m_txFrequency, hz)
        && m_txVfo.compare(txVfo, Qt::CaseInsensitive) == 0) {
        return;
    }

    QString const activeBeforeSequence = m_activeVfo;
    if (m_controlReady && m_connected
        && !txVfo.isEmpty()
        && !activeBeforeSequence.isEmpty()
        && txVfo.compare(activeBeforeSequence, Qt::CaseInsensitive) != 0
        && commandAvailable(QStringLiteral("setVfo"))) {
        m_frequencySequencePending = true;
        m_frequencySequenceTargetHz = hz;
        m_frequencySequenceTargetVfo = txVfo;
        m_frequencySequenceRestoreVfo = activeBeforeSequence;
    }

    queueFrequencyOnVfo(hz, txVfo);

    QJsonObject splitParams{{QStringLiteral("enabled"), true}};
    if (!txVfo.isEmpty()) {
        splitParams.insert(QStringLiteral("txVfo"), txVfo);
    }
    queueOrSendWrite(QStringLiteral("setSplit"), splitParams);
}

void DecodiumCat4OmManager::setRigTxFrequencyAndPtt(double hz, bool on)
{
    setRigTxFrequencyAndPttAsync(hz, on);
}

void DecodiumCat4OmManager::setRigTxFrequencyAndPttAsync(double hz, bool on)
{
    if (on && hz > 0.0) {
        setRigTxFrequency(hz);
    }
    setRigPtt(on);
}

QString DecodiumCat4OmManager::selectSupportedMode(QString const& requested) const
{
    QString const upper = requested.trimmed().toUpper();
    QStringList candidates;
    if (upper == QStringLiteral("DATA-U") || upper == QStringLiteral("DATA/PACKET")
        || upper == QStringLiteral("DATA/PKT") || upper == QStringLiteral("USB-D")
        || upper == QStringLiteral("DIGU") || upper == QStringLiteral("PKT-U")) {
        candidates = {QStringLiteral("DATA-U"), QStringLiteral("USB-D"),
                      QStringLiteral("DIGU"), QStringLiteral("PKT-U"),
                      QStringLiteral("DATA"), QStringLiteral("USB")};
    } else {
        candidates = {upper};
    }
    if (m_supportedModes.isEmpty()) {
        return candidates.first();
    }
    for (QString const& candidate : candidates) {
        for (QString const& supported : m_supportedModes) {
            if (supported.compare(candidate, Qt::CaseInsensitive) == 0) {
                return supported;
            }
        }
    }
    return {};
}

void DecodiumCat4OmManager::setRigMode(QString const& mode)
{
    QString const selected = selectSupportedMode(mode);
    if (selected.isEmpty()) {
        emit errorOccurred(tr("CAT4OM: mode %1 is not supported by radio %2.")
                               .arg(mode, m_radioId));
        return;
    }
    queueOrSendWrite(QStringLiteral("setMode"),
                     QJsonObject{{QStringLiteral("mode"), selected}});
}

void DecodiumCat4OmManager::setRigPtt(bool on)
{
    queueOrSendWrite(QStringLiteral("setPtt"),
                     QJsonObject{{QStringLiteral("enabled"), on}});
}

void DecodiumCat4OmManager::checkRequestTimeouts()
{
    qint64 const now = m_requestClock.elapsed();
    auto expire = [this, now](QHash<QString, PendingRequest>& requests, QString const& surface) {
        QStringList expired;
        for (auto it = requests.constBegin(); it != requests.constEnd(); ++it) {
            if (it->deadlineMs <= now) {
                expired.append(it.key());
            }
        }
        for (QString const& id : expired) {
            PendingRequest const request = requests.take(id);
            if (request.action == QStringLiteral("getOwnership")) {
                m_ownershipRequestPending = false;
                setOwnershipState(QStringLiteral("slave"));
                clearQueuedWrites(tr("ownership request timed out"));
            }
            emit errorOccurred(tr("CAT4OM %1 request timed out: %2")
                                   .arg(surface, request.action));
        }
    };
    expire(m_managementRequests, tr("management"));
    expire(m_controlRequests, tr("control"));
}

void DecodiumCat4OmManager::saveSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.beginGroup(QStringLiteral("CAT_Cat4OM"));
    settings.setValue(QStringLiteral("managementEndpoint"), m_managementEndpoint);
    settings.setValue(QStringLiteral("controlEndpoint"), m_controlEndpoint);
    settings.setValue(QStringLiteral("groupId"), m_groupId);
    settings.setValue(QStringLiteral("radioId"), m_radioId);
    settings.setValue(QStringLiteral("autoRequestOwnership"), m_autoRequestOwnership);
    settings.setValue(QStringLiteral("splitMode"), m_splitMode);
    settings.setValue(QStringLiteral("catAutoConnect"), m_catAutoConnect);
    settings.setValue(QStringLiteral("audioAutoStart"), m_audioAutoStart);
    settings.endGroup();
}

void DecodiumCat4OmManager::loadSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    decodium::beginActiveSettingsProfile(settings);
    settings.beginGroup(QStringLiteral("CAT_Cat4OM"));
    setManagementEndpoint(settings.value(QStringLiteral("managementEndpoint"),
                                         m_managementEndpoint).toString());
    setControlEndpoint(settings.value(QStringLiteral("controlEndpoint"),
                                      m_controlEndpoint).toString());
    setGroupId(settings.value(QStringLiteral("groupId"), m_groupId).toString());
    setRadioId(settings.value(QStringLiteral("radioId"), m_radioId).toString());
    setAutoRequestOwnership(settings.value(QStringLiteral("autoRequestOwnership"), true).toBool());
    setCatAutoConnect(settings.value(QStringLiteral("catAutoConnect"), false).toBool());
    setAudioAutoStart(settings.value(QStringLiteral("audioAutoStart"), false).toBool());
    setSplitMode(settings.value(QStringLiteral("splitMode"), QStringLiteral("none")).toString());
    settings.endGroup();
}
