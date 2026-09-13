// DecodiumTransceiverManager.cpp
// Wrapper QML-friendly attorno a TransceiverFactory (Hamlib, OmniRig, HRD, DXLab, TCI).

#include "DecodiumTransceiverManager.h"

#include "DecodiumLogging.hpp"
#include "DecodiumProfileSettings.h"
#include "Transceiver/TransceiverFactory.hpp"
#include "Transceiver/Transceiver.hpp"
#include "Transceiver/TransceiverBase.hpp"

#include <QByteArray>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QThread>
#include <QSerialPortInfo>
#include <QSettings>
#include <QMetaObject>
#include <QMetaType>
#include <QPointer>
#include <QDebug>
#include <QRegularExpression>
#include <QTimer>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <stdexcept>
#include <hamlib/rig.h>

namespace
{
constexpr int kHrdStartupWatchdogMs = 75000;

// Estrae il nome di una porta COM (es. "COM5") dal blob di errore hamlib
// che può contenere path tipo "\\.\COM5" o "/dev/ttyUSB0".
QString extractPortNameFromReason(QString const& reason)
{
    static QRegularExpression const rxWin(QStringLiteral(R"((COM\d+))"),
                                          QRegularExpression::CaseInsensitiveOption);
    auto m = rxWin.match(reason);
    if (m.hasMatch())
        return m.captured(1).toUpper();
    static QRegularExpression const rxUnix(QStringLiteral(R"((/dev/[^\s:;,)]+))"));
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
    QString const lower = reason.toLower();
    bool const hrdProtocolSilent =
        reason.contains(QStringLiteral("protocol silent"), Qt::CaseInsensitive);
    bool const hrdProtocolTimeout =
        (hrdProtocolSilent
         || reason.contains(QStringLiteral("get id"), Qt::CaseInsensitive)
         || reason.contains(QStringLiteral("get context"), Qt::CaseInsensitive))
        && (reason.contains(QStringLiteral("retries exhausted"), Qt::CaseInsensitive)
            || reason.contains(QStringLiteral("ritenta esaurito"), Qt::CaseInsensitive)
            || reason.contains(QStringLiteral("failed to reply"), Qt::CaseInsensitive)
            || reason.contains(QStringLiteral("non risponde"), Qt::CaseInsensitive)
            || reason.contains(QStringLiteral("timed out"), Qt::CaseInsensitive)
            || hrdProtocolSilent);
    if (hrdProtocolTimeout) {
        return QObject::tr(
            "Ham Radio Deluxe accetta la connessione TCP, ma non risponde al protocollo HRD. "
            "Verifica che HRD Rig Control sia avviato, che la radio sia gia' connessa in HRD "
            "e che il server TCP/Remote sia abilitato sulla porta 7809.");
    }

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

    static QStringList const missingPortMarkers = {
        QStringLiteral("does not exist"),
        QStringLiteral("No such file"),
        QStringLiteral("The system cannot find"),
        QStringLiteral("cannot find the file"),
        QStringLiteral("ENODEV"),
        QStringLiteral("ENOENT"),
    };
    for (auto const& marker : missingPortMarkers) {
        if (reason.contains(marker, Qt::CaseInsensitive)) {
            QString port = extractPortNameFromReason(reason);
            if (port.isEmpty()) {
                return QObject::tr("Porta seriale CAT non disponibile. Attendi che Windows enumeri la radio e riprova.");
            }
            return QObject::tr("Porta %1 non disponibile. Attendi che Windows enumeri la radio e riprova.").arg(port);
        }
    }

    bool const hamlibTimeout =
        lower.contains(QStringLiteral("timed out"))
        || lower.contains(QStringLiteral("timeout"))
        || lower.contains(QStringLiteral("returning(-5)"));
    bool const yaesuNewCatTrace =
        lower.contains(QStringLiteral("newcat_"))
        || lower.contains(QStringLiteral("cmd_str = fa"))
        || lower.contains(QStringLiteral("cmd_str=fa"))
        || lower.contains(QStringLiteral("newcat_get_freq"));
    if (hamlibTimeout && yaesuNewCatTrace) {
        return QObject::tr(
            "Hamlib Yaesu/NewCAT: il rig non risponde alla query CAT. "
            "Verifica porta COM, baud rate CAT della radio, handshake RTS/DTR, "
            "CAT TOT e che nessun altro software stia usando la porta.");
    }

    if (hamlibTimeout) {
        return QObject::tr(
            "Comunicazione CAT in timeout. Verifica cavo USB, porta COM, baud rate, "
            "handshake RTS/DTR e che il rig sia acceso e non usato da altri software.");
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
                               "il rig sia acceso. Dettagli tecnici nel diagnostic log.");
        }
    }
    return reason;
}

QString normalizeDevicePath(QString value)
{
    value = value.trimmed();
    if (value.isEmpty())
        return value;
    if (0 == value.compare(QStringLiteral("CAT"), Qt::CaseInsensitive))
        return QStringLiteral("CAT");
    if (0 == value.compare(QStringLiteral("None"), Qt::CaseInsensitive))
        return QStringLiteral("None");

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

bool truthyEnvironmentFlag(char const* name)
{
    QString const value = qEnvironmentVariable(name).trimmed().toLower();
    return value == QStringLiteral("1")
        || value == QStringLiteral("true")
        || value == QStringLiteral("yes")
        || value == QStringLiteral("on");
}

bool catSuppressedByEnvironment()
{
    return truthyEnvironmentFlag("DECODIUM_DISABLE_CAT")
        || truthyEnvironmentFlag("DECODIUM_RX_RECORD_DISABLE_CAT");
}

QString catSuppressionReason()
{
    if (truthyEnvironmentFlag("DECODIUM_RX_RECORD_DISABLE_CAT")) {
        return QObject::tr("CAT disabilitato per test RX/recording: la seriale resta disponibile per JTDX.");
    }
    return QObject::tr("CAT disabilitato da variabile d'ambiente DECODIUM_DISABLE_CAT.");
}

QString comparablePortName(QString value)
{
    value = normalizeDevicePath(value);
    if (value.isEmpty() || value == "CAT" || value == "None")
        return value;
    if (value.startsWith(QStringLiteral("\\\\.\\")))
        value.remove(0, 4);
#if defined(Q_OS_LINUX)
    if (value.startsWith(QLatin1Char('/'))) {
        QFileInfo const info(value);
        QString const canonical = info.canonicalFilePath();
        if (!canonical.isEmpty())
            value = canonical;
    }
#endif
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

bool sameCatFrequency(double lhs, double rhs, double toleranceHz = 1.0)
{
    return std::abs(lhs - rhs) <= toleranceHz;
}

QString defaultNetworkEndpointForRig(QString const& rigName)
{
    if (0 == rigName.compare(QStringLiteral("Ham Radio Deluxe"), Qt::CaseInsensitive))
        return QStringLiteral("127.0.0.1:7809");
    if (0 == rigName.compare(QStringLiteral("DX Lab Suite Commander"), Qt::CaseInsensitive))
        return QStringLiteral("127.0.0.1:52002");
    return QString();
}

bool isHamRadioDeluxeRig(QString const& rigName)
{
    return 0 == rigName.compare(QStringLiteral("Ham Radio Deluxe"), Qt::CaseInsensitive);
}

QString activeCatTransport(QString const& rigName, QString const& portType,
                           QString const& serialPort, QString const& networkPort,
                           QString const& tciPort)
{
    QString const lowerPortType = portType.trimmed().toLower();
    if (isHamRadioDeluxeRig(rigName)
        || 0 == rigName.compare(QStringLiteral("DX Lab Suite Commander"), Qt::CaseInsensitive)) {
        return QStringLiteral("network(%1)").arg(networkPort);
    }
    if (lowerPortType == QStringLiteral("serial")) {
        return QStringLiteral("serial(%1)").arg(serialPort);
    }
    if (lowerPortType == QStringLiteral("usb")) {
        return QStringLiteral("usb(%1)").arg(serialPort);
    }
    if (lowerPortType == QStringLiteral("tci")) {
        return QStringLiteral("tci(%1)").arg(tciPort);
    }
    if (lowerPortType == QStringLiteral("network")) {
        return QStringLiteral("network(%1)").arg(networkPort);
    }
    return lowerPortType.isEmpty() ? QStringLiteral("none") : lowerPortType;
}

double sanitizedCatTxFrequencyHz(double requestedHz, double rxHz,
                                 QString const& splitMode, QString const& context)
{
    if (requestedHz <= 0.0 || rxHz <= 0.0) {
        return requestedHz;
    }

    // In emulate/fake-it split the TX dial should only move by a few kHz.
    // A stale UI value can briefly look like a VHF dial and make the rig
    // jump bands; recover to the expected Decodium offset instead.
    if (std::abs(requestedHz - rxHz) <= 1000000.0) {
        return requestedHz;
    }

    double const fallbackHz = rxHz + 4000.0;
    qWarning().noquote()
        << "[CATDBG] TX frequency sanitized"
        << "context=" << context
        << "splitMode=" << splitMode
        << "rxHz=" << QString::number(rxHz, 'f', 0)
        << "requestedTxHz=" << QString::number(requestedHz, 'f', 0)
        << "fallbackTxHz=" << QString::number(fallbackHz, 'f', 0);
    return fallbackHz;
}

bool parseNetworkPortText(QString const& text, quint16* out = nullptr)
{
    bool ok = false;
    uint const value = text.trimmed().toUInt(&ok);
    if (!ok || value == 0 || value > 65535) {
        return false;
    }
    if (out) {
        *out = static_cast<quint16>(value);
    }
    return true;
}

QString endpointHostPart(QString const& endpoint)
{
    QString const value = endpoint.trimmed();
    int const colon = value.lastIndexOf(QLatin1Char(':'));
    if (colon <= 0) {
        return QString();
    }
    return value.left(colon).trimmed();
}

quint16 endpointPortPart(QString const& endpoint)
{
    QString const value = endpoint.trimmed();
    int const colon = value.lastIndexOf(QLatin1Char(':'));
    if (colon < 0) {
        return 0;
    }
    quint16 port = 0;
    return parseNetworkPortText(value.mid(colon + 1), &port) ? port : 0;
}

QString normalizeNetworkHost(QString host)
{
    host = host.trimmed();
    QString const compact = host;
    if (compact == QStringLiteral("127.0.01")
        || compact == QStringLiteral("127.0.1")
        || compact == QStringLiteral("0.0.0.0")) {
        return QStringLiteral("127.0.0.1");
    }
    static QRegularExpression const embeddedIpv4(
        QStringLiteral("(\\d{1,3}(?:\\.\\d{1,3}){3})"));
    QRegularExpressionMatch const ipv4Match = embeddedIpv4.match(compact);
    if (ipv4Match.hasMatch() && ipv4Match.captured(1) != compact) {
        return normalizeNetworkHost(ipv4Match.captured(1));
    }
    return host;
}

QString normalizeNetworkEndpoint(QString endpoint, QString const& rigName,
                                 bool* changed = nullptr)
{
    QString const original = endpoint.trimmed();
    QString value = original;
    if (changed) {
        *changed = false;
    }

    if (value.startsWith(QStringLiteral("tcp://"), Qt::CaseInsensitive)) {
        value.remove(0, 6);
    }

    QString const defaultEndpoint = defaultNetworkEndpointForRig(rigName);
    QString defaultHost = endpointHostPart(defaultEndpoint);
    quint16 const defaultPort = endpointPortPart(defaultEndpoint);
    auto finish = [&](QString normalized) {
        normalized = normalized.trimmed();
        if (changed) {
            *changed = normalized != original;
        }
        return normalized;
    };
    auto join = [](QString host, quint16 port) {
        host = normalizeNetworkHost(host);
        return host.isEmpty()
            ? QStringLiteral(":%1").arg(port)
            : QStringLiteral("%1:%2").arg(host).arg(port);
    };

    if (value.isEmpty()) {
        return finish(defaultEndpoint);
    }

    // Keep bracketed IPv6 untouched; NetworkServerLookup already understands it.
    if (value.startsWith(QLatin1Char('['))) {
        return finish(value);
    }

    quint16 portOnly = 0;
    if (parseNetworkPortText(value, &portOnly) && !defaultHost.isEmpty()) {
        return finish(join(defaultHost, portOnly));
    }

    QStringList parts = value.split(QLatin1Char(':'), Qt::KeepEmptyParts);
    if (parts.size() == 1 && defaultPort > 0) {
        QString const host = normalizeNetworkHost(value);
        if (!host.isEmpty() && !parseNetworkPortText(host)) {
            return finish(join(host, defaultPort));
        }
    }

    if (parts.size() == 2) {
        QString const host = parts.at(0).trimmed();
        QString const rhs = parts.at(1).trimmed();
        quint16 port = 0;
        if (parseNetworkPortText(rhs, &port)) {
            return finish(join(host, port));
        }
        // Common broken state from pasted/merged fields: "localhost:127.0.0.1".
        if (defaultPort > 0 && !rhs.isEmpty()) {
            return finish(join(rhs, defaultPort));
        }
        return finish(value);
    }

    if (parts.size() >= 3) {
        QString const last = parts.last().trimmed();
        quint16 port = 0;
        if (parseNetworkPortText(last, &port)) {
            QString host = parts.at(parts.size() - 2).trimmed();
            if (host.isEmpty()) {
                host = defaultHost;
            }
            return finish(join(host, port));
        }
        // Common broken state while editing: "localhost:127.0.0.1".
        if (defaultPort > 0 && !last.isEmpty()) {
            return finish(join(last, defaultPort));
        }
    }

    return finish(value);
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
    if (port.isEmpty())
        return;
#if defined(Q_OS_LINUX)
    // 1.0.352 fix: dedup CANONICO (vedi DecodiumCatManager). /dev/ttyUSB0 e
    // /dev/serial/by-id/usb-... -> stesso device fisico = una sola voce.
    QString const key = comparablePortName(port);
    for (QString const& existing : ports) {
        if (comparablePortName(existing) == key)
            return;
    }
    ports << port;
#else
    if (!ports.contains(port, Qt::CaseInsensitive)) {
        ports << port;
    }
#endif
}

#if defined(Q_OS_LINUX)
QStringList enumerateLinuxSerialByIdPaths()
{
    QStringList paths;
    QDir const byIdDir(QStringLiteral("/dev/serial/by-id"));
    QFileInfoList const entries = byIdDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                                        QDir::Name);
    for (QFileInfo const& entry : entries) {
        // 1.0.352 fix: solo exists() (risolve il symlink), esclude i symlink rotti.
        if (entry.exists())
            paths << entry.absoluteFilePath();
    }
    return paths;
}

bool linuxSerialByIdPathAvailable(QString const& port)
{
    QString const value = normalizeDevicePath(port);
    if (!value.startsWith(QStringLiteral("/dev/serial/by-id/")))
        return false;
    QFileInfo const info(value);
    return info.exists();
}
#endif

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

#if defined(Q_OS_LINUX)
    for (QString const& path : enumerateLinuxSerialByIdPaths())
        appendUniqueSerialPort(ports, path);
#endif

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

bool serialPortCurrentlyAvailable(QString const& port)
{
    QString const wanted = comparablePortName(port);
    if (wanted.isEmpty()
        || wanted == QStringLiteral("cat")
        || wanted == QStringLiteral("none")) {
        return true;
    }

    for (QSerialPortInfo const& info : QSerialPortInfo::availablePorts()) {
        if (comparablePortName(info.portName()) == wanted
            || comparablePortName(info.systemLocation()) == wanted) {
            return true;
        }
    }

#if defined(Q_OS_LINUX)
    if (linuxSerialByIdPathAvailable(port))
        return true;
    for (QString const& path : enumerateLinuxSerialByIdPaths()) {
        if (comparablePortName(path) == wanted)
            return true;
    }
#endif

#if defined(Q_OS_WIN)
    QSettings serialMap(QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DEVICEMAP\\SERIALCOMM"),
                        QSettings::NativeFormat);
    for (QString const& key : serialMap.allKeys()) {
        if (comparablePortName(serialMap.value(key).toString()) == wanted) {
            return true;
        }
    }
#endif

    return false;
}

QString serialPortFamilyKey(QString const& port)
{
    QString value = comparablePortName(port);
    if (value.startsWith(QStringLiteral("/dev/")))
        value.remove(0, 5);
    if (value.startsWith(QStringLiteral("cu.")))
        value.remove(0, 3);
    if (value.startsWith(QStringLiteral("tty.")))
        value.remove(0, 4);

    int const dash = value.lastIndexOf(QLatin1Char('-'));
    if (dash > 0)
        value = value.left(dash);

    while (!value.isEmpty() && value.back().isDigit())
        value.chop(1);

    return value;
}

bool serialPortAutoFallbackCandidate(QString const& port)
{
    QString const value = comparablePortName(port);
    if (value.isEmpty()
        || value == QStringLiteral("cat")
        || value == QStringLiteral("none")) {
        return false;
    }

    static QStringList const excludedMarkers = {
        QStringLiteral("bluetooth"),
        QStringLiteral("debug-console"),
        QStringLiteral("incoming-port"),
        QStringLiteral("wireless"),
    };
    for (auto const& marker : excludedMarkers) {
        if (value.contains(marker, Qt::CaseInsensitive))
            return false;
    }
    return true;
}

QString autoFallbackSerialPortForMissing(QString const& missingPort)
{
    QStringList rawCandidates;
    for (QSerialPortInfo const& info : QSerialPortInfo::availablePorts()) {
        appendUniqueSerialPort(rawCandidates, info.systemLocation());
        appendUniqueSerialPort(rawCandidates, info.portName());
    }

#if defined(Q_OS_LINUX)
    for (QString const& path : enumerateLinuxSerialByIdPaths())
        appendUniqueSerialPort(rawCandidates, path);
#endif

#if defined(Q_OS_WIN)
    QSettings serialMap(QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DEVICEMAP\\SERIALCOMM"),
                        QSettings::NativeFormat);
    for (QString const& key : serialMap.allKeys())
        appendUniqueSerialPort(rawCandidates, serialMap.value(key).toString());
#endif

    QString const missingComparable = comparablePortName(missingPort);
    QString const missingFamily = serialPortFamilyKey(missingPort);

    QStringList seen;
    QString onlyUsable;
    int usableCount = 0;
    QString best;
    int bestScore = -100000;
    bool bestTied = false;

    for (QString const& raw : rawCandidates) {
        QString const candidate = normalizeDevicePath(raw);
        QString const comparable = comparablePortName(candidate);
        if (seen.contains(comparable, Qt::CaseInsensitive))
            continue;
        seen << comparable;

        if (!serialPortAutoFallbackCandidate(candidate)
            || !serialPortCurrentlyAvailable(candidate)
            || comparable == missingComparable) {
            continue;
        }

        ++usableCount;
        onlyUsable = candidate;

        int score = 0;
        QString const family = serialPortFamilyKey(candidate);
        if (!missingFamily.isEmpty() && family == missingFamily)
            score += 120;
        if (missingComparable.contains(QStringLiteral("usbserial"))
            && comparable.contains(QStringLiteral("usbserial"))) {
            score += 90;
        } else if (missingComparable.contains(QStringLiteral("usbmodem"))
                   && comparable.contains(QStringLiteral("usbmodem"))) {
            score += 90;
        } else if (missingComparable.contains(QStringLiteral("usb"))
                   && comparable.contains(QStringLiteral("usb"))) {
            score += 35;
        }
        if (comparable.contains(QStringLiteral("usb")))
            score += 20;
        if (candidate.startsWith(QStringLiteral("/dev/cu.")))
            score += 15;
        if (candidate.startsWith(QStringLiteral("/dev/tty.")))
            score -= 5;

        if (score > bestScore) {
            best = candidate;
            bestScore = score;
            bestTied = false;
        } else if (score == bestScore) {
            bestTied = true;
        }
    }

    if (!best.isEmpty() && !bestTied && bestScore >= 50)
        return best;
    if (usableCount == 1)
        return onlyUsable;
    return QString();
}

bool isSerialPortMissingFailure(QString const& reason)
{
    static QStringList const markers = {
        QStringLiteral("does not exist"),
        QStringLiteral("No such file"),
        QStringLiteral("The system cannot find"),
        QStringLiteral("cannot find the file"),
        QStringLiteral("ENODEV"),
        QStringLiteral("ENOENT"),
    };
    for (auto const& marker : markers) {
        if (reason.contains(marker, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool isDefaultSerialChoice(QString const& value)
{
    QString const text = value.trimmed().toLower();
    return text.isEmpty()
        || text == QStringLiteral("default")
        || text == QStringLiteral("predefinito")
        || text == QStringLiteral("auto");
}

QString normalizeDataBitsChoice(QString const& value)
{
    QString const text = value.trimmed();
    QString const lower = text.toLower();
    if (lower == QStringLiteral("7") || lower.contains(QStringLiteral("seven")))
        return QStringLiteral("7");
    if (lower == QStringLiteral("8") || lower.contains(QStringLiteral("eight")))
        return QStringLiteral("8");
    if (isDefaultSerialChoice(text))
        return QStringLiteral("Default");
    return QStringLiteral("Default");
}

QString normalizeStopBitsChoice(QString const& value)
{
    QString const text = value.trimmed();
    QString const lower = text.toLower();
    if (lower == QStringLiteral("2") || lower == QStringLiteral("2.0") || lower.contains(QStringLiteral("two")))
        return QStringLiteral("2");
    if (lower == QStringLiteral("1") || lower == QStringLiteral("1.0") || lower.contains(QStringLiteral("one")))
        return QStringLiteral("1");
    if (isDefaultSerialChoice(text))
        return QStringLiteral("Default");
    return QStringLiteral("Default");
}

QString normalizeHandshakeChoice(QString const& value)
{
    QString const text = value.trimmed();
    QString const lower = text.toLower();
    if (isDefaultSerialChoice(text))
        return QStringLiteral("Default");
    if (lower == QStringLiteral("none") || lower == QStringLiteral("no") || lower == QStringLiteral("off"))
        return QStringLiteral("none");
    if (lower == QStringLiteral("xonxoff") || lower == QStringLiteral("xon/xoff")
        || lower == QStringLiteral("xoff") || lower.contains(QStringLiteral("software")))
        return QStringLiteral("xonxoff");
    if (lower == QStringLiteral("hardware") || lower == QStringLiteral("hw"))
        return QStringLiteral("hardware");
    return QStringLiteral("Default");
}
}

// ── PIMPL privato ──────────────────────────────────────────────────────────
// Il transceiver gira su xcvThread ed entrambi vengono eliminati tramite il
// ciclo eventi Qt.  I puntatori devono quindi auto-invalidarsi: il segnale
// QThread::finished può completare i deleteLater prima che la callback nel
// thread GUI azzeri lo stato del manager (in particolare durante l'uscita
// dall'applicazione).
struct DecodiumTransceiverManagerPrivate
{
    TransceiverFactory  factory;
    QPointer<Transceiver> transceiver;            // owned via deleteLater
    QPointer<QThread>     xcvThread;               // owned via deleteLater / manager shutdown
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

    // Auto-save debounced: ogni cambio di parametro CAT salva entro 500ms.
    // Risolve "CAT non ricorda le impostazioni" quando l'utente chiude il
    // dialog senza premere Connetti.
    auto* saveTimer = new QTimer(this);
    saveTimer->setSingleShot(true);
    saveTimer->setInterval(500);
    connect(saveTimer, &QTimer::timeout, this, [this]() {
        qInfo().noquote() << "[CATSAVE] auto-save firing: rig=" << m_rigName
                          << "port=" << m_serialPort
                          << "baud=" << m_baudRate
                          << "autoConn=" << m_catAutoConnect;
        saveSettings();
    });
    auto scheduleSave = [saveTimer]() {
        qInfo().noquote() << "[CATSAVE] scheduleSave triggered (debounce 500ms)";
        saveTimer->start();
    };
    connect(this, &DecodiumTransceiverManager::rigNameChanged,        this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::serialPortChanged,     this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::baudRateChanged,       this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::dataBitsChanged,       this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::stopBitsChanged,       this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::handshakeChanged,      this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::pttMethodChanged,      this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::pttPortChanged,        this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::civAddressChanged,     this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::forceDtrChanged,       this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::dtrHighChanged,        this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::forceRtsChanged,       this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::rtsHighChanged,        this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::networkPortChanged,    this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::tciPortChanged,        this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::catKeepAliveChanged,   this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::pollIntervalChanged,   this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::catAutoConnectChanged, this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::audioAutoStartChanged, this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::splitModeChanged,      this, scheduleSave);
    connect(this, &DecodiumTransceiverManager::hrdStrictRadioMatchChanged, this, scheduleSave);
}

DecodiumTransceiverManager::~DecodiumTransceiverManager()
{
    // QPointer is essential here.  A finished transceiver may already have
    // been destroyed by deleteLater while the queued GUI cleanup callback has
    // not run yet.  Raw pointers made the virtual xcv->stop() below call into
    // freed memory during application shutdown.
    QPointer<Transceiver> xcv = d->transceiver;
    QPointer<QThread> thread = d->xcvThread;
    d->transceiver.clear();
    d->xcvThread.clear();

    // No state/reconnect callback may re-enter this manager while its derived
    // destructor is running.  Keep the worker-local finished->deleteLater and
    // finished->quit connections intact so Qt can still dispose it on the
    // correct thread.
    if (xcv) {
        QObject::disconnect(xcv.data(), nullptr, this, nullptr);
    }
    if (thread) {
        QObject::disconnect(thread.data(), nullptr, this, nullptr);
    }

    // Chiudi il rig prima di distruggere il QThread per evitare il fatal
    // "QThread: Destroyed while thread is still running".
    if (xcv && thread && thread->isRunning()) {
        QMetaObject::invokeMethod(xcv.data(), [xcv]() {
            if (xcv) {
                xcv->stop();
            }
        }, Qt::BlockingQueuedConnection);
    }

    if (thread && thread->isRunning()) {
        thread->quit();
        if (!thread->wait(4000)) {
            thread->terminate();
            thread->wait(1000);
        }
    }

    if (thread) {
        delete thread.data();
    }
}

void DecodiumTransceiverManager::setConnecting(bool v)
{
    if (m_connecting == v) {
        return;
    }
    m_connecting = v;
    if (v) {
        m_connectAttemptTimer.restart();
    } else {
        m_connectAttemptTimer.invalidate();
    }
    emit connectingChanged();
}

void DecodiumTransceiverManager::abortConnectingRigAfterTimeout(Transceiver* xcv, QThread* thread,
                                                                const QString& shownReason)
{
    if (!m_connecting || m_connected || d->transceiver != xcv) {
        return;
    }

    qWarning().noquote()
        << "[CATDBG] Connect watchdog abort"
        << "rig=" << m_rigName
        << "portType=" << m_portType
        << "network=" << m_networkPort
        << "elapsedMs=" << (m_connectAttemptTimer.isValid() ? m_connectAttemptTimer.elapsed() : -1)
        << "shownReason=" << shownReason;

    emit errorOccurred(QStringLiteral("CAT failure: ") + shownReason);

    // Keep the rig and thread owned by the manager until their normal
    // finished() path has run.  This prevents startup retries from reopening
    // a serial port while the watchdog shutdown is still executing.
    Q_UNUSED(thread);
    Q_UNUSED(xcv);
    disconnectRigInternal(false);
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
    if (!m_connected || (!d->transceiver && !d->xcvThread)) {
        return;
    }

    emit statusUpdate(reason + QStringLiteral(": riconnessione CAT per applicare il PTT"));
    disconnectRigInternal(true);
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

void DecodiumTransceiverManager::setDataBits(const QString& v)
{
    QString const normalized = normalizeDataBitsChoice(v);
    if (m_dataBits != normalized) {
        m_dataBits = normalized;
        emit dataBitsChanged();
    }
}

void DecodiumTransceiverManager::setStopBits(const QString& v)
{
    QString const normalized = normalizeStopBitsChoice(v);
    if (m_stopBits != normalized) {
        m_stopBits = normalized;
        emit stopBitsChanged();
    }
}

void DecodiumTransceiverManager::setHandshake(const QString& v)
{
    QString const normalized = normalizeHandshakeChoice(v);
    if (m_handshake != normalized) {
        m_handshake = normalized;
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

void DecodiumTransceiverManager::setNetworkPort(const QString& v)
{
    QString value = v.trimmed();
    if (0 == m_portType.compare(QStringLiteral("network"), Qt::CaseInsensitive)) {
        value = normalizeNetworkEndpoint(value, m_rigName);
    }
    if (m_networkPort != value) {
        m_networkPort = value;
        emit networkPortChanged();
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
        disconnectRigInternal(true);
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
        disconnectRigInternal(true);
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
        disconnectRigInternal(true);
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
        bool normalized = false;
        QString const cleanEndpoint = normalizeNetworkEndpoint(m_networkPort, m_rigName, &normalized);
        if (normalized) {
            qInfo().noquote()
                << "[CATDBG] Network endpoint normalized"
                << "rig=" << m_rigName
                << "raw=" << m_networkPort
                << "normalized=" << cleanEndpoint;
            m_networkPort = cleanEndpoint;
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
    QString const value = normalizeDataBitsChoice(s);
    if (value == QStringLiteral("7"))
        return TransceiverFactory::seven_data_bits;
    if (value == QStringLiteral("8"))
        return TransceiverFactory::eight_data_bits;
    return TransceiverFactory::default_data_bits;
}

static TransceiverFactory::StopBits parseStop(const QString& s)
{
    QString const value = normalizeStopBitsChoice(s);
    if (value == QStringLiteral("2"))
        return TransceiverFactory::two_stop_bits;
    if (value == QStringLiteral("1"))
        return TransceiverFactory::one_stop_bit;
    return TransceiverFactory::default_stop_bits;
}

static QString dataBitsName(TransceiverFactory::DataBits dataBits)
{
    switch (dataBits) {
    case TransceiverFactory::seven_data_bits: return QStringLiteral("7");
    case TransceiverFactory::eight_data_bits: return QStringLiteral("8");
    default:                                  return QStringLiteral("Default");
    }
}

static QString stopBitsName(TransceiverFactory::StopBits stopBits)
{
    switch (stopBits) {
    case TransceiverFactory::one_stop_bit:  return QStringLiteral("1");
    case TransceiverFactory::two_stop_bits: return QStringLiteral("2");
    default:                                return QStringLiteral("Default");
    }
}

static TransceiverFactory::TXAudioSource configuredTxAudioSource()
{
    QVariant const raw = decodium::profiledSettingsValue(QString {}, QStringLiteral("TXAudioSource"), 0);
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
    return decodium::profiledSettingsValue(QString {}, QStringLiteral("PWRandSWR"), false).toBool()
        || decodium::profiledSettingsValue(QString {}, QStringLiteral("CheckSWR"), false).toBool();
}

static TransceiverFactory::Handshake parseHandshake(const QString& s)
{
    QString const value = normalizeHandshakeChoice(s);
    if (value == QStringLiteral("Default")) return TransceiverFactory::handshake_default;
    if (value == QStringLiteral("xonxoff")) return TransceiverFactory::handshake_XonXoff;
    if (value == QStringLiteral("hardware")) return TransceiverFactory::handshake_hardware;
    return TransceiverFactory::handshake_none;
}

static QString handshakeName(TransceiverFactory::Handshake handshake)
{
    switch (handshake) {
    case TransceiverFactory::handshake_default:  return QStringLiteral("Default");
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
    int const basePollInterval = qMax(serialCat ? 2 : 1, requestedPollInterval);
    p.rig_name      = m->rigName();
    p.serial_port   = normalizeDevicePath(m->serialPort());
    bool networkEndpointNormalized = false;
    p.network_port  = normalizeNetworkEndpoint(m->networkPort(), m->rigName(),
                                                &networkEndpointNormalized);
    if (networkEndpointNormalized) {
        qInfo().noquote()
            << "[CATDBG] Network endpoint normalized"
            << "rig=" << m->rigName()
            << "raw=" << m->networkPort()
            << "normalized=" << p.network_port;
    }
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
    p.cat_keep_alive = m->catKeepAlive();
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
        << "activeTransport=" << activeCatTransport(p.rig_name, m->portType(), p.serial_port,
                                                    p.network_port, p.tci_port)
        << "serial=" << p.serial_port
        << "network=" << p.network_port
        << "tci=" << p.tci_port
        << "baud=" << p.baud
        << "dataBits=" << dataBitsName(p.data_bits)
        << "dataBitsRequested=" << m->dataBits()
        << "stopBits=" << stopBitsName(p.stop_bits)
        << "stopBitsRequested=" << m->stopBits()
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
        << "catKeepAlive=" << p.cat_keep_alive
        << "poll=" << (p.poll_interval & 0xffff);
    return p;
}

// ── connectRig ────────────────────────────────────────────────────────────
void DecodiumTransceiverManager::connectRig()
{
    if (catSuppressedByEnvironment()) {
        ++m_transientCatReconnectSerial;
        m_transientCatReconnectPending = false;
        m_transientCatRetryCount = 0;
        setConnecting(false);
        if (m_connected) {
            m_connected = false;
            emit connectedChanged();
        }
        updateTelemetry(0.0, 0.0);
        qInfo().noquote()
            << "[CATDBG] Connect suppressed by environment"
            << "rig=" << m_rigName
            << "portType=" << m_portType
            << "serial=" << m_serialPort
            << "network=" << m_networkPort
            << "tci=" << m_tciPort
            << "DECODIUM_DISABLE_CAT="
            << qEnvironmentVariable("DECODIUM_DISABLE_CAT")
            << "DECODIUM_RX_RECORD_DISABLE_CAT="
            << qEnvironmentVariable("DECODIUM_RX_RECORD_DISABLE_CAT");
        emit statusUpdate(catSuppressionReason());
        return;
    }

    // A rig/thread still present owns the serial handle until its worker has
    // emitted finished() and its QThread has emitted finished().  Never open
    // the same port from this call while that shutdown is still in progress;
    // queue the new connection for the thread-finished callback instead.
    if (m_disconnectInProgress) {
        m_reconnectAfterDisconnect = true;
        qInfo().noquote()
            << "[CATDBG] Connect queued: waiting for rig thread to finish"
            << "rig=" << m_rigName
            << "serial=" << m_serialPort;
        return;
    }

    if (d->transceiver || d->xcvThread) {
        if (m_connecting && !m_connected && d->transceiver) {
            qint64 const elapsedMs = m_connectAttemptTimer.isValid()
                ? m_connectAttemptTimer.elapsed()
                : -1;
            int const staleConnectThresholdMs = isHamRadioDeluxeRig(m_rigName)
                ? kHrdStartupWatchdogMs
                : 20000;
            if (elapsedMs < staleConnectThresholdMs && elapsedMs >= 0) {
                emit statusUpdate(QStringLiteral("Connessione a %1 gia' in corso...").arg(m_rigName));
                return;
            }
            qWarning().noquote()
                << "[CATDBG] Stale connect attempt reset"
                << "rig=" << m_rigName
                << "portType=" << m_portType
                << "network=" << m_networkPort
                << "elapsedMs=" << elapsedMs
                << "thresholdMs=" << staleConnectThresholdMs;
            emit statusUpdate(QStringLiteral("Connessione a %1 scaduta, riavvio tentativo...")
                              .arg(m_rigName));
        }
        disconnectRigInternal(true);
        return;
    }

    // Valida che il rig sia effettivamente nel registry (evita rig_init(0) → crash Hamlib 4.7)
    if (!d->factory.supported_transceivers().contains(m_rigName)) {
        emit errorOccurred("Transceiver non trovato nel registry: \"" + m_rigName
                           + "\". Seleziona un rig dalla lista.");
        return;
    }

    QString const lowerPortType = m_portType.trimmed().toLower();
    bool const serialTransport =
        lowerPortType == QStringLiteral("serial")
        || lowerPortType == QStringLiteral("usb");
    if (serialTransport
        && !isHamRadioDeluxeRig(m_rigName)
        && !serialPortCurrentlyAvailable(m_serialPort)) {
        QString const previousPort = m_serialPort;
        QString const fallbackPort = autoFallbackSerialPortForMissing(previousPort);
        if (!fallbackPort.isEmpty()) {
            setSerialPort(fallbackPort);
            saveSettings();
            refreshPorts();
            qInfo().noquote()
                << "[CATDBG] Serial port auto-fallback"
                << "rig=" << m_rigName
                << "portType=" << m_portType
                << "old=" << previousPort
                << "new=" << m_serialPort;
            emit statusUpdate(QStringLiteral("Porta CAT %1 non disponibile: uso %2.")
                              .arg(previousPort.trimmed().isEmpty()
                                   ? QStringLiteral("<non impostata>")
                                   : previousPort.trimmed(),
                                   m_serialPort));
        }
    }
    if (serialTransport
        && !isHamRadioDeluxeRig(m_rigName)
        && !serialPortCurrentlyAvailable(m_serialPort)) {
        refreshPorts();
        qInfo().noquote()
            << "[CATDBG] Connect deferred: serial port unavailable"
            << "rig=" << m_rigName
            << "portType=" << m_portType
            << "serial=" << m_serialPort;
        emit statusUpdate(QStringLiteral("Porta %1 non ancora disponibile, ritento CAT a breve...")
                          .arg(m_serialPort.trimmed().isEmpty()
                               ? QStringLiteral("<non impostata>")
                               : m_serialPort.trimmed()));
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
            << "activeTransport=" << activeCatTransport(params.rig_name, m_portType,
                                                        params.serial_port, params.network_port,
                                                        params.tci_port)
            << "serial=" << params.serial_port
            << "network=" << params.network_port
            << "tci=" << params.tci_port
            << "baud=" << params.baud
            << "dataBits=" << dataBitsName(params.data_bits)
            << "stopBits=" << stopBitsName(params.stop_bits)
            << "handshake=" << handshakeName(params.handshake)
            << "ptt=" << pttMethodName(params.ptt_type)
            << "pttPort=" << params.ptt_port
            << "split=" << splitModeName(params.split_mode)
            << "catKeepAlive=" << params.cat_keep_alive
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
    setConnecting(true);

    if (isHamRadioDeluxeRig(params.rig_name)) {
        QString const shownReason = sanitizeHamlibFailure(
            QStringLiteral("Ham Radio Deluxe retries exhausted sending command \"get id\""));
        qInfo().noquote()
            << "[CATDBG] HRD startup watchdog armed"
            << "timeoutMs=" << kHrdStartupWatchdogMs
            << "network=" << m_networkPort
            << "initialProbe=get id";
        QTimer::singleShot(kHrdStartupWatchdogMs, this, [this, xcv, thread, shownReason]() {
            abortConnectingRigAfterTimeout(xcv, thread, shownReason);
        });
    }

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
                bool const reconnect = m_reconnectAfterDisconnect;
                m_reconnectAfterDisconnect = false;
                if (d->xcvThread == thread)
                    d->xcvThread = nullptr;
                if (d->transceiver == xcv)
                    d->transceiver = nullptr;
                d->desired = Transceiver::TransceiverState {};
                m_disconnectInProgress = false;
                setConnecting(false);
                thread->deleteLater();
                if (m_connected) {
                    m_connected = false;
                    emit connectedChanged();
                }
                updateTelemetry(0.0, 0.0);
                if (reconnect) {
                    QTimer::singleShot(0, this, [this]() {
                        if (!d->transceiver && !d->xcvThread) {
                            connectRig();
                        }
                    });
                }
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
                    setConnecting(false);
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
                if (freq > 0 && m_frequency != freq) {
                    double const previousFrequency = m_frequency;
                    m_frequency = freq;
                    qInfo().noquote()
                        << "[CATDBG] Rig frequency update"
                        << "oldHz=" << QString::number(previousFrequency, 'f', 0)
                        << "newHz=" << QString::number(freq, 'f', 0)
                        << "mode=" << mode
                        << "split=" << spl
                        << "ptt=" << ptt;
                    emit frequencyChanged();
                }
                if (txf  > 0 && m_txFrequency != txf) { m_txFrequency = txf; emit txFrequencyChanged(); }
                if (m_mode    != mode) { m_mode    = mode; emit modeChanged(); }
                if (m_pttActive != ptt) { m_pttActive = ptt; emit pttActiveChanged(); }
                if (m_split   != spl)  { m_split   = spl;  emit splitChanged(); }
                updateTelemetry(static_cast<double>(state.power()) / 1000.0,
                                static_cast<double>(state.swr()) / 100.0,
                                static_cast<double>(state.alc()),
                                state.alc_valid());
                // S-meter: arriva dallo stesso stato, ma non passa da
                // updateTelemetry perche' non e' telemetria di trasmissione —
                // vive nell'altra meta' del ciclo, quella in cui si ascolta.
                if (state.level_valid() != m_strengthValid
                    || (state.level_valid() && state.level() != m_strengthDb)) {
                    m_strengthValid = state.level_valid();
                    m_strengthDb = state.level();
                    emit strengthChanged();
                }
                // 1.0.581 — strumenti del finale. Un solo segnale per tutti e
                // quattro: cambiano insieme, arrivano insieme, e quattro
                // segnali separati vorrebbero dire quattro giri di binding in
                // QML per un dato che e' uno solo.
                {
                    double const vd = state.vd() / 100.0;
                    double const id = state.id() / 100.0;
                    double const tp = state.pa_temp() / 10.0;
                    double const cp = state.comp() / 10.0;
                    double const pk = state.rfpower() / 10.0;   // millesimi -> percento
                    bool const cambiato =
                        m_drainVoltageValid  != state.vd_valid()
                        || m_drainCurrentValid  != state.id_valid()
                        || m_paTemperatureValid != state.pa_temp_valid()
                        || m_compressionValid   != state.comp_valid()
                        || (state.vd_valid()      && m_drainVoltage  != vd)
                        || (state.id_valid()      && m_drainCurrent  != id)
                        || (state.pa_temp_valid() && m_paTemperature != tp)
                        || (state.comp_valid()    && m_compressionDb != cp)
                        || m_powerSettingValid  != state.rfpower_valid()
                        || (state.rfpower_valid() && m_powerSettingPct != pk);
                    if (cambiato) {
                        m_drainVoltageValid  = state.vd_valid();
                        m_drainCurrentValid  = state.id_valid();
                        m_paTemperatureValid = state.pa_temp_valid();
                        m_compressionValid   = state.comp_valid();
                        m_drainVoltage  = vd;
                        m_drainCurrent  = id;
                        m_paTemperature = tp;
                        m_compressionDb = cp;
                        m_powerSettingValid = state.rfpower_valid();
                        m_powerSettingPct = pk;
                        emit paMetersChanged();
                    }
                }
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
                setConnecting(false);
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
                if (startupAttempt && isSerialPortMissingFailure(reason)) {
                    emit statusUpdate(shownReason + QStringLiteral(" Ritento CAT a breve..."));
                } else if (startupAttempt && isTransientCatIoFailure(reason)) {
                    emit statusUpdate(QStringLiteral("CAT non connesso: ") + shownReason);
                } else if ((wasConnected || recovering) && isTransientCatIoFailure(reason)) {
                    scheduleTransientReconnect(reason);
                } else {
                    emit errorOccurred("CAT failure: " + shownReason);
                }
                if (m_connected) { m_connected = false; emit connectedChanged(); }
                updateTelemetry(0.0, 0.0);
            },
            Qt::QueuedConnection);
    connect(xcv, &Transceiver::tciPcmSamplesReady,
            this, &DecodiumTransceiverManager::tciPcmSamplesProduced,
            Qt::DirectConnection);
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
    disconnectRigInternal(false);
}

void DecodiumTransceiverManager::disconnectRigInternal(bool reconnectAfterDisconnect)
{
    ++m_transientCatReconnectSerial;
    m_transientCatReconnectPending = false;
    m_reconnectAfterDisconnect = reconnectAfterDisconnect;

    // A public disconnect cancels a queued reconnect.  An internal reconnect
    // request leaves it armed so the thread-finished callback can start the
    // next session only after the old serial handle has really been closed.
    if (m_disconnectInProgress) {
        return;
    }

    setConnecting(false);
    if (!d->transceiver && !d->xcvThread) {
        m_disconnectInProgress = false;
        if (m_reconnectAfterDisconnect) {
            m_reconnectAfterDisconnect = false;
            QTimer::singleShot(0, this, [this]() { connectRig(); });
        }
        return;
    }

    auto* xcv    = d->transceiver.data();
    auto* thread = d->xcvThread.data();
    m_disconnectInProgress = true;

    // Keep both pointers valid until QThread::finished().  They are the
    // ownership/serialisation guard: clearing them here used to let a second
    // connectRig() call Hamlib rig_open while the old rig_close was still
    // running on the worker thread.
    d->desired = Transceiver::TransceiverState {};

    // Richiedi lo stop sul thread del rig: xcv->stop() deve completare il
    // cleanup Hamlib (PTT off, split reset, rig_close) prima che il thread
    // termini e che la riconnessione venga accodata.
    if (!thread || !thread->isRunning()) {
        if (xcv) {
            xcv->stop();
            xcv->deleteLater();
        }
        d->transceiver = nullptr;
        d->xcvThread = nullptr;
        if (thread) {
            thread->deleteLater();
        }
        m_disconnectInProgress = false;
        bool const reconnect = m_reconnectAfterDisconnect;
        m_reconnectAfterDisconnect = false;
        if (reconnect) {
            QTimer::singleShot(0, this, [this]() { connectRig(); });
        }
    } else {
        if (xcv) {
            QMetaObject::invokeMethod(xcv, [xcv, thread]() {
                xcv->stop();
                thread->quit();
            }, Qt::QueuedConnection);
        } else {
            thread->quit();
        }

        // Do not block the GUI thread waiting for Hamlib.  If a backend is
        // stuck in I/O, terminate only after the graceful close window; the
        // QThread::finished callback still gates the subsequent connect.
        QPointer<QThread> watchedThread(thread);
        QTimer::singleShot(2500, this, [this, watchedThread]() {
            if (!watchedThread || !watchedThread->isRunning()
                || d->xcvThread != watchedThread) {
                return;
            }
            qWarning().noquote()
                << "[CATDBG] Rig disconnect: graceful stop timed out, terminating thread";
            watchedThread->requestInterruption();
            watchedThread->quit();
            QTimer::singleShot(500, this, [watchedThread]() {
                if (watchedThread && watchedThread->isRunning()) {
                    watchedThread->terminate();
                }
            });
        });
    }

    if (m_connected) {
        m_connected = false;
        emit connectedChanged();
    }
    updateTelemetry(0.0, 0.0);
    emit statusUpdate("Disconnesso dal transceiver");
}

void DecodiumTransceiverManager::restartTransientCatConnectionNonBlocking()
{
    if (!m_transientCatReconnectPending) {
        return;
    }

    m_transientCatReconnectPending = false;
    disconnectRigInternal(true);
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
    // Polling now absorbs short USB/serial timeout bursts. If the backend still
    // reaches this path, give the rig/driver time to settle before reopening the
    // serial session; immediate reconnects can rewrite fake-split state and make
    // the operator hear CAT activity on the radio.
    int const delayMs = qMin(30000, 5000 * m_transientCatRetryCount);
    emit statusUpdate(tr("CAT interrotto, riconnessione automatica (%1/%2)...")
                          .arg(m_transientCatRetryCount)
                          .arg(maxRetries));

    QTimer::singleShot(delayMs, this, [this]() {
        restartTransientCatConnectionNonBlocking();
    });
}

void DecodiumTransceiverManager::updateTelemetry(double powerWatts, double swr, double alc, bool alcValid)
{
    if (m_powerWatts != powerWatts) {
        m_powerWatts = powerWatts;
        emit powerWattsChanged();
    }
    if (m_swr != swr) {
        m_swr = swr;
        emit swrChanged();
    }
    if (m_alc != alc || m_alcValid != alcValid) {  // 1.0.323 — ALC meter
        m_alc = alc;
        m_alcValid = alcValid;
        emit alcChanged();
    }
}

// ── sendState: invia TransceiverState al rig sul suo thread ───────────────
// Chiamato solo dal main thread → nessuna race condition su desired
static void sendState(DecodiumTransceiverManagerPrivate* d)
{
    if (!d->transceiver) return;
    auto* xcv  = d->transceiver.data();
    auto  st   = d->desired;                    // copia locale (valore)
    unsigned seq = ++(d->seqNum);
    QMetaObject::invokeMethod(xcv, [xcv, st, seq]() {
        xcv->set(st, seq);
    }, Qt::QueuedConnection);
}

static void sendStateAsync(DecodiumTransceiverManagerPrivate* d, char const* context)
{
    if (!d->transceiver) return;
    auto* xcv = d->transceiver.data();
    auto  st  = d->desired;
    unsigned seq = ++(d->seqNum);
    qInfo().noquote() << "[CAT-QUEUE] Hamlib state queued"
                      << "context=" << context << "seq=" << seq;
    QMetaObject::invokeMethod(xcv, [xcv, st, seq]() {
        xcv->set(st, seq);
    }, Qt::QueuedConnection);
}

void DecodiumTransceiverManager::setRigFrequency(double hz)
{
    if (hz <= 0.0) {
        return;
    }

    double const desiredHz = static_cast<double>(d->desired.frequency());
    bool const desiredMatches = desiredHz > 0.0 && sameCatFrequency(desiredHz, hz);
    bool const reportedMatches = m_frequency > 0.0 && sameCatFrequency(m_frequency, hz);
    if (desiredMatches && reportedMatches) {
        return;
    }

    // m_frequency is the frequency confirmed by the rig.  Do not publish an
    // optimistic value here: a deferred CI-V write would otherwise look as if
    // the QSY had completed, suppressing both a later retry and the bridge's
    // stale-poll guard.
    d->desired.frequency(static_cast<Transceiver::Frequency>(hz));

    // A frequency change must reach the Hamlib worker even when a following
    // mode, split or polling update is queued immediately afterwards. The
    // worker serialises states and rejects genuinely older sequence numbers;
    // sendLatestState(), on the other hand, can discard this QSY before the
    // worker has had a chance to send it to a slow CI-V rig.
    sendState(d.get());
}

void DecodiumTransceiverManager::setRigTxFrequency(double hz)
{
    double const rxHz = d->desired.frequency() > 0
        ? static_cast<double>(d->desired.frequency())
        : m_frequency;
    hz = sanitizedCatTxFrequencyHz(hz, rxHz, m_splitMode, QStringLiteral("setRigTxFrequency"));
    QString const splitModeText = m_splitMode.trimmed().toLower();
    bool const splitDisabledByConfig =
        splitModeText.isEmpty() || splitModeText == QStringLiteral("none");
    if (splitDisabledByConfig) {
        if (!sameCatFrequency(m_txFrequency, 0.0)) {
            m_txFrequency = 0.0;
            emit txFrequencyChanged();
        } else {
            m_txFrequency = 0.0;
        }
        d->desired.split(false);
        d->desired.tx_frequency(0);
        return;
    }
    bool const targetSplit = hz > 0.0;
    double const desiredTxHz = static_cast<double>(d->desired.tx_frequency());
    bool const desiredMatches = sameCatFrequency(desiredTxHz, hz)
        && d->desired.split() == targetSplit;
    bool const reportedTxMatches = sameCatFrequency(m_txFrequency, hz)
        && m_split == targetSplit;
    if (desiredMatches && reportedTxMatches) {
        return;
    }

    if (!sameCatFrequency(m_txFrequency, hz)) {
        m_txFrequency = hz;
        emit txFrequencyChanged();
    } else {
        m_txFrequency = hz;
    }
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

bool DecodiumTransceiverManager::prepareSatelliteHalfDuplex(double rxHz, double txHz)
{
    if (!m_connected || rxHz <= 0.0 || txHz <= 0.0 || qFuzzyCompare(rxHz + 1.0, txHz + 1.0)) {
        return false;
    }
    if (m_splitMode.trimmed().compare(QStringLiteral("rig"), Qt::CaseInsensitive) != 0) {
        return false;
    }

    bool const rxChanged = !sameCatFrequency(m_frequency, rxHz);
    bool const txChanged = !sameCatFrequency(m_txFrequency, txHz);
    bool const splitStateChanged = !m_split;
    m_frequency = rxHz;
    m_txFrequency = txHz;
    m_split = true;
    if (rxChanged) emit frequencyChanged();
    if (txChanged) emit txFrequencyChanged();
    if (splitStateChanged) emit splitChanged();

    d->desired.frequency(static_cast<Transceiver::Frequency>(rxHz));
    d->desired.split(true);
    d->desired.tx_frequency(static_cast<Transceiver::Frequency>(txHz));
    qInfo().noquote()
        << "[FT2SAT] CAT prepare half-duplex"
        << "rxHz=" << QString::number(rxHz, 'f', 0)
        << "txHz=" << QString::number(txHz, 'f', 0)
        << "settle=queued";
    sendState(d.get());
    return true;
}

bool DecodiumTransceiverManager::restoreSatelliteHalfDuplexRx(double rxHz)
{
    if (!m_connected || rxHz <= 0.0) {
        return false;
    }

    bool const rxChanged = !sameCatFrequency(m_frequency, rxHz);
    bool const txChanged = !sameCatFrequency(m_txFrequency, 0.0);
    bool const splitStateChanged = m_split;
    m_frequency = rxHz;
    m_txFrequency = 0.0;
    m_split = false;
    if (rxChanged) emit frequencyChanged();
    if (txChanged) emit txFrequencyChanged();
    if (splitStateChanged) emit splitChanged();

    d->desired.frequency(static_cast<Transceiver::Frequency>(rxHz));
    d->desired.split(false);
    d->desired.tx_frequency(0);
    qInfo().noquote()
        << "[FT2SAT] CAT restore RX"
        << "rxHz=" << QString::number(rxHz, 'f', 0);
    sendState(d.get());
    return true;
}

void DecodiumTransceiverManager::setRigTxFrequencyAndPtt(double hz, bool on)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    double const rxHz = d->desired.frequency() > 0
        ? static_cast<double>(d->desired.frequency())
        : m_frequency;
    hz = sanitizedCatTxFrequencyHz(hz, rxHz, m_splitMode, QStringLiteral("setRigTxFrequencyAndPtt"));
    QString const splitModeText = m_splitMode.trimmed().toLower();
    bool const splitDisabledByConfig =
        splitModeText.isEmpty() || splitModeText == QStringLiteral("none");
    if (splitDisabledByConfig) {
        hz = 0.0;
    }
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
    sendStateAsync(d.get(), "setRigTxFrequencyAndPtt");
    DIAG_INFO(QStringLiteral("[TX-TL] hamlib_set_tx_frequency_ptt queued_ms=%1 splitMode=%2 on=%3 rxHz=%4 txHz=%5")
                  .arg(totalTimer.elapsed())
                  .arg(m_splitMode)
                  .arg(on ? 1 : 0)
                  .arg(QString::number(static_cast<double>(d->desired.frequency()), 'f', 0),
                       QString::number(hz, 'f', 0)));
}

void DecodiumTransceiverManager::setRigTxFrequencyAndPttAsync(double hz, bool on)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    double const rxHz = d->desired.frequency() > 0
        ? static_cast<double>(d->desired.frequency())
        : m_frequency;
    hz = sanitizedCatTxFrequencyHz(hz, rxHz, m_splitMode, QStringLiteral("setRigTxFrequencyAndPttAsync"));
    QString const splitModeText = m_splitMode.trimmed().toLower();
    bool const splitDisabledByConfig =
        splitModeText.isEmpty() || splitModeText == QStringLiteral("none");
    if (splitDisabledByConfig) {
        hz = 0.0;
    }
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
        << "[CATDBG] Hamlib async set TX frequency + PTT"
        << "splitMode=" << m_splitMode
        << "on=" << on
        << "rxHz=" << QString::number(static_cast<double>(d->desired.frequency()), 'f', 0)
        << "txHz=" << QString::number(hz, 'f', 0);
    d->desired.ptt(on);
    sendState(d.get());
    DIAG_INFO(QStringLiteral("[TX-TL] hamlib_set_tx_frequency_ptt_async schedule_ms=%1 splitMode=%2 on=%3 rxHz=%4 txHz=%5")
                  .arg(totalTimer.elapsed())
                  .arg(m_splitMode)
                  .arg(on ? 1 : 0)
                  .arg(QString::number(static_cast<double>(d->desired.frequency()), 'f', 0),
                       QString::number(hz, 'f', 0)));
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
    // PTT ON/OFF resta nella stessa coda serializzata. Il bridge aggiorna
    // il proprio stato TX localmente e il worker completa il comando Hamlib.
    sendStateAsync(d.get(), on ? "setRigPtt:on" : "setRigPtt:off");
}

void DecodiumTransceiverManager::setRigMode(const QString& mode)
{
    Transceiver::MODE const targetMode = parseMode(mode);
    if (targetMode == Transceiver::UNK) {
        return;
    }

    QString const targetModeText = modeStr(targetMode);
    bool const desiredMatches = d->desired.mode() == targetMode;
    bool const reportedMatches = !m_mode.isEmpty()
        && m_mode.compare(targetModeText, Qt::CaseInsensitive) == 0;
    if (desiredMatches && (reportedMatches || m_mode.isEmpty())) {
        return;
    }

    d->desired.mode(targetMode);
    sendState(d.get());
}

void DecodiumTransceiverManager::setTciRxGainDb(double db)
{
    double const bounded = qBound(-60.0, db, 20.0);
    if (qFuzzyCompare(m_tciRxGainDb, bounded)) {
        return;
    }
    m_tciRxGainDb = bounded;
    emit tciRxGainDbChanged();
    // do_volume() viene invocato a ogni set(): basta aggiornare lo stato
    // desiderato perche' il nuovo guadagno valga dal frame successivo.
    d->desired.volume(m_tciRxGainDb);
    if (m_connected && d->transceiver) {
        sendState(d.get());
    }
}

void DecodiumTransceiverManager::setRigAudio(bool on, double periodSeconds, int blockSize)
{
    d->desired.online(true);
    d->desired.period(qBound(0.1, periodSeconds, 1800.0));
    d->desired.blocksize(qBound(256, blockSize, 48000));
    d->desired.volume(m_tciRxGainDb);
    d->desired.audio(on);
    // 1.0.204 — Audio off non richiede attesa sincrona; il main puo'
    // proseguire senza bloccarsi sul worker thread CAT.
    sendState(d.get());
}

void DecodiumTransceiverManager::setRigTune(bool on)
{
    d->desired.online(true);
    d->desired.tune(on);
    sendStateAsync(d.get(), on ? "setRigTune:on" : "setRigTune:off");
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
    sendStateAsync(d.get(), "startRigTxAudio");
}

void DecodiumTransceiverManager::stopRigTxAudio(bool quick)
{
    d->desired.online(true);
    d->desired.quick(quick);
    d->desired.tx_audio(false);
    // 1.0.204 — Stop TX audio non e' time-critical (il modulatore si ferma
    // localmente prima di questo invio). Async per non bloccare il main.
    sendState(d.get());
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
    const QString networkPort = normalizeNetworkEndpoint(m_networkPort, m_rigName);
    bool const canForceDtr = forceDtrAvailable();
    bool const canForceRts = forceRtsAvailable();

    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
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
    s.setValue("networkPort",  networkPort);
    s.setValue("tciPort",      m_tciPort);
    s.setValue("pttMethod",    m_pttMethod);
    s.setValue("pttPort",      pttPort);
    s.setValue("splitMode",    m_splitMode);
    s.setValue("civAddress",   m_civAddress);
    s.setValue("catKeepAlive", m_catKeepAlive);
    s.setValue("pollInterval", m_pollInterval);
    s.setValue("catAutoConnect", m_catAutoConnect);
    s.setValue("audioAutoStart", m_audioAutoStart);
    s.setValue("tciAudioEnabled", m_tciAudioEnabled);
    s.setValue("tciRxGainDb", m_tciRxGainDb);
    s.setValue("hrdStrictRadioMatch", m_hrdStrictRadioMatch);
    s.endGroup();
}

void DecodiumTransceiverManager::loadSettings()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, "Decodium", "Decodium3");
    decodium::beginActiveSettingsProfile(s);
    s.beginGroup("Transceiver");
    auto get = [&](const QString& k, const QVariant& def) { return s.value(k, def); };
    bool const hasSavedCivAddress = s.contains(QStringLiteral("civAddress"));
    int const savedCivAddress = qBound(0, get(QStringLiteral("civAddress"), 0).toInt(), 0xff);
    m_serialPort   = normalizeDevicePath(get("serialPort",   m_serialPort).toString());
    m_baudRate     = get("baudRate",     m_baudRate).toInt();
    m_dataBits     = normalizeDataBitsChoice(get("dataBits",     m_dataBits).toString());
    m_stopBits     = normalizeStopBitsChoice(get("stopBits",     m_stopBits).toString());
    m_handshake    = normalizeHandshakeChoice(get("handshake",    m_handshake).toString());
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
    m_catKeepAlive = get("catKeepAlive", m_catKeepAlive).toBool();
    int const rawPollInterval = get("pollInterval", m_pollInterval).toInt();
    int const secondsPart = rawPollInterval & 0xffff;
    m_pollInterval = qBound(1, secondsPart > 0 ? secondsPart : rawPollInterval, 99);
    m_catAutoConnect = get("catAutoConnect", m_catAutoConnect).toBool();
    m_audioAutoStart = get("audioAutoStart", m_audioAutoStart).toBool();
    m_hrdStrictRadioMatch = get("hrdStrictRadioMatch", m_hrdStrictRadioMatch).toBool();
    bool const legacyTciAudioFlag = (rawPollInterval & tci__audio) == tci__audio;
    m_tciAudioEnabled = get("tciAudioEnabled", m_tciAudioEnabled || legacyTciAudioFlag).toBool();
    m_tciRxGainDb = qBound(-60.0, get("tciRxGainDb", m_tciRxGainDb).toDouble(), 20.0);
    // setRigName DOPO gli altri per aggiornare portType correttamente
    QString rig = get("rigName", m_rigName).toString();
    s.endGroup();
    if (!hasModernSplitMode || m_splitMode.trimmed().isEmpty()) {
        m_splitMode = splitModeNameFromLegacyValue(s.value(QStringLiteral("SplitMode")), m_splitMode);
    }
    m_splitMode = splitModeName(parseSplit(m_splitMode.trimmed().toLower()));
    setRigName(rig);
    // setRigName supplies a Hamlib/model default for a newly selected radio.
    // Do not let that default overwrite a CI-V address the operator saved for
    // this CAT configuration/profile.
    if (hasSavedCivAddress && m_civAddress != savedCivAddress) {
        m_civAddress = savedCivAddress;
        emit civAddressChanged();
    }
    bool networkEndpointNormalized = false;
    QString const cleanEndpoint = normalizeNetworkEndpoint(m_networkPort, m_rigName,
                                                           &networkEndpointNormalized);
    if (networkEndpointNormalized) {
        qInfo().noquote()
            << "[CATDBG] Network endpoint normalized"
            << "rig=" << m_rigName
            << "raw=" << m_networkPort
            << "normalized=" << cleanEndpoint;
        m_networkPort = cleanEndpoint;
        emit networkPortChanged();
    }
    enforceForceLineAvailability();
}
