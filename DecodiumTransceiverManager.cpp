// DecodiumTransceiverManager.cpp
// Wrapper QML-friendly attorno a TransceiverFactory (Hamlib, OmniRig, HRD, DXLab, TCI).

#include "DecodiumTransceiverManager.h"

#include "Transceiver/TransceiverFactory.hpp"
#include "Transceiver/Transceiver.hpp"
#include "Transceiver/TransceiverBase.hpp"

#include <QByteArray>
#include <QElapsedTimer>
#include <QThread>
#include <QSerialPortInfo>
#include <QSettings>
#include <QMetaObject>
#include <QMetaType>
#include <QDebug>
#include <QRegularExpression>
#include <algorithm>
#include <atomic>
#include <stdexcept>
#include <hamlib/rig.h>

namespace
{
// Estrae il nome di una porta COM (es. "COM5") dal blob di errore hamlib
// che può contenere path tipo "\\.\COM5" o "/dev/ttyUSB0".
QString extractPortNameFromReason(QString const& reason)
{
    static QRegularExpression const rxWin(QStringLiteral(R"((COM\d+))"),
                                          QRegularExpression::CaseInsensitiveOption);
    auto m = rxWin.match(reason);
    if (m.hasMatch())
        return m.captured(1).toUpper();
    static QRegularExpression const rxUnix(QStringLiteral(R"((/dev/[A-Za-z0-9._-]+))"));
    m = rxUnix.match(reason);
    if (m.hasMatch())
        return m.captured(1);
    return QString();
}

// Hamlib 4.x pollute rigerror() con l'ultimo messaggio rig_debug() invece
// dello strerror del codice di errore. Quando un'operazione fallisce, il
// motivo mostrato all'utente può essere una stringa di debug interna tipo
// "read_string_generic called, rxmax=129 direct=1, expected_len=1" — che
// non è un errore vero ma solo trace di I/O seriale. Questa funzione la
// rileva e la sostituisce con un testo piu' comprensibile.
QString sanitizeHamlibFailure(QString const& reason)
{
    // Prima cerchiamo pattern di porta seriale occupata (il dump hamlib
    // tipicamente contiene tutto il trace di debug seguito dall'errore
    // reale "serial port \\.\COMx is already open" / "Access denied").
    static QStringList const busyMarkers = {
        QStringLiteral("is already open"),
        QStringLiteral("Access denied"),
        QStringLiteral("Permission denied"),
        QStringLiteral("returning2(-22)"),
        QStringLiteral("EACCES"),
        QStringLiteral("EBUSY"),
    };
    for (auto const& marker : busyMarkers) {
        if (reason.contains(marker, Qt::CaseInsensitive)) {
            QString port = extractPortNameFromReason(reason);
            if (port.isEmpty()) {
                return QObject::tr(
                    "Porta seriale occupata da un altro software "
                    "(probabilmente OmniRig, WSJT-X, FLDigi o un terminale seriale). "
                    "Chiudi il programma che sta usando la porta e riprova.");
            }
            return QObject::tr(
                "Porta %1 occupata da un altro software "
                "(probabilmente OmniRig, WSJT-X, FLDigi o un terminale seriale). "
                "Chiudi il programma che sta usando la porta e riprova.").arg(port);
        }
    }

    static QStringList const debugMarkers = {
        QStringLiteral("read_string_generic"),
        QStringLiteral("write_block"),
        QStringLiteral("tn_"),
        QStringLiteral("rig_flush"),
        QStringLiteral("serial_flush"),
        QStringLiteral("ser_set_"),
        QStringLiteral("ser_get_"),
    };
    for (auto const& marker : debugMarkers) {
        if (reason.contains(marker, Qt::CaseInsensitive)) {
            return QObject::tr("Comunicazione CAT interrotta con il rig. "
                               "Verifica cavo USB, porta COM, baud rate e che "
                               "il rig sia acceso. (trace hamlib: %1)").arg(reason);
        }
    }
    return reason;
}

QString normalizeDevicePath(QString value)
{
    value = value.trimmed();
    if (value.isEmpty() || value == "CAT" || value == "None")
        return value;

#if defined(Q_OS_WIN)
    return value;
#else
    if (value.startsWith('/'))
        return value;
    if (value.contains(':'))
        return value;
    return QStringLiteral("/dev/") + value;
#endif
}

QString comparablePortName(QString value)
{
    value = normalizeDevicePath(value);
    if (value.isEmpty() || value == "CAT" || value == "None")
        return value;
    if (value.startsWith(QStringLiteral("\\\\.\\")))
        value.remove(0, 4);
    if (value.startsWith(QStringLiteral("/dev/")))
        value.remove(0, 5);
    return value.toLower();
}

bool pttPortSharesCatPort(QString const& serialPort, QString const& pttPort)
{
    QString const trimmedPttPort = pttPort.trimmed();
    if (trimmedPttPort.isEmpty()
        || 0 == trimmedPttPort.compare(QStringLiteral("CAT"), Qt::CaseInsensitive)) {
        return true;
    }

    return comparablePortName(trimmedPttPort) == comparablePortName(serialPort);
}

QString defaultNetworkEndpointForRig(QString const& rigName)
{
    if (0 == rigName.compare(QStringLiteral("Ham Radio Deluxe"), Qt::CaseInsensitive))
        return QStringLiteral("127.0.0.1:7809");
    if (0 == rigName.compare(QStringLiteral("DX Lab Suite Commander"), Qt::CaseInsensitive))
        return QStringLiteral("127.0.0.1:52002");
    return QString();
}

bool isLegacyNetworkEndpoint(QString const& endpoint)
{
    QString const value = endpoint.trimmed().toLower();
    return value.isEmpty()
        || value == QStringLiteral("localhost:4532")
        || value == QStringLiteral("127.0.0.1:4532")
        || value == QStringLiteral("[::1]:4532");
}

QString normalizedRigIdentity(QString value)
{
    value = value.toUpper();
    value.remove(QRegularExpression(QStringLiteral("[\\s_-]+")));
    return value;
}

int parseCivAddressText(QByteArray const& raw)
{
    QString text = QString::fromLatin1(raw.constData()).trimmed();
    if (text.isEmpty()) {
        return 0;
    }

    bool ok = false;
    int value = text.toInt(&ok, 0);
    if (!ok && text.endsWith(QLatin1Char('H'), Qt::CaseInsensitive)) {
        text.chop(1);
        value = text.toInt(&ok, 16);
    }
    if (!ok) {
        return 0;
    }
    return qBound(0, value, 0xff);
}

int queryHamlibDefaultCivAddress(unsigned modelNumber)
{
    // Non-Hamlib pseudo models live above this range in TransceiverFactory.cpp.
    if (modelNumber == 0 || modelNumber >= 90000) {
        return 0;
    }

    RIG* rig = rig_init(static_cast<rig_model_t>(modelNumber));
    if (!rig) {
        return 0;
    }

    int result = 0;
    token_t const token = rig_token_lookup(rig, "civaddr");
    if (token != RIG_CONF_END) {
        QByteArray value(128, '\0');
#if HAVE_HAMLIB_GET_CONF2
        int const rc = rig_get_conf2(rig, token, value.data(), value.size());
#else
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
        int const rc = rig_get_conf(rig, token, value.data());
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
#endif
        if (rc == RIG_OK) {
            result = parseCivAddressText(value);
        }
    }

    rig_cleanup(rig);
    return result;
}

int fallbackIcomCivAddress(QString const& rigName)
{
    QString const normalized = normalizedRigIdentity(rigName);
    struct Entry { const char* match; int value; };
    static Entry const table[] = {
        {"IC7300MK2", 0xB6},
        {"IC7300",    0x94},
        {"IC7600",    0x7A},
        {"IC7610",    0x98},
        {"IC9700",    0xA2},
        {"IC705",     0xA4},
    };

    for (auto const& entry : table) {
        if (normalized.contains(QLatin1String(entry.match))) {
            return entry.value;
        }
    }
    return 0;
}

int defaultCivAddressForRig(QString const& rigName, TransceiverFactory const& factory)
{
    auto const it = factory.supported_transceivers().constFind(rigName);
    if (it != factory.supported_transceivers().cend()) {
        int const hamlibDefault = queryHamlibDefaultCivAddress(it.value().model_number_);
        if (hamlibDefault > 0) {
            return hamlibDefault;
        }
    }
    return fallbackIcomCivAddress(rigName);
}

bool isTransientCatIoFailure(QString const& reason)
{
    QString const lower = reason.toLower();
    static QStringList const markers = {
        QStringLiteral("io error"),
        QStringLiteral("input/output error"),
        QStringLiteral("device not configured"),
        QStringLiteral("no such device"),
        QStringLiteral("device unavailable"),
        QStringLiteral("resource temporarily unavailable"),
        QStringLiteral("temporarily unavailable"),
        QStringLiteral("timed out"),
        QStringLiteral("timeout"),
        QStringLiteral("rig_get_ptt"),
        QStringLiteral("write_block"),
        QStringLiteral("read_block"),
        QStringLiteral("returning(-6)"),
        QStringLiteral("returning (-6)"),
    };
    for (QString const& marker : markers) {
        if (lower.contains(marker)) {
            return true;
        }
    }
    return false;
}

bool d3ForceDtrAvailable(QString const& portType, QString const& pttMethod,
                         QString const& serialPort, QString const& pttPort)
{
    bool const serialCat = 0 == portType.compare(QStringLiteral("serial"), Qt::CaseInsensitive);
    return serialCat
        && !(0 == pttMethod.compare(QStringLiteral("DTR"), Qt::CaseInsensitive)
             && pttPortSharesCatPort(serialPort, pttPort));
}

bool d3ForceRtsAvailable(QString const& portType, QString const& handshake,
                         QString const& pttMethod, QString const& serialPort,
                         QString const& pttPort)
{
    Q_UNUSED(handshake);
    bool const serialCat = 0 == portType.compare(QStringLiteral("serial"), Qt::CaseInsensitive);
    return serialCat
        && !(0 == pttMethod.compare(QStringLiteral("RTS"), Qt::CaseInsensitive)
             && pttPortSharesCatPort(serialPort, pttPort));
}

QString normalizedSerialPortName(QString value)
{
    value = value.trimmed();
    if (value.isEmpty()
        || 0 == value.compare(QStringLiteral("CAT"), Qt::CaseInsensitive)
        || 0 == value.compare(QStringLiteral("None"), Qt::CaseInsensitive)) {
        return {};
    }
    if (value.startsWith(QStringLiteral("\\\\.\\"))) {
        value.remove(0, 4);
    }
#if defined(Q_OS_WIN)
    static QRegularExpression const rx(QStringLiteral(R"(^COM(\d+)$)"),
                                       QRegularExpression::CaseInsensitiveOption);
    auto const match = rx.match(value);
    if (match.hasMatch()) {
        return QStringLiteral("COM") + match.captured(1);
    }
#endif
    return value;
}

void appendUniqueSerialPort(QStringList& ports, QString const& rawPort)
{
    QString const port = normalizedSerialPortName(rawPort);
    if (!port.isEmpty() && !ports.contains(port, Qt::CaseInsensitive)) {
        ports << port;
    }
}

int serialPortNumber(QString const& port)
{
    static QRegularExpression const rx(QStringLiteral(R"(^COM(\d+)$)"),
                                       QRegularExpression::CaseInsensitiveOption);
    auto const match = rx.match(port.trimmed());
    return match.hasMatch() ? match.captured(1).toInt() : -1;
}

void sortSerialPorts(QStringList& ports)
{
    std::sort(ports.begin(), ports.end(), [](QString const& a, QString const& b) {
        int const an = serialPortNumber(a);
        int const bn = serialPortNumber(b);
        if (an >= 0 && bn >= 0) {
            return an < bn;
        }
        if (an >= 0 || bn >= 0) {
            return an >= 0;
        }
        return QString::localeAwareCompare(a, b) < 0;
    });
}

QStringList enumerateSerialPorts(QString const& savedSerialPort, QString const& savedPttPort)
{
    QStringList ports;
    for (QSerialPortInfo const& info : QSerialPortInfo::availablePorts()) {
        appendUniqueSerialPort(ports, info.portName());
        appendUniqueSerialPort(ports, info.systemLocation());
    }

#if defined(Q_OS_WIN)
    // Qt occasionally misses virtual/driver-created ports on Windows. The
    // canonical Windows COM mapping is also exposed in this registry key.
    QSettings serialMap(QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DEVICEMAP\\SERIALCOMM"),
                        QSettings::NativeFormat);
    for (QString const& key : serialMap.allKeys()) {
        appendUniqueSerialPort(ports, serialMap.value(key).toString());
    }
#endif

    appendUniqueSerialPort(ports, savedSerialPort);
    appendUniqueSerialPort(ports, savedPttPort);
    sortSerialPorts(ports);
    return ports;
}
}

// ── PIMPL privato ──────────────────────────────────────────────────────────
// Il transceiver gira su xcvThread; usiamo raw pointer + deleteLater
// per evitare che unique_ptr venga distrutto dal thread sbagliato.
struct DecodiumTransceiverManagerPrivate
{
    TransceiverFactory  factory;
    Transceiver*        transceiver {nullptr};   // owned via deleteLater
    QThread*            xcvThread   {nullptr};   // owned via parent (DecodiumTransceiverManager)
    std::atomic<unsigned> seqNum    {0};
    Transceiver::TransceiverState desired;
};

// ── Costruttore / distruttore ──────────────────────────────────────────────
DecodiumTransceiverManager::DecodiumTransceiverManager(QObject* parent)
    : QObject(parent)
    , d(std::make_unique<DecodiumTransceiverManagerPrivate>())
{
    qRegisterMetaType<QVector<short>>("QVector<short>");
    loadSettings();
}

DecodiumTransceiverManager::~DecodiumTransceiverManager()
{
    auto* xcv = d->transceiver;
    auto* thread = d->xcvThread;
    d->transceiver = nullptr;
    d->xcvThread = nullptr;

    // Chiudi il rig prima di distruggere il QThread per evitare il fatal
    // "QThread: Destroyed while thread is still running".
    if (xcv) {
        if (!thread || !thread->isRunning() || xcv->thread() == QThread::currentThread()) {
            xcv->stop();
        } else {
            QMetaObject::invokeMethod(xcv, [xcv]() {
                xcv->stop();
            }, Qt::BlockingQueuedConnection);
        }
    }

    if (thread && thread->isRunning()) {
        thread->quit();
        if (!thread->wait(4000)) {
            thread->terminate();
            thread->wait(1000);
        }
    }

    if (thread) {
        delete thread;
    }
}

bool DecodiumTransceiverManager::pttSharesCatPort() const
{
    return pttPortSharesCatPort(m_serialPort, m_pttPort);
}

bool DecodiumTransceiverManager::forceDtrAvailable() const
{
    return d3ForceDtrAvailable(m_portType, m_pttMethod, m_serialPort, m_pttPort);
}

bool DecodiumTransceiverManager::forceRtsAvailable() const
{
    return d3ForceRtsAvailable(m_portType, m_handshake, m_pttMethod, m_serialPort, m_pttPort);
}

void DecodiumTransceiverManager::reconnectRigForParameterChange(const QString& reason)
{
    if (!m_connected || !d->transceiver) {
        return;
    }

    emit statusUpdate(reason + QStringLiteral(": riconnessione CAT per applicare il PTT"));
    disconnectRig();
    connectRig();
}

void DecodiumTransceiverManager::enforceForceLineAvailability()
{
    if (!forceDtrAvailable() && m_forceDtr) {
        m_forceDtr = false;
        emit forceDtrChanged();
    }
    if ((!forceDtrAvailable() || !m_forceDtr) && m_dtrHigh) {
        m_dtrHigh = false;
        emit dtrHighChanged();
    }
    if (!forceRtsAvailable() && m_forceRts) {
        m_forceRts = false;
        emit forceRtsChanged();
    }
    if ((!forceRtsAvailable() || !m_forceRts) && m_rtsHigh) {
        m_rtsHigh = false;
        emit rtsHighChanged();
    }
}

void DecodiumTransceiverManager::setSerialPort(const QString& v)
{
    if (m_serialPort != v) {
        m_serialPort = v;
        emit serialPortChanged();
    }
    enforceForceLineAvailability();
}

void DecodiumTransceiverManager::setHandshake(const QString& v)
{
    if (m_handshake != v) {
        m_handshake = v;
        emit handshakeChanged();
    }
    enforceForceLineAvailability();
}

void DecodiumTransceiverManager::setForceDtr(bool v)
{
    bool const effective = forceDtrAvailable() ? v : false;
    if (m_forceDtr != effective) {
        m_forceDtr = effective;
        emit forceDtrChanged();
    }
    if (!m_forceDtr && m_dtrHigh) {
        m_dtrHigh = false;
        emit dtrHighChanged();
    }
}

void DecodiumTransceiverManager::setDtrHigh(bool v)
{
    bool const effective = (forceDtrAvailable() && m_forceDtr) ? v : false;
    if (m_dtrHigh != effective) {
        m_dtrHigh = effective;
        emit dtrHighChanged();
    }
}

void DecodiumTransceiverManager::setForceRts(bool v)
{
    bool const effective = forceRtsAvailable() ? v : false;
    if (m_forceRts != effective) {
        m_forceRts = effective;
        emit forceRtsChanged();
    }
    if (!m_forceRts && m_rtsHigh) {
        m_rtsHigh = false;
        emit rtsHighChanged();
    }
}

void DecodiumTransceiverManager::setRtsHigh(bool v)
{
    bool const effective = (forceRtsAvailable() && m_forceRts) ? v : false;
    if (m_rtsHigh != effective) {
        m_rtsHigh = effective;
        emit rtsHighChanged();
    }
}

void DecodiumTransceiverManager::setPttMethod(const QString& v)
{
    QString const normalized = v.trimmed().toUpper();
    QString const effective =
        0 == m_portType.compare(QStringLiteral("tci"), Qt::CaseInsensitive)
            ? QStringLiteral("CAT")
            : (normalized.isEmpty() ? QStringLiteral("CAT") : normalized);
    if (m_pttMethod != effective) {
        m_pttMethod = effective;
        emit pttMethodChanged();
        reconnectRigForParameterChange(QStringLiteral("PTT Method"));
    }
    enforceForceLineAvailability();
}

void DecodiumTransceiverManager::setPttPort(const QString& v)
{
    QString const effective = v.trimmed().isEmpty() ? QStringLiteral("CAT") : v.trimmed();
    if (m_pttPort != effective) {
        m_pttPort = effective;
        emit pttPortChanged();
        reconnectRigForParameterChange(QStringLiteral("PTT Port"));
    }
    enforceForceLineAvailability();
}

void DecodiumTransceiverManager::setCivAddress(int v)
{
    int const effective = qBound(0, v, 0xff);
    if (m_civAddress == effective) {
        return;
    }

    m_civAddress = effective;
    emit civAddressChanged();
    if (m_connected && d->transceiver
        && 0 == m_portType.compare(QStringLiteral("serial"), Qt::CaseInsensitive)) {
        QString const civText = QString::number(m_civAddress, 16).rightJustified(2, QLatin1Char('0')).toUpper();
        emit statusUpdate(QStringLiteral("CI-V Address: riconnessione CAT per applicare 0x%1").arg(civText));
        disconnectRig();
        connectRig();
    }
}

void DecodiumTransceiverManager::setTciAudioEnabled(bool v)
{
    if (m_tciAudioEnabled == v) {
        return;
    }

    bool const reconnect = m_connected && d->transceiver
        && 0 == m_portType.compare(QStringLiteral("tci"), Qt::CaseInsensitive);
    m_tciAudioEnabled = v;
    emit tciAudioEnabledChanged();

    if (reconnect) {
        emit statusUpdate(QStringLiteral("Audio TCI: riconnessione CAT per applicare %1")
                              .arg(v ? QStringLiteral("ON") : QStringLiteral("OFF")));
        disconnectRig();
        connectRig();
    }
}

// ── rigList ────────────────────────────────────────────────────────────────
void DecodiumTransceiverManager::setSplitMode(const QString& v)
{
    QString normalized = v.trimmed().toLower();
    if (normalized != QStringLiteral("rig") && normalized != QStringLiteral("emulate")) {
        normalized = QStringLiteral("none");
    }
    if (m_splitMode == normalized) {
        return;
    }

    bool const reconnect = m_connected && d->transceiver;
    m_splitMode = normalized;
    emit splitModeChanged();

    if (reconnect) {
        emit statusUpdate(QStringLiteral("Split: riconnessione CAT per applicare %1")
                              .arg(normalized == QStringLiteral("emulate")
                                       ? QStringLiteral("Fake It")
                                       : normalized == QStringLiteral("rig")
                                           ? QStringLiteral("Rig")
                                           : QStringLiteral("None")));
        disconnectRig();
        connectRig();
    }
}

QStringList DecodiumTransceiverManager::rigList() const
{
    QStringList list;
    for (auto it = d->factory.supported_transceivers().cbegin();
         it != d->factory.supported_transceivers().cend(); ++it)
        list << it.key();
    list.sort(Qt::CaseInsensitive);
    return list;
}

// ── setRigName: aggiorna portType dalle Capabilities ──────────────────────
void DecodiumTransceiverManager::setRigName(const QString& v)
{
    if (m_rigName == v) return;
    m_rigName = v;
    emit rigNameChanged();

    using PT = TransceiverFactory::Capabilities::PortType;
    switch (d->factory.CAT_port_type(v)) {
    case PT::serial:  m_portType = "serial";  break;
    case PT::network: m_portType = "network"; break;
    case PT::usb:     m_portType = "usb";     break;
    case PT::tci:     m_portType = "tci";     break;
    default:          m_portType = "none";    break;
    }
    emit portTypeChanged();
    if (0 == m_portType.compare(QStringLiteral("network"), Qt::CaseInsensitive)) {
        QString const endpoint = defaultNetworkEndpointForRig(v);
        if (!endpoint.isEmpty() && isLegacyNetworkEndpoint(m_networkPort)) {
            m_networkPort = endpoint;
            emit networkPortChanged();
        }
    }
    int const civAddress = defaultCivAddressForRig(v, d->factory);
    if (m_civAddress != civAddress) {
        m_civAddress = civAddress;
        emit civAddressChanged();
    }
    if (0 == m_portType.compare(QStringLiteral("tci"), Qt::CaseInsensitive)
        && m_pttMethod != QStringLiteral("CAT")) {
        m_pttMethod = QStringLiteral("CAT");
        emit pttMethodChanged();
    }
    enforceForceLineAvailability();
}

// ── Helpers enum ──────────────────────────────────────────────────────────
static TransceiverFactory::PTTMethod parsePtt(const QString& s)
{
    if (s == "CAT") return TransceiverFactory::PTT_method_CAT;
    if (s == "DTR") return TransceiverFactory::PTT_method_DTR;
    if (s == "RTS") return TransceiverFactory::PTT_method_RTS;
    return TransceiverFactory::PTT_method_VOX;
}

static QString pttMethodName(TransceiverFactory::PTTMethod method)
{
    switch (method) {
    case TransceiverFactory::PTT_method_CAT: return QStringLiteral("CAT");
    case TransceiverFactory::PTT_method_DTR: return QStringLiteral("DTR");
    case TransceiverFactory::PTT_method_RTS: return QStringLiteral("RTS");
    case TransceiverFactory::PTT_method_VOX:
    default: return QStringLiteral("VOX");
    }
}

static QString resolvedPttPort(const DecodiumTransceiverManager* m)
{
    QString const method = m->pttMethod().trimmed().toUpper();
    if (method != QStringLiteral("DTR") && method != QStringLiteral("RTS")) {
        return QStringLiteral("None");
    }

    QString const raw = m->pttPort().trimmed();
    if (raw.isEmpty() || 0 == raw.compare(QStringLiteral("CAT"), Qt::CaseInsensitive)) {
        if (0 == m->portType().compare(QStringLiteral("serial"), Qt::CaseInsensitive)) {
            return normalizeDevicePath(m->serialPort());
        }
        return QStringLiteral("None");
    }

    return normalizeDevicePath(raw);
}

static TransceiverFactory::SplitMode parseSplit(const QString& s)
{
    if (s == "rig")     return TransceiverFactory::split_mode_rig;
    if (s == "fake" || s == "fake it") return TransceiverFactory::split_mode_emulate;
    if (s == "emulate") return TransceiverFactory::split_mode_emulate;
    return TransceiverFactory::split_mode_none;
}

static QString splitModeName(TransceiverFactory::SplitMode mode)
{
    switch (mode) {
    case TransceiverFactory::split_mode_rig:
        return QStringLiteral("rig");
    case TransceiverFactory::split_mode_emulate:
        return QStringLiteral("emulate");
    case TransceiverFactory::split_mode_none:
    default:
        return QStringLiteral("none");
    }
}

static QString splitModeNameFromLegacyValue(const QVariant& value, const QString& fallback)
{
    if (!value.isValid()) {
        return fallback;
    }

    QString const text = value.toString().trimmed().toLower();
    if (text == QStringLiteral("rig") || text == QStringLiteral("split_mode_rig")) {
        return QStringLiteral("rig");
    }
    if (text == QStringLiteral("emulate")
        || text == QStringLiteral("fake")
        || text == QStringLiteral("fake it")
        || text == QStringLiteral("split_mode_emulate")) {
        return QStringLiteral("emulate");
    }
    if (text == QStringLiteral("none") || text == QStringLiteral("split_mode_none")) {
        return QStringLiteral("none");
    }

    bool ok = false;
    int const raw = value.toInt(&ok);
    if (ok) {
        return splitModeName(static_cast<TransceiverFactory::SplitMode>(raw));
    }
    return fallback;
}

static TransceiverFactory::DataBits parseData(const QString& s)
{
    return s == "7" ? TransceiverFactory::seven_data_bits : TransceiverFactory::eight_data_bits;
}

static TransceiverFactory::StopBits parseStop(const QString& s)
{
    return s == "2" ? TransceiverFactory::two_stop_bits : TransceiverFactory::one_stop_bit;
}

static TransceiverFactory::TXAudioSource configuredTxAudioSource()
{
    QSettings s(QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    QVariant const raw = s.value(QStringLiteral("TXAudioSource"), 0);
    QString const text = raw.toString().trimmed();
    if (text.compare(QStringLiteral("Front/Mic"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("front"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("mic"), Qt::CaseInsensitive) == 0) {
        return TransceiverFactory::TX_audio_source_front;
    }
    if (text.compare(QStringLiteral("Rear/Data"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("rear"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("data"), Qt::CaseInsensitive) == 0) {
        return TransceiverFactory::TX_audio_source_rear;
    }

    bool ok = false;
    int const index = raw.toInt(&ok);
    if (ok && index == 1) {
        return TransceiverFactory::TX_audio_source_front;
    }
    return TransceiverFactory::TX_audio_source_rear;
}

static bool configuredPwrAndSwrEnabled()
{
    QSettings settings(QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
    return settings.value(QStringLiteral("PWRandSWR"), false).toBool()
        || settings.value(QStringLiteral("CheckSWR"), false).toBool();
}

static TransceiverFactory::Handshake parseHandshake(const QString& s)
{
    QString const value = s.trimmed().toLower();
    if (value == "xonxoff")  return TransceiverFactory::handshake_XonXoff;
    if (value == "hardware") return TransceiverFactory::handshake_hardware;
    return TransceiverFactory::handshake_none;
}

static QString handshakeName(TransceiverFactory::Handshake handshake)
{
    switch (handshake) {
    case TransceiverFactory::handshake_XonXoff:  return QStringLiteral("xonxoff");
    case TransceiverFactory::handshake_hardware: return QStringLiteral("hardware");
    default:                                     return QStringLiteral("none");
    }
}

static QString modeStr(Transceiver::MODE m)
{
    switch (m) {
    case Transceiver::CW:     return "CW";
    case Transceiver::CW_R:   return "CW-R";
    case Transceiver::USB:    return "USB";
    case Transceiver::LSB:    return "LSB";
    case Transceiver::FSK:    return "FSK";
    case Transceiver::FSK_R:  return "FSK-R";
    case Transceiver::DIG_U:  return "DATA-U";
    case Transceiver::DIG_L:  return "DATA-L";
    case Transceiver::AM:     return "AM";
    case Transceiver::FM:     return "FM";
    case Transceiver::DIG_FM: return "DIG-FM";
    default:                  return "UNK";
    }
}

static Transceiver::MODE parseMode(const QString& s)
{
    QString const mode = s.trimmed().toUpper();
    if (mode == "CW")     return Transceiver::CW;
    if (mode == "CW-R")   return Transceiver::CW_R;
    if (mode == "USB")    return Transceiver::USB;
    if (mode == "LSB")    return Transceiver::LSB;
    if (mode == "FSK")    return Transceiver::FSK;
    if (mode == "FSK-R")  return Transceiver::FSK_R;
    if (mode == "DATA-U" || mode == "DIGU") return Transceiver::DIG_U;
    if (mode == "DATA-L" || mode == "DIGL") return Transceiver::DIG_L;
    if (mode == "AM")     return Transceiver::AM;
    if (mode == "FM")     return Transceiver::FM;
    if (mode == "DIG-FM") return Transceiver::DIG_FM;
    if (mode == "FT8" || mode == "FT4" || mode == "FT2" || mode == "Q65"
        || mode == "MSK144" || mode == "JT65" || mode == "JT9" || mode == "JT4"
        || mode == "FST4" || mode == "FST4W" || mode == "WSPR"
        || mode.startsWith("FST4-") || mode.startsWith("FST4W-"))
        return Transceiver::DIG_U;
    return Transceiver::UNK;
}

// ── buildParams ───────────────────────────────────────────────────────────
static TransceiverFactory::ParameterPack buildParams(const DecodiumTransceiverManager* m)
{
    TransceiverFactory::ParameterPack p;
    bool const canForceDtr = d3ForceDtrAvailable(m->portType(), m->pttMethod(), m->serialPort(), m->pttPort());
    bool const canForceRts = d3ForceRtsAvailable(m->portType(), m->handshake(), m->pttMethod(), m->serialPort(), m->pttPort());
    bool const serialCat = 0 == m->portType().compare(QStringLiteral("serial"), Qt::CaseInsensitive);
    TransceiverFactory::PTTMethod const pttType =
        0 == m->portType().compare(QStringLiteral("tci"), Qt::CaseInsensitive)
            ? TransceiverFactory::PTT_method_CAT
            : parsePtt(m->pttMethod());
#if defined(Q_OS_LINUX)
    // Linux tty drivers may assert DTR/RTS when Hamlib opens the CAT port.
    // Keep unused control lines inactive so opening CAT cannot key PTT on
    // radios/interfaces that map those lines to transmit.
    bool const autoDtrLow = serialCat
        && canForceDtr
        && !m->forceDtr()
        && !(pttType == TransceiverFactory::PTT_method_DTR
             && pttPortSharesCatPort(m->serialPort(), m->pttPort()));
    bool const autoRtsLow = serialCat
        && canForceRts
        && !m->forceRts()
        && !(pttType == TransceiverFactory::PTT_method_RTS
             && pttPortSharesCatPort(m->serialPort(), m->pttPort()));
#else
    bool const autoDtrLow = false;
    bool const autoRtsLow = false;
#endif
    bool const pwrAndSwrEnabled = configuredPwrAndSwrEnabled();
    int const requestedPollInterval = qBound(1, m->pollInterval(), 99);
    int const serialMinimumPollInterval = pwrAndSwrEnabled ? 1 : 2;
    int const basePollInterval = qMax(serialCat ? serialMinimumPollInterval : 1,
                                      requestedPollInterval);
    p.rig_name      = m->rigName();
    p.serial_port   = normalizeDevicePath(m->serialPort());
    p.network_port  = m->networkPort();
    p.usb_port      = normalizeDevicePath(m->serialPort());
    p.tci_port      = m->tciPort();
    p.baud          = m->baudRate();
    p.data_bits     = parseData(m->dataBits());
    p.stop_bits     = parseStop(m->stopBits());
    TransceiverFactory::Handshake const requestedHandshake = parseHandshake(m->handshake());
    p.handshake     = requestedHandshake;
    p.force_dtr     = canForceDtr && (m->forceDtr() || autoDtrLow);
    p.dtr_high      = canForceDtr && m->forceDtr() && m->dtrHigh();
    p.force_rts     = canForceRts && (m->forceRts() || autoRtsLow);
    p.rts_high      = canForceRts && m->forceRts() && m->rtsHigh();
#if defined(Q_OS_LINUX)
    bool const linuxRtsLowGuard = serialCat
        && p.force_rts
        && !p.rts_high
        && requestedHandshake == TransceiverFactory::handshake_hardware;
    if (linuxRtsLowGuard) {
        p.handshake = TransceiverFactory::handshake_none;
    }
#endif
    p.ptt_type      = pttType;
    p.audio_source  = configuredTxAudioSource();
    p.split_mode    = parseSplit(m->splitMode());
    // "CAT" in PTT Port means "use the same serial port as CAT", as in WSJT-X.
    p.ptt_port      = resolvedPttPort(m);
    p.civ_address   = m->civAddress();
    p.poll_interval = basePollInterval;
    if (pwrAndSwrEnabled)
        p.poll_interval |= do__pwr;
    if (m->tciAudioEnabled()
        && 0 == m->portType().compare(QStringLiteral("tci"), Qt::CaseInsensitive)) {
        p.poll_interval |= tci__audio;
    }
    qDebug().noquote()
        << "[CATDBG] Transceiver params"
        << "rig=" << p.rig_name
        << "portType=" << m->portType()
        << "serial=" << p.serial_port
        << "network=" << p.network_port
        << "tci=" << p.tci_port
        << "baud=" << p.baud
        << "handshake=" << handshakeName(p.handshake)
        << "handshakeRequested=" << m->handshake()
        << "ptt=" << pttMethodName(p.ptt_type)
        << "pttPort=" << p.ptt_port
        << "forceDtr=" << p.force_dtr
        << "dtrHigh=" << p.dtr_high
        << "forceRts=" << p.force_rts
        << "rtsHigh=" << p.rts_high
#if defined(Q_OS_LINUX)
        << "linuxAutoDtrLow=" << autoDtrLow
        << "linuxAutoRtsLow=" << autoRtsLow
        << "linuxRtsLowGuard=" << linuxRtsLowGuard
#endif
        << "split=" << splitModeName(p.split_mode)
        << "poll=" << (p.poll_interval & 0xffff);
    return p;
}

// ── connectRig ────────────────────────────────────────────────────────────
void DecodiumTransceiverManager::connectRig()
{
    if (d->transceiver) {
        if (m_connecting && !m_connected) {
            emit statusUpdate(QStringLiteral("Connessione a %1 gia' in corso...").arg(m_rigName));
            return;
        }
        disconnectRig();
        // Piccola attesa per assicurarsi che il vecchio thread sia fermato
        if (d->xcvThread && d->xcvThread->isRunning())
            d->xcvThread->wait(2000);
    }

    // Valida che il rig sia effettivamente nel registry (evita rig_init(0) → crash Hamlib 4.7)
    if (!d->factory.supported_transceivers().contains(m_rigName)) {
        emit errorOccurred("Transceiver non trovato nel registry: \"" + m_rigName
                           + "\". Seleziona un rig dalla lista.");
        return;
    }

    emit statusUpdate("Connessione a " + m_rigName + "…");

    // Thread senza parent: sarà gestito via QThread::finished + deleteLater
    auto* thread = new QThread();
    d->xcvThread = thread;

    TransceiverFactory::ParameterPack params;
    Transceiver* xcv = nullptr;
    try {
        params = buildParams(this);
        qInfo().noquote()
            << "[CATDBG] Connect attempt"
            << "rig=" << params.rig_name
            << "portType=" << m_portType
            << "serial=" << params.serial_port
            << "network=" << params.network_port
            << "tci=" << params.tci_port
            << "baud=" << params.baud
            << "handshake=" << handshakeName(params.handshake)
            << "ptt=" << pttMethodName(params.ptt_type)
            << "pttPort=" << params.ptt_port
            << "split=" << splitModeName(params.split_mode)
            << "poll=" << (params.poll_interval & 0xffff);
        auto uptr = d->factory.create(params, thread);
        xcv = uptr.release();          // trasferisce ownership al thread (via deleteLater)
    } catch (std::exception const& e) {
        qWarning().noquote()
            << "[CATDBG] Connect create failed"
            << "rig=" << m_rigName
            << "portType=" << m_portType
            << "serial=" << m_serialPort
            << "network=" << m_networkPort
            << "tci=" << m_tciPort
            << "reason=" << e.what();
        emit errorOccurred(QString("Errore creazione transceiver: %1").arg(e.what()));
        delete thread;
        d->xcvThread = nullptr;
        return;
    } catch (...) {
        qWarning().noquote()
            << "[CATDBG] Connect create failed"
            << "rig=" << m_rigName
            << "portType=" << m_portType
            << "serial=" << m_serialPort
            << "network=" << m_networkPort
            << "tci=" << m_tciPort
            << "reason=unknown";
        emit errorOccurred("Errore sconosciuto durante la creazione del transceiver");
        delete thread;
        d->xcvThread = nullptr;
        return;
    }

    d->transceiver = xcv;
    d->desired = Transceiver::TransceiverState {};
    d->desired.online(true);
    m_connecting = true;

    // Il transceiver è "ripe for destruction" quando emette finished().
    connect(xcv, &Transceiver::finished,
            xcv, &QObject::deleteLater,
            Qt::QueuedConnection);

    // Quando il transceiver ha terminato il suo shutdown, fai terminare
    // anche il relativo event loop; il QThread verrà poi ripulito solo
    // dopo il vero QThread::finished.
    connect(xcv, &Transceiver::finished,
            thread, &QThread::quit,
            Qt::QueuedConnection);

    // Cleanup automatico: il QThread viene eliminato solo quando è davvero
    // terminato, evitando di distruggerlo mentre il worker è ancora attivo.
    connect(thread, &QThread::finished, xcv, &QObject::deleteLater);
    connect(thread, &QThread::finished, this,
            [this, thread, xcv]() {
                if (d->xcvThread == thread)
                    d->xcvThread = nullptr;
                if (d->transceiver == xcv)
                    d->transceiver = nullptr;
                d->desired = Transceiver::TransceiverState {};
                m_connecting = false;
                thread->deleteLater();
                if (m_connected) {
                    m_connected = false;
                    emit connectedChanged();
                }
                updateTelemetry(0.0, 0.0);
            },
            Qt::QueuedConnection);

    // Aggiornamenti di stato dal rig → main thread
    connect(xcv, &Transceiver::update,
            this,
            [this](Transceiver::TransceiverState const& state, unsigned /*seq*/) {
                auto const requestedAudio = d->desired.audio();
                auto const requestedPeriod = d->desired.period();
                auto const requestedBlocksize = d->desired.blocksize();
                auto const requestedTxAudio = d->desired.tx_audio();
                auto const requestedTune = d->desired.tune();
                auto const requestedQuick = d->desired.quick();
                auto const requestedJtMode = d->desired.jtmode();
                auto const requestedSymbolsLength = d->desired.symbolslength();
                auto const requestedFramesPerSymbol = d->desired.framespersymbol();
                auto const requestedTrFrequency = d->desired.trfrequency();
                auto const requestedToneSpacing = d->desired.tonespacing();
                auto const requestedSynchronize = d->desired.synchronize();
                auto const requestedFastMode = d->desired.fastmode();
                auto const requestedDbSnr = d->desired.dbsnr();
                auto const requestedTrPeriod = d->desired.trperiod();
                d->desired = state;
                d->desired.audio(requestedAudio);
                d->desired.period(requestedPeriod);
                d->desired.blocksize(requestedBlocksize);
                d->desired.tx_audio(requestedTxAudio);
                d->desired.tune(requestedTune);
                d->desired.quick(requestedQuick);
                d->desired.jtmode(requestedJtMode);
                d->desired.symbolslength(requestedSymbolsLength);
                d->desired.framespersymbol(requestedFramesPerSymbol);
                d->desired.trfrequency(requestedTrFrequency);
                d->desired.tonespacing(requestedToneSpacing);
                d->desired.synchronize(requestedSynchronize);
                d->desired.fastmode(requestedFastMode);
                d->desired.dbsnr(requestedDbSnr);
                d->desired.trperiod(requestedTrPeriod);
                double  freq = static_cast<double>(state.frequency());
                double  txf  = static_cast<double>(state.tx_frequency());
                QString mode = modeStr(state.mode());
                bool    ptt  = state.ptt();
                bool    spl  = state.split();
                bool    onl  = state.online();

                if (m_connected != onl) {
                    m_connecting = false;
                    m_connected = onl;
                    emit connectedChanged();
                    emit statusUpdate(onl ? ("Connesso: " + m_rigName) : "Disconnesso");
                    if (onl) {
                        m_transientCatRetryCount = 0;
                        m_transientCatReconnectPending = false;
                    }
                    if (onl && m_audioAutoStart)
                        emit audioAutoStartChanged();
                }
                if (freq > 0 && m_frequency != freq) { m_frequency = freq; emit frequencyChanged(); }
                if (txf  > 0 && m_txFrequency != txf) { m_txFrequency = txf; emit txFrequencyChanged(); }
                if (m_mode    != mode) { m_mode    = mode; emit modeChanged(); }
                if (m_pttActive != ptt) { m_pttActive = ptt; emit pttActiveChanged(); }
                if (m_split   != spl)  { m_split   = spl;  emit splitChanged(); }
                updateTelemetry(static_cast<double>(state.power()) / 1000.0,
                                static_cast<double>(state.swr()) / 100.0);
            },
            Qt::QueuedConnection);

    // Errori dal rig
    QString const attemptRig = params.rig_name;
    QString const attemptPortType = m_portType;
    QString const attemptSerial = params.serial_port;
    QString const attemptNetwork = params.network_port;
    QString const attemptTci = params.tci_port;
    int const attemptBaud = params.baud;
    QString const attemptHandshake = handshakeName(params.handshake);
    QString const attemptPtt = pttMethodName(params.ptt_type);
    QString const attemptPttPort = params.ptt_port;
    connect(xcv, &Transceiver::failure,
            this,
            [this, attemptRig, attemptPortType, attemptSerial, attemptNetwork, attemptTci,
             attemptBaud, attemptHandshake, attemptPtt, attemptPttPort](QString const& reason) {
                bool const wasConnected = m_connected;
                bool const startupAttempt = m_connecting && !wasConnected;
                m_connecting = false;
                d->desired.online(false);
                bool const recovering = m_transientCatRetryCount > 0;
                QString const shownReason = sanitizeHamlibFailure(reason);
                qWarning().noquote()
                    << (startupAttempt ? "[CATDBG] Connect failed" : "[CATDBG] CAT failure")
                    << "rig=" << attemptRig
                    << "portType=" << attemptPortType
                    << "serial=" << attemptSerial
                    << "network=" << attemptNetwork
                    << "tci=" << attemptTci
                    << "baud=" << attemptBaud
                    << "handshake=" << attemptHandshake
                    << "ptt=" << attemptPtt
                    << "pttPort=" << attemptPttPort
                    << "raw=" << reason
                    << "shown=" << shownReason;
                if ((wasConnected || recovering) && isTransientCatIoFailure(reason)) {
                    scheduleTransientReconnect(reason);
                } else {
                    emit errorOccurred("CAT failure: " + shownReason);
                }
                if (m_connected) { m_connected = false; emit connectedChanged(); }
                updateTelemetry(0.0, 0.0);
            },
            Qt::QueuedConnection);
    connect(xcv, &Transceiver::tciPcmSamplesReady,
            this, &DecodiumTransceiverManager::tciPcmSamplesReady,
            Qt::QueuedConnection);
    connect(xcv, &Transceiver::tci_mod_active,
            this, &DecodiumTransceiverManager::tciModActiveChanged,
            Qt::QueuedConnection);

    thread->start();

    // Avvia il transceiver sul suo thread tramite lambda (no Q_ARG / metatype)
    unsigned seq = ++(d->seqNum);
    QMetaObject::invokeMethod(xcv, [xcv, seq]() {
        xcv->start(seq);
    }, Qt::QueuedConnection);
}

// ── disconnectRig ─────────────────────────────────────────────────────────
void DecodiumTransceiverManager::disconnectRig()
{
    m_transientCatReconnectPending = false;
    m_connecting = false;
    if (!d->transceiver) return;

    auto* xcv    = d->transceiver;
    auto* thread = d->xcvThread;

    // Invalida subito i puntatori nel PIMPL per evitare double-call
    d->transceiver = nullptr;
    d->xcvThread   = nullptr;
    d->desired = Transceiver::TransceiverState {};

    // Richiedi stop in modo sincrono sul thread del rig così il cleanup
    // (PTT off, split reset, rig_close) avviene prima del quit del thread.
    if (!thread || !thread->isRunning() || xcv->thread() == QThread::currentThread()) {
        xcv->stop();
    } else {
        QMetaObject::invokeMethod(xcv, [xcv]() {
            xcv->stop();
        }, Qt::BlockingQueuedConnection);
    }

    if (thread && thread->isRunning()) {
        thread->quit();
        if (!thread->wait(4000)) {
            thread->terminate();
            thread->wait(1000);
        }
        thread->deleteLater();
    }

    if (m_connected) {
        m_connected = false;
        emit connectedChanged();
    }
    updateTelemetry(0.0, 0.0);
    emit statusUpdate("Disconnesso dal transceiver");
}

void DecodiumTransceiverManager::scheduleTransientReconnect(const QString& reason)
{
    if (m_transientCatReconnectPending) {
        return;
    }

    static constexpr int maxRetries = 5;
    if (m_transientCatRetryCount >= maxRetries) {
        m_transientCatRetryCount = 0;
        emit errorOccurred(QStringLiteral("CAT failure: ") + sanitizeHamlibFailure(reason));
        return;
    }

    ++m_transientCatRetryCount;
    m_transientCatReconnectPending = true;
    int const delayMs = qMin(10000, 1200 * m_transientCatRetryCount);
    emit statusUpdate(tr("CAT interrotto, riconnessione automatica (%1/%2)...")
                          .arg(m_transientCatRetryCount)
                          .arg(maxRetries));

    QTimer::singleShot(delayMs, this, [this]() {
        if (!m_transientCatReconnectPending) {
            return;
        }
        m_transientCatReconnectPending = false;
        disconnectRig();
        connectRig();
    });
}

void DecodiumTransceiverManager::updateTelemetry(double powerWatts, double swr)
{
    if (m_powerWatts != powerWatts) {
        m_powerWatts = powerWatts;
        emit powerWattsChanged();
    }
    if (m_swr != swr) {
        m_swr = swr;
        emit swrChanged();
    }
}

// ── sendState: invia TransceiverState al rig sul suo thread ───────────────
// Chiamato solo dal main thread → nessuna race condition su desired
static void sendState(DecodiumTransceiverManagerPrivate* d)
{
    if (!d->transceiver) return;
    auto* xcv  = d->transceiver;
    auto  st   = d->desired;                    // copia locale (valore)
    unsigned seq = ++(d->seqNum);
    QMetaObject::invokeMethod(xcv, [xcv, st, seq]() {
        xcv->set(st, seq);
    }, Qt::QueuedConnection);
}

static void sendStateSync(DecodiumTransceiverManagerPrivate* d)
{
    if (!d->transceiver) return;
    auto* xcv = d->transceiver;
    auto  st  = d->desired;
    unsigned seq = ++(d->seqNum);

    if (xcv->thread() == QThread::currentThread()) {
        xcv->set(st, seq);
        return;
    }

    QMetaObject::invokeMethod(xcv, [xcv, st, seq]() {
        xcv->set(st, seq);
    }, Qt::BlockingQueuedConnection);
}

void DecodiumTransceiverManager::setRigFrequency(double hz)
{
    m_frequency = hz;
    emit frequencyChanged();
    d->desired.frequency(static_cast<Transceiver::Frequency>(hz));
    sendState(d.get());
}

void DecodiumTransceiverManager::setRigTxFrequency(double hz)
{
    m_txFrequency = hz;
    emit txFrequencyChanged();
    if (hz > 0.0 && d->desired.frequency() == 0 && m_frequency > 0.0) {
        d->desired.frequency(static_cast<Transceiver::Frequency>(m_frequency));
    }
    if (hz > 0.0 || d->desired.tx_frequency() > 0) {
        qDebug().noquote()
            << "[CATDBG] Hamlib set TX frequency"
            << "splitMode=" << m_splitMode
            << "rxHz=" << QString::number(static_cast<double>(d->desired.frequency()), 'f', 0)
            << "txHz=" << QString::number(hz, 'f', 0);
    }
    d->desired.split(hz > 0.0);
    d->desired.tx_frequency(static_cast<Transceiver::Frequency>(hz));
    sendState(d.get());
}

void DecodiumTransceiverManager::setRigTxFrequencyAndPtt(double hz, bool on)
{
    if (hz > 0.0 && d->desired.frequency() == 0 && m_frequency > 0.0) {
        d->desired.frequency(static_cast<Transceiver::Frequency>(m_frequency));
    }
    if (hz > 0.0) {
        if (!qFuzzyCompare(m_txFrequency + 1.0, hz + 1.0)) {
            m_txFrequency = hz;
            emit txFrequencyChanged();
        }
        d->desired.split(true);
        d->desired.tx_frequency(static_cast<Transceiver::Frequency>(hz));
    } else {
        d->desired.split(false);
        d->desired.tx_frequency(0);
    }
    qDebug().noquote()
        << "[CATDBG] Hamlib set TX frequency + PTT"
        << "splitMode=" << m_splitMode
        << "on=" << on
        << "rxHz=" << QString::number(static_cast<double>(d->desired.frequency()), 'f', 0)
        << "txHz=" << QString::number(hz, 'f', 0);
    d->desired.ptt(on);
    sendStateSync(d.get());
}

void DecodiumTransceiverManager::setRigPtt(bool on)
{
    if (m_pttMethod == QStringLiteral("VOX")) {
        qDebug() << "[CATDBG] VOX PTT request ignored: TX must be triggered by audio only"
                 << "on=" << on;
        if (m_pttActive) {
            m_pttActive = false;
            emit pttActiveChanged();
        }
        return;
    }
    d->desired.ptt(on);
    if (on)
        sendState(d.get());
    else
        sendStateSync(d.get());
}

void DecodiumTransceiverManager::setRigMode(const QString& mode)
{
    d->desired.mode(parseMode(mode));
    sendState(d.get());
}

void DecodiumTransceiverManager::setRigAudio(bool on, double periodSeconds, int blockSize)
{
    d->desired.online(true);
    d->desired.period(qBound(0.1, periodSeconds, 1800.0));
    d->desired.blocksize(qBound(256, blockSize, 48000));
    d->desired.audio(on);
    if (on)
        sendState(d.get());
    else
        sendStateSync(d.get());
}

void DecodiumTransceiverManager::setRigTune(bool on)
{
    d->desired.online(true);
    d->desired.tune(on);
    sendStateSync(d.get());
}

void DecodiumTransceiverManager::startRigTxAudio(const QString& mode, unsigned symbolsLength,
                                                 double framesPerSymbol, double frequency,
                                                 double toneSpacing, bool synchronize,
                                                 bool fastMode, double dbsnr, double trPeriod)
{
    d->desired.online(true);
    d->desired.jtmode(mode);
    d->desired.symbolslength(symbolsLength);
    d->desired.framespersymbol(framesPerSymbol);
    d->desired.trfrequency(frequency);
    d->desired.tonespacing(toneSpacing);
    d->desired.synchronize(synchronize);
    d->desired.fastmode(fastMode);
    d->desired.dbsnr(dbsnr);
    d->desired.trperiod(trPeriod);
    d->desired.tx_audio(true);
    sendStateSync(d.get());
}

void DecodiumTransceiverManager::stopRigTxAudio(bool quick)
{
    d->desired.online(true);
    d->desired.quick(quick);
    d->desired.tx_audio(false);
    sendStateSync(d.get());
}

// ── refreshPorts ──────────────────────────────────────────────────────────
void DecodiumTransceiverManager::refreshPorts()
{
    QElapsedTimer timer;
    timer.start();
    QStringList ports = enumerateSerialPorts(m_serialPort, m_pttPort);
    if (ports != m_portList) {
        m_portList = ports;
        emit portListChanged();
    }
    if (timer.elapsed() > 1000) {
        qWarning("Hamlib serial port enumeration took %lld ms (%lld ports)",
                 timer.elapsed(), static_cast<long long>(m_portList.size()));
    }
}

// ── Persistenza ───────────────────────────────────────────────────────────
void DecodiumTransceiverManager::saveSettings()
{
    const QString serialPort = normalizeDevicePath(m_serialPort);
    const QString pttPort = m_pttPort.isEmpty() ? QStringLiteral("CAT") : normalizeDevicePath(m_pttPort);
    bool const canForceDtr = forceDtrAvailable();
    bool const canForceRts = forceRtsAvailable();

    QSettings s("Decodium", "Decodium3");
    s.beginGroup("Transceiver");
    s.setValue("rigName",      m_rigName);
    s.setValue("serialPort",   serialPort);
    s.setValue("baudRate",     m_baudRate);
    s.setValue("dataBits",     m_dataBits);
    s.setValue("stopBits",     m_stopBits);
    s.setValue("handshake",    m_handshake);
    s.setValue("forceDtr",     canForceDtr && m_forceDtr);
    s.setValue("dtrHigh",      canForceDtr && m_dtrHigh);
    s.setValue("forceRts",     canForceRts && m_forceRts);
    s.setValue("rtsHigh",      canForceRts && m_rtsHigh);
    s.setValue("networkPort",  m_networkPort);
    s.setValue("tciPort",      m_tciPort);
    s.setValue("pttMethod",    m_pttMethod);
    s.setValue("pttPort",      pttPort);
    s.setValue("splitMode",    m_splitMode);
    s.setValue("civAddress",   m_civAddress);
    s.setValue("pollInterval", m_pollInterval);
    s.setValue("catAutoConnect", m_catAutoConnect);
    s.setValue("audioAutoStart", m_audioAutoStart);
    s.setValue("tciAudioEnabled", m_tciAudioEnabled);
    s.endGroup();
}

void DecodiumTransceiverManager::loadSettings()
{
    QSettings s("Decodium", "Decodium3");
    s.beginGroup("Transceiver");
    auto get = [&](const QString& k, const QVariant& def) { return s.value(k, def); };
    m_serialPort   = normalizeDevicePath(get("serialPort",   m_serialPort).toString());
    m_baudRate     = get("baudRate",     m_baudRate).toInt();
    m_dataBits     = get("dataBits",     m_dataBits).toString();
    m_stopBits     = get("stopBits",     m_stopBits).toString();
    m_handshake    = get("handshake",    m_handshake).toString();
    m_forceDtr     = get("forceDtr",     m_forceDtr).toBool();
    m_dtrHigh      = get("dtrHigh",      m_dtrHigh).toBool();
    m_forceRts     = get("forceRts",     m_forceRts).toBool();
    m_rtsHigh      = get("rtsHigh",      m_rtsHigh).toBool();
    m_networkPort  = get("networkPort",  m_networkPort).toString();
    m_tciPort      = get("tciPort",      m_tciPort).toString();
    m_pttMethod    = get("pttMethod",    m_pttMethod).toString().trimmed().toUpper();
    if (m_pttMethod.isEmpty())
        m_pttMethod = QStringLiteral("CAT");
    m_pttPort      = normalizeDevicePath(get("pttPort",      m_pttPort).toString());
    bool const hasModernSplitMode = s.contains(QStringLiteral("splitMode"));
    m_splitMode    = get("splitMode",    m_splitMode).toString();
    int const rawPollInterval = get("pollInterval", m_pollInterval).toInt();
    int const secondsPart = rawPollInterval & 0xffff;
    m_pollInterval = qBound(1, secondsPart > 0 ? secondsPart : rawPollInterval, 99);
    m_catAutoConnect = get("catAutoConnect", m_catAutoConnect).toBool();
    m_audioAutoStart = get("audioAutoStart", m_audioAutoStart).toBool();
    bool const legacyTciAudioFlag = (rawPollInterval & tci__audio) == tci__audio;
    m_tciAudioEnabled = get("tciAudioEnabled", m_tciAudioEnabled || legacyTciAudioFlag).toBool();
    // setRigName DOPO gli altri per aggiornare portType correttamente
    QString rig = get("rigName", m_rigName).toString();
    s.endGroup();
    if (!hasModernSplitMode || m_splitMode.trimmed().isEmpty()) {
        m_splitMode = splitModeNameFromLegacyValue(s.value(QStringLiteral("SplitMode")), m_splitMode);
    }
    m_splitMode = splitModeName(parseSplit(m_splitMode.trimmed().toLower()));
    setRigName(rig);
    enforceForceLineAvailability();
}
