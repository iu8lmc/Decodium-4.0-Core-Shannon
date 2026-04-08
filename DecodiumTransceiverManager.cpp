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
#include <QDebug>
#include <atomic>
#include <stdexcept>

namespace
{
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

// ── rigList ────────────────────────────────────────────────────────────────
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
}

// ── Helpers enum ──────────────────────────────────────────────────────────
static TransceiverFactory::PTTMethod parsePtt(const QString& s)
{
    if (s == "CAT") return TransceiverFactory::PTT_method_CAT;
    if (s == "DTR") return TransceiverFactory::PTT_method_DTR;
    if (s == "RTS") return TransceiverFactory::PTT_method_RTS;
    return TransceiverFactory::PTT_method_VOX;
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
    if (s == "CW")     return Transceiver::CW;
    if (s == "CW-R")   return Transceiver::CW_R;
    if (s == "USB")    return Transceiver::USB;
    if (s == "LSB")    return Transceiver::LSB;
    if (s == "FSK")    return Transceiver::FSK;
    if (s == "FSK-R")  return Transceiver::FSK_R;
    if (s == "DATA-U") return Transceiver::DIG_U;
    if (s == "DATA-L") return Transceiver::DIG_L;
    if (s == "AM")     return Transceiver::AM;
    if (s == "FM")     return Transceiver::FM;
    if (s == "DIG-FM") return Transceiver::DIG_FM;
    return Transceiver::UNK;
}

// ── buildParams ───────────────────────────────────────────────────────────
static TransceiverFactory::ParameterPack buildParams(const DecodiumTransceiverManager* m)
{
    TransceiverFactory::ParameterPack p;
    p.rig_name      = m->rigName();
    p.serial_port   = normalizeDevicePath(m->serialPort());
    p.network_port  = m->networkPort();
    p.usb_port      = normalizeDevicePath(m->serialPort());
    p.tci_port      = m->tciPort();
    p.baud          = m->baudRate();
    p.data_bits     = parseData(m->dataBits());
    p.stop_bits     = parseStop(m->stopBits());
    p.handshake     = parseHandshake(m->handshake());
    p.force_dtr     = m->forceDtr();
    p.dtr_high      = m->dtrHigh();
    p.force_rts     = m->forceRts();
    p.rts_high      = m->rtsHigh();
    p.ptt_type      = parsePtt(m->pttMethod());
    p.audio_source  = TransceiverFactory::TX_audio_source_front;
    p.split_mode    = parseSplit(m->splitMode());
    // ptt_port: "CAT" usa la stessa porta CAT; altrimenti porta seriale dedicata
    p.ptt_port      = m->pttPort().isEmpty() ? "CAT" : normalizeDevicePath(m->pttPort());
    p.poll_interval = m->pollInterval();
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
                thread->deleteLater();
                if (m_connected) {
                    m_connected = false;
                    emit connectedChanged();
                }
            },
            Qt::QueuedConnection);

    // Aggiornamenti di stato dal rig → main thread
    connect(xcv, &Transceiver::update,
            this,
            [this](Transceiver::TransceiverState const& state, unsigned /*seq*/) {
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
            },
            Qt::QueuedConnection);

    // Errori dal rig
    connect(xcv, &Transceiver::failure,
            this,
            [this](QString const& reason) {
                emit errorOccurred("CAT failure: " + reason);
                if (m_connected) { m_connected = false; emit connectedChanged(); }
            },
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
    emit statusUpdate("Disconnesso dal transceiver");
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

    QSettings s("Decodium", "Decodium3");
    s.beginGroup("Transceiver");
    s.setValue("rigName",      m_rigName);
    s.setValue("serialPort",   serialPort);
    s.setValue("baudRate",     m_baudRate);
    s.setValue("dataBits",     m_dataBits);
    s.setValue("stopBits",     m_stopBits);
    s.setValue("handshake",    m_handshake);
    s.setValue("forceDtr",     m_forceDtr);
    s.setValue("dtrHigh",      m_dtrHigh);
    s.setValue("forceRts",     m_forceRts);
    s.setValue("rtsHigh",      m_rtsHigh);
    s.setValue("networkPort",  m_networkPort);
    s.setValue("tciPort",      m_tciPort);
    s.setValue("pttMethod",    m_pttMethod);
    s.setValue("pttPort",      pttPort);
    s.setValue("splitMode",    m_splitMode);
    s.setValue("pollInterval", m_pollInterval);
    s.setValue("catAutoConnect", m_catAutoConnect);
    s.setValue("audioAutoStart", m_audioAutoStart);
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
    m_pttMethod    = get("pttMethod",    m_pttMethod).toString();
    m_pttPort      = normalizeDevicePath(get("pttPort",      m_pttPort).toString());
    m_splitMode    = get("splitMode",    m_splitMode).toString();
    m_pollInterval = get("pollInterval", m_pollInterval).toInt();
    m_catAutoConnect = get("catAutoConnect", m_catAutoConnect).toBool();
    m_audioAutoStart = get("audioAutoStart", m_audioAutoStart).toBool();
    // setRigName DOPO gli altri per aggiornare portType correttamente
    QString rig = get("rigName", m_rigName).toString();
    s.endGroup();
    setRigName(rig);
}
