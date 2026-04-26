// DecodiumTransceiverManager.cpp
// Wrapper QML-friendly attorno a TransceiverFactory (Hamlib, OmniRig, HRD, DXLab, TCI).

#include "DecodiumTransceiverManager.h"

#include "Transceiver/TransceiverFactory.hpp"
#include "Transceiver/Transceiver.hpp"
#include "Transceiver/TransceiverBase.hpp"

#include <QThread>
#include <QSerialPortInfo>
#include <QSettings>
#include <QMetaObject>
#include <QMetaType>
#include <QDebug>
#include <QRegularExpression>
#include <atomic>
#include <stdexcept>

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
    bool const serialCat = 0 == portType.compare(QStringLiteral("serial"), Qt::CaseInsensitive);
    bool const hardwareHandshake = 0 == handshake.trimmed().compare(QStringLiteral("hardware"), Qt::CaseInsensitive);
    return serialCat
        && !hardwareHandshake
        && !(0 == pttMethod.compare(QStringLiteral("RTS"), Qt::CaseInsensitive)
             && pttPortSharesCatPort(serialPort, pttPort));
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
    refreshPorts();
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
    QString const effective = normalized.isEmpty() ? QStringLiteral("CAT") : normalized;
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
    if (s == "emulate") return TransceiverFactory::split_mode_emulate;
    return TransceiverFactory::split_mode_none;
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
    if (s == "xonxoff")  return TransceiverFactory::handshake_XonXoff;
    if (s == "hardware") return TransceiverFactory::handshake_hardware;
    return TransceiverFactory::handshake_none;
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
    p.rig_name      = m->rigName();
    p.serial_port   = normalizeDevicePath(m->serialPort());
    p.network_port  = m->networkPort();
    p.usb_port      = normalizeDevicePath(m->serialPort());
    p.tci_port      = m->tciPort();
    p.baud          = m->baudRate();
    p.data_bits     = parseData(m->dataBits());
    p.stop_bits     = parseStop(m->stopBits());
    p.handshake     = parseHandshake(m->handshake());
    p.force_dtr     = canForceDtr && m->forceDtr();
    p.dtr_high      = canForceDtr && m->dtrHigh();
    p.force_rts     = canForceRts && m->forceRts();
    p.rts_high      = canForceRts && m->rtsHigh();
    p.ptt_type      = parsePtt(m->pttMethod());
    p.audio_source  = configuredTxAudioSource();
    p.split_mode    = parseSplit(m->splitMode());
    // "CAT" in PTT Port means "use the same serial port as CAT", as in WSJT-X.
    p.ptt_port      = resolvedPttPort(m);
    p.poll_interval = m->pollInterval();
    if (configuredPwrAndSwrEnabled())
        p.poll_interval |= do__pwr;
    if (m->tciAudioEnabled()
        && 0 == m->portType().compare(QStringLiteral("tci"), Qt::CaseInsensitive)) {
        p.poll_interval |= tci__audio;
    }
    return p;
}

// ── connectRig ────────────────────────────────────────────────────────────
void DecodiumTransceiverManager::connectRig()
{
    if (d->transceiver) {
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
        auto uptr = d->factory.create(params, thread);
        xcv = uptr.release();          // trasferisce ownership al thread (via deleteLater)
    } catch (std::exception const& e) {
        emit errorOccurred(QString("Errore creazione transceiver: %1").arg(e.what()));
        delete thread;
        d->xcvThread = nullptr;
        return;
    } catch (...) {
        emit errorOccurred("Errore sconosciuto durante la creazione del transceiver");
        delete thread;
        d->xcvThread = nullptr;
        return;
    }

    d->transceiver = xcv;
    d->desired = Transceiver::TransceiverState {};
    d->desired.online(true);

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
                d->desired = state;
                d->desired.audio(requestedAudio);
                d->desired.period(requestedPeriod);
                d->desired.blocksize(requestedBlocksize);
                double  freq = static_cast<double>(state.frequency());
                double  txf  = static_cast<double>(state.tx_frequency());
                QString mode = modeStr(state.mode());
                bool    ptt  = state.ptt();
                bool    spl  = state.split();
                bool    onl  = state.online();

                if (m_connected != onl) {
                    m_connected = onl;
                    emit connectedChanged();
                    emit statusUpdate(onl ? ("Connesso: " + m_rigName) : "Disconnesso");
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
    connect(xcv, &Transceiver::failure,
            this,
            [this](QString const& reason) {
                d->desired.online(false);
                emit errorOccurred("CAT failure: " + sanitizeHamlibFailure(reason));
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
    d->desired.tx_frequency(static_cast<Transceiver::Frequency>(hz));
    sendState(d.get());
}

void DecodiumTransceiverManager::setRigPtt(bool on)
{
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

// ── refreshPorts ──────────────────────────────────────────────────────────
void DecodiumTransceiverManager::refreshPorts()
{
    QStringList ports;
    for (const auto& info : QSerialPortInfo::availablePorts()) {
#if defined(Q_OS_WIN)
        const QString port = info.portName();
#else
        const QString port = info.systemLocation().isEmpty() ? info.portName() : info.systemLocation();
#endif
        if (!port.isEmpty())
            ports << port;
    }
    if (ports != m_portList) {
        m_portList = ports;
        emit portListChanged();
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
    setRigName(rig);
    enforceForceLineAvailability();
}
