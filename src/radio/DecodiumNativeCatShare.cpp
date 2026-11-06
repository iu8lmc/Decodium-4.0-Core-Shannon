#include "DecodiumNativeCatShare.h"

#include "DecoPortCatFraming.h"
#include "DecoPortCatMux.h"
#include "DecoPortRadioLibrary.h"
#include "DecoPortRigDriver.h"
#include "DecoPortVirtualPorts.h"

#include <QCoreApplication>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QVariantMap>
#include <QtDebug>

namespace {
// Ogni quanto il thread del multiplexer manda all'interfaccia lo stato delle
// prese: l'interfaccia non deve mai fermarsi ad aspettarlo.
constexpr int kSnapshotMs = 1000;
const char* const kDecodiumTap = "Decodium";
const char* const kRawTap = "CAT grezzo";
} // namespace

DecodiumNativeCatShare::DecodiumNativeCatShare(QObject* parent)
    : QObject(parent)
{
    // Il thread e il suo ancoraggio non hanno parent: il thread si ferma e si
    // cancella nel distruttore, l'ancoraggio con lui.
    m_thread = new QThread;
    m_thread->setObjectName(QStringLiteral("DecoPortCatMux"));
    m_ctx = new QObject;
    m_ctx->moveToThread(m_thread);
    m_thread->start();
    m_status = tr("Off");
}

DecodiumNativeCatShare::~DecodiumNativeCatShare()
{
    stopMux();
    if (m_thread) {
        // Un oggetto di un thread senza ciclo di eventi viene cancellato quando
        // il thread finisce.
        m_ctx->deleteLater();
        m_thread->quit();
        m_thread->wait(5000);
        delete m_thread;
        m_thread = nullptr;
    }
}

bool DecodiumNativeCatShare::com0comInstalled() const
{
    return DecoPortVirtualPorts::available();
}

bool DecodiumNativeCatShare::com0comBundled() const
{
    return !DecoPortVirtualPorts::bundledInstaller().isEmpty();
}

void DecodiumNativeCatShare::setStatus(const QString& s)
{
    if (m_status == s)
        return;
    m_status = s;
    emit stateChanged();
}

void DecodiumNativeCatShare::configure(bool enabled, bool allowPtt, int rawPort,
                                       const QString& rememberedPair)
{
    if (rawPort != 0 && (rawPort < 1024 || rawPort > 65535))
        rawPort = 0;
    bool const changed = m_enabled != enabled || m_allowPtt != allowPtt || m_rawPort != rawPort;
    m_enabled = enabled;
    m_allowPtt = allowPtt;
    m_rawPort = rawPort;
    if (!rememberedPair.isEmpty())
        m_pair = rememberedPair;

    // Il permesso di trasmettere vale subito, anche a multiplexer avviato.
    if (m_mux) {
        DecoPortCatMux* mux = m_mux;
        QMetaObject::invokeMethod(m_ctx, [mux, allowPtt] { mux->setPttAllowed(allowPtt); },
                                  Qt::BlockingQueuedConnection);
    }
    if (!enabled) {
        release();
        m_vcomStatus.clear();
        setStatus(tr("Off"));
    } else if (!m_mux) {
        // Lo stato dice gia' cosa manca, prima ancora che il CAT si apra.
        if (!DecoPortVirtualPorts::available())
            m_vcomStatus = com0comBundled()
                ? tr("com0com is not installed: press \"Create virtual port\" to install it "
                     "(Windows will ask for administrator rights)")
                : tr("com0com is not installed and its installer is missing next to Decodium");
        else if (m_pair.isEmpty())
            m_vcomStatus = tr("No virtual port yet: press \"Create virtual port\" "
                              "(Windows will ask for administrator rights)");
        setStatus(tr("Waiting for the CAT to open"));
    }
    if (changed)
        emit configChanged();
    emit stateChanged();
}

QString DecodiumNativeCatShare::redirect(const QString& port, const QString& rigName,
                                         DecoPortLineSettings settings, bool pttOnCatLines,
                                         QString* why)
{
    if (!m_enabled) {
        if (m_mux)
            release();
        return {};
    }
    auto refuse = [&](const QString& reason) {
        release();
        setStatus(reason);
        if (why)
            *why = reason;
        return QString();
    };
    QString const physical = port.trimmed();
    if (physical.isEmpty() || physical.compare(QLatin1String("None"), Qt::CaseInsensitive) == 0)
        return refuse(tr("Radio-protocol sharing not started: no CAT serial port is set"));
    if (pttOnCatLines)
        return refuse(tr("Radio-protocol sharing not started: PTT uses RTS/DTR on the CAT port, "
                         "which cannot travel through the shared socket. Use CAT or VOX PTT, "
                         "or a separate PTT port."));

    // Il dialetto viene dalla libreria radio di DecoPort, cioe' da Hamlib.
    // Una radio che la libreria non conosce si condivide lo stesso, a turni
    // di botta e risposta.
    DecoPortRadioLibrary const& lib = DecoPortRadioLibrary::instance();
    int model = lib.resolve(rigName);
    if (!model)
        model = DecoPortRigDriver::resolveModel(rigName, QString());
    QString protocol = model ? lib.protocolOf(model) : QString();
    if (protocol.isEmpty())
        protocol = QStringLiteral("binary");

    // Quello che l'utente ha impostato in Decodium vale; il resto lo dice la
    // Hamlib installata per quel modello.
    bool known = false;
    DecoPortLineSettings const caps = model ? DecoPortRigDriver::serialSettings(model, &known)
                                            : DecoPortLineSettings{};
    if (settings.dataBits <= 0)
        settings.dataBits = known ? caps.dataBits : 8;
    if (settings.stopBits <= 0)
        settings.stopBits = known ? caps.stopBits : 1;
    if (settings.handshake.isEmpty())
        settings.handshake = known ? caps.handshake : QStringLiteral("none");
    settings.parity = known ? caps.parity : QLatin1Char('N');

    QString const key = QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9")
                            .arg(physical.toUpper(), protocol)
                            .arg(settings.baud).arg(settings.dataBits).arg(settings.stopBits)
                            .arg(settings.handshake).arg(settings.rts).arg(settings.dtr)
                            .arg(m_rawPort);
    if (m_mux && key == m_key)
        return m_address;

    stopMux();
    QString error;
    startMux(physical, protocol, settings, &error);
    if (!m_mux)
        return refuse(tr("Radio-protocol sharing not started: %1").arg(error));

    m_key = key;
    m_active = true;
    setStatus(tr("Sharing %1 in its own protocol (%2)").arg(physical, protocol));
    qInfo().noquote() << "[NATIVESHARE]" << physical << "protocol" << protocol
                      << "baud" << settings.baud << "Hamlib via" << m_address;
    openVirtualTap();
    return m_address;
}

void DecodiumNativeCatShare::startMux(const QString& port, const QString& protocol,
                                      const DecoPortLineSettings& s, QString* why)
{
    quint16 tap = 0;
    QString rawError;
    bool const allowPtt = m_allowPtt;
    int const rawPort = m_rawPort;
    QPointer<DecodiumNativeCatShare> self(this);
    // Il multiplexer nasce nel suo thread: i suoi timer, la seriale e le prese
    // appartengono a quel thread, e solo quel thread li tocca.
    QMetaObject::invokeMethod(m_ctx, [&] {
        auto* mux = new DecoPortCatMux;
        mux->setDialect(decoport::cat::dialectFromName(protocol));
        mux->setPttAllowed(allowPtt);
        if (!mux->open(port, s)) {
            if (why)
                *why = tr("%1: %2").arg(port, mux->errorString());
            delete mux;
            return;
        }
        // La presa di Decodium: fidata (il divieto di PTT non la riguarda) e
        // senza le frasi spontanee della radio, che Hamlib non si aspetta.
        tap = mux->addTcpTap(0, QString::fromLatin1(kDecodiumTap), false, true);
        if (!tap) {
            if (why)
                *why = tr("no local socket for Decodium's CAT");
            mux->close();
            delete mux;
            return;
        }
        if (rawPort > 0 && !mux->addTcpTap(quint16(rawPort), QString::fromLatin1(kRawTap), true))
            rawError = tr("raw TCP port %1 is busy").arg(rawPort);

        connect(mux, &DecoPortCatMux::lost, this, [self](const QString& why) {
            if (!self)
                return;
            self->m_active = false;
            self->setStatus(tr("Radio port lost: %1").arg(why));
        });
        connect(mux, &DecoPortCatMux::pttReleased, this, [self](const QString& who) {
            if (self)
                self->setStatus(tr("Transmit released: %1 went away while transmitting").arg(who));
        });
        connect(mux, &DecoPortCatMux::pttRefused, this, [self](const QString& who) {
            if (self)
                self->setStatus(tr("Transmit refused to %1: allow transmit to let other programs key the radio")
                                    .arg(who));
        });

        auto* timer = new QTimer(mux);
        timer->setInterval(kSnapshotMs);
        connect(timer, &QTimer::timeout, mux, [self, mux] {
            int programs = 0;
            bool vcomOpen = false;
            for (const QVariant& v : mux->taps()) {
                QVariantMap const t = v.toMap();
                if (t.value(QStringLiteral("name")).toString() == QLatin1String(kDecodiumTap))
                    continue;
                programs += t.value(QStringLiteral("clients")).toInt();
                vcomOpen = vcomOpen || t.value(QStringLiteral("kind")).toString() == QLatin1String("serial");
            }
            QString const traffic = tr("%1 commands, %2 answers routed, %3 to everyone, %4 transmit refused")
                                        .arg(mux->commandsSent()).arg(mux->repliesRouted())
                                        .arg(mux->broadcasts()).arg(mux->pttRefusedCount());
            QMetaObject::invokeMethod(qApp, [self, programs, traffic, vcomOpen] {
                if (self)
                    self->onSnapshot(programs, traffic, vcomOpen);
            }, Qt::QueuedConnection);
        });
        timer->start();
        m_mux = mux;
    }, Qt::BlockingQueuedConnection);

    if (m_mux) {
        m_address = QStringLiteral("127.0.0.1:%1").arg(tap);
        if (!rawError.isEmpty())
            m_vcomStatus = rawError;
    }
}

void DecodiumNativeCatShare::stopMux()
{
    if (!m_mux)
        return;
    DecoPortCatMux* mux = m_mux;
    m_mux = nullptr;
    // close() riporta in ricezione la radio se qualcuno l'ha lasciata in aria.
    QMetaObject::invokeMethod(m_ctx, [mux] {
        mux->close();
        delete mux;
    }, Qt::BlockingQueuedConnection);
    m_vcomOpen = false;
}

void DecodiumNativeCatShare::release()
{
    stopMux();
    m_key.clear();
    m_address.clear();
    bool const wasActive = m_active;
    m_active = false;
    m_programs = 0;
    if (wasActive) {
        setStatus(m_enabled ? tr("Waiting for the CAT to open") : tr("Off"));
        emit stateChanged();
    }
}

void DecodiumNativeCatShare::openVirtualTap()
{
    if (!m_mux)
        return;
    QStringList const pair = m_pair.split(QLatin1Char('|'));
    if (pair.size() != 2) {
        m_vcomStatus = DecoPortVirtualPorts::available()
            ? tr("No virtual port yet: press \"Create virtual port\" "
                 "(Windows will ask for administrator rights)")
            : tr("com0com is not installed: press \"Create virtual port\" to install it "
                 "(Windows will ask for administrator rights)");
        emit stateChanged();
        return;
    }
    // La coppia ricordata deve esistere ancora, e il suo driver deve essere
    // partito: con Secure Boot attivo Windows puo' rifiutarlo (codice 52).
    int problem = -1;
    for (const DecoPortVirtualPorts::Pair& p : DecoPortVirtualPorts::list())
        if (p.visible == pair.at(0) && p.hidden == pair.at(1))
            problem = p.problem;
    if (problem < 0) {
        m_vcomStatus = tr("The virtual port %1 no longer exists: press \"Create virtual port\"")
                           .arg(pair.at(0));
        emit stateChanged();
        return;
    }
    if (problem > 0) {
        m_vcomStatus = DecoPortVirtualPorts::problemText(problem);
        emit stateChanged();
        return;
    }
    QString error;
    bool ok = false;
    DecoPortCatMux* mux = m_mux;
    QString const hidden = pair.at(1);
    QString const visible = pair.at(0);
    // Le coppie create da Decodium hanno il DCD collegato all'apertura
    // dell'altra parte (com0com dcd=ropen): cosi' si sa quando un programma
    // chiude la porta, anche se non usa DTR, e il suo PTT si abbassa.
    QMetaObject::invokeMethod(m_ctx, [&] {
        ok = mux->addSerialTap(hidden, visible, true, &error);
    }, Qt::BlockingQueuedConnection);
    m_vcomOpen = ok;
    m_vcomVisible = visible;
    m_vcomStatus = ok ? tr("In the other program choose your radio model on %1").arg(visible)
                      : error;
    emit stateChanged();
}

void DecodiumNativeCatShare::onSnapshot(int programs, const QString& traffic, bool open)
{
    if (programs == m_programs && traffic == m_traffic && open == m_vcomOpen)
        return;
    m_programs = programs;
    m_traffic = traffic;
    // La porta virtuale puo' sparire da sola (driver tolto, coppia rimossa).
    m_vcomOpen = open;
    emit stateChanged();
}

void DecodiumNativeCatShare::createVirtualPort()
{
    if (m_busy)
        return;
    m_busy = true;
    m_vcomStatus = tr("Preparing the virtual port: Windows will ask for administrator rights");
    emit stateChanged();

    QString const remembered = m_pair;
    QPointer<DecodiumNativeCatShare> self(this);
    // Installare e creare aspettano l'utente davanti alla richiesta di
    // Windows: mai nel thread dell'interfaccia, mai in quello del CAT.
    QThread* worker = QThread::create([self, remembered] {
        QString error;
        QString pair;
        if (!DecoPortVirtualPorts::available())
            DecoPortVirtualPorts::install(&error);
        if (error.isEmpty()) {
            QStringList const want = remembered.split(QLatin1Char('|'));
            for (const DecoPortVirtualPorts::Pair& p : DecoPortVirtualPorts::list())
                if (want.size() == 2 && p.visible == want.at(0) && p.hidden == want.at(1))
                    pair = remembered;
            if (pair.isEmpty()) {
                DecoPortVirtualPorts::Pair const p = DecoPortVirtualPorts::create(&error);
                if (!p.hidden.isEmpty()) {
                    pair = p.visible + QLatin1Char('|') + p.hidden;
                    if (error.isEmpty())
                        error = DecoPortVirtualPorts::problemText(p.problem);
                }
            }
        }
        QMetaObject::invokeMethod(qApp, [self, pair, error] {
            if (!self)
                return;
            self->m_busy = false;
            if (!pair.isEmpty() && pair != self->m_pair) {
                self->m_pair = pair;
                emit self->pairRemembered(pair);
            }
            if (!error.isEmpty()) {
                self->m_vcomStatus = error;
                emit self->stateChanged();
                return;
            }
            if (self->m_mux && !self->m_vcomOpen)
                self->openVirtualTap();
            else if (!self->m_mux)
                self->m_vcomStatus = tr("Virtual port %1 ready: it opens with the CAT")
                                         .arg(pair.section(QLatin1Char('|'), 0, 0));
            emit self->stateChanged();
        }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}
