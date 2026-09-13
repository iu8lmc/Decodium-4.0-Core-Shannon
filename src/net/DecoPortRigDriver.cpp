// SPDX-License-Identifier: GPL-3.0-or-later
#include "DecoPortRigDriver.h"

#include <QDebug>
#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

#include <hamlib/rig.h>

namespace {

// Confronto fra nomi di radio: "FT-991A", "FT991A" e "ft 991 a" sono la stessa
// radio scritta da tre persone diverse. Si tiene solo cio' che identifica.
QString normalised(const QString& s)
{
    QString out;
    out.reserve(s.size());
    for (QChar c : s) {
        if (c.isLetterOrNumber())
            out.append(c.toLower());
    }
    return out;
}

struct CatalogueEntry
{
    int     model {0};
    QString mfg;
    QString name;
    int     status {0};
};

int collectEntry(const struct rig_caps* caps, rig_ptr_t data)
{
    if (!caps || !data)
        return 1;
    auto* out = static_cast<QVector<CatalogueEntry>*>(data);
    CatalogueEntry e;
    e.model  = static_cast<int>(caps->rig_model);
    e.mfg    = QString::fromLatin1(caps->mfg_name ? caps->mfg_name : "");
    e.name   = QString::fromLatin1(caps->model_name ? caps->model_name : "");
    e.status = static_cast<int>(caps->status);
    out->append(e);
    return 1;   // 1 = continua l'iterazione
}

// Hamlib parte con la traccia accesa e scrive su stderr a ogni comando: su un
// gateway che interroga la radio due volte al secondo sono centinaia di
// kilobyte al minuto di I/O sincrono, che nessuno ha chiesto e che rallenta il
// thread da cui esce — misurato, faceva perdere il 6% dell'audio pubblicato.
void silenceHamlibOnce()
{
    static bool done = false;
    if (done)
        return;
    done = true;
    rig_set_debug(RIG_DEBUG_ERR);
}

QVector<CatalogueEntry> catalogue()
{
    static QVector<CatalogueEntry> cached;
    if (!cached.isEmpty())
        return cached;
    silenceHamlibOnce();
    rig_load_all_backends();
    QVector<CatalogueEntry> entries;
    rig_list_foreach(collectEntry, &entries);
    cached = entries;
    return cached;
}

} // namespace

struct DecoPortRigDriver::Impl
{
    RIG* rig {nullptr};
};

DecoPortRigDriver::DecoPortRigDriver()
    : QObject(nullptr)
    , d(new Impl)
{
    // Il thread nasce con l'oggetto: aprire una seriale, e ancora di piu'
    // leggerne una che non risponde, e' un'attesa che l'interfaccia non deve
    // mai subire.
    m_thread = new QThread;
    m_thread->setObjectName(QStringLiteral("DecoPortRigDriver"));
    moveToThread(m_thread);
    m_thread->start();
}

DecoPortRigDriver::~DecoPortRigDriver()
{
    if (m_thread) {
        QThread* const ownerThread = QThread::currentThread();
        if (thread() == ownerThread) {
            doClose();
        } else if (m_thread->isRunning()) {
            // Close Hamlib on the worker that owns it, then return this object
            // (and its stopped timer child) to the deleting thread.  This
            // keeps both QObject destruction and serial teardown on valid
            // thread affinities.
            QMetaObject::invokeMethod(this, [this, ownerThread]() {
                doClose();
                moveToThread(ownerThread);
            }, Qt::BlockingQueuedConnection);
        }
        m_thread->quit();
        if (!m_thread->wait(5000))
            m_thread->terminate();
        delete m_thread;
        m_thread = nullptr;
    }
    delete d;
}

QVariantList DecoPortRigDriver::hamlibCatalogue()
{
    QVariantList out;
    for (const CatalogueEntry& e : catalogue()) {
        QVariantMap m;
        m.insert(QStringLiteral("model"), e.model);
        m.insert(QStringLiteral("manufacturer"), e.mfg);
        m.insert(QStringLiteral("name"), e.name);
        m.insert(QStringLiteral("status"), e.status);
        out.append(m);
    }
    return out;
}

// La risoluzione va dal piu' specifico al meno: prima il nome completo che
// l'identita' USB ha dedotto, poi la radice del modello. Fra piu' voci che
// combaciano si preferisce quella con il nome piu' corto, che nei cataloghi
// Hamlib e' quasi sempre il modello base invece di una variante.
int DecoPortRigDriver::resolveModel(const QString& rigLabel,
                                    const QString& rigToken,
                                    QString* matchedName)
{
    QVector<CatalogueEntry> const entries = catalogue();
    if (entries.isEmpty())
        return 0;

    QStringList needles;
    if (!rigToken.trimmed().isEmpty())
        needles << normalised(rigToken);
    // Un'etichetta come "Yaesu FT-991 / FT-991A / FT-DX10" contiene piu' nomi:
    // si provano tutti, nell'ordine in cui sono scritti.
    for (const QString& part : rigLabel.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        QString const n = normalised(part);
        if (!n.isEmpty() && !needles.contains(n))
            needles << n;
    }

    for (const QString& needle : needles) {
        if (needle.size() < 3)
            continue;
        CatalogueEntry best;
        for (const CatalogueEntry& e : entries) {
            QString const full = normalised(e.mfg + e.name);
            QString const only = normalised(e.name);
            bool const hit = (only == needle) || (full == needle)
                             || only.contains(needle) || needle.contains(only);
            if (!hit || only.isEmpty())
                continue;
            if (best.model == 0 || only.size() < normalised(best.name).size())
                best = e;
        }
        if (best.model != 0) {
            if (matchedName)
                *matchedName = (best.mfg + QLatin1Char(' ') + best.name).trimmed();
            return best.model;
        }
    }
    return 0;
}

bool DecoPortRigDriver::open(const QString& port, int baudRate, int civAddress, int model)
{
    if (model == 0 || port.trimmed().isEmpty())
        return false;
    {
        QMutexLocker lock(&m_stateMutex);
        // startDecoPortGateway() can be requested more than once while a slow
        // Hamlib rig_open() is still pending.  Coalesce those requests rather
        // than queueing another full serial timeout behind the first one.
        if (m_open || m_opening)
            return true;
        m_opening = true;
    }
    QMetaObject::invokeMethod(this, [this, port, baudRate, civAddress, model]() {
        doOpen(port, baudRate, civAddress, model);
    }, Qt::QueuedConnection);
    return true;
}

void DecoPortRigDriver::doOpen(const QString& port, int baudRate, int civAddress, int model)
{
    doClose(false);
    if (model == 0 || port.trimmed().isEmpty()) {
        QMutexLocker lock(&m_stateMutex);
        m_lastError = tr("no radio model resolved");
        m_opening = false;
        return;
    }

    silenceHamlibOnce();
    rig_load_all_backends();
    d->rig = rig_init(static_cast<rig_model_t>(model));
    if (!d->rig) {
        QString reason;
        {
            QMutexLocker lock(&m_stateMutex);
            m_lastError = tr("Hamlib refused model %1").arg(model);
            m_opening = false;
            reason = m_lastError;
        }
        emit failed(reason);
        return;
    }

    // La configurazione passa dai token, non dai campi della struttura: fra una
    // versione di Hamlib e l'altra i campi si sono spostati, i token no.
    auto setConf = [this](const char* name, const QString& value) {
        token_t const t = rig_token_lookup(d->rig, name);
        if (t == RIG_CONF_END)
            return;
        rig_set_conf(d->rig, t, value.toLatin1().constData());
    };
    setConf("rig_pathname", port);
    if (baudRate > 0)
        setConf("serial_speed", QString::number(baudRate));
    if (civAddress > 0)
        setConf("civaddr", QString::number(civAddress));

    int const rc = rig_open(d->rig);
    if (rc != RIG_OK) {
        QString const why = QString::fromLatin1(rigerror(rc)).trimmed();
        rig_cleanup(d->rig);
        d->rig = nullptr;
        QString reason;
        {
            QMutexLocker lock(&m_stateMutex);
            m_lastError = tr("cannot open %1: %2").arg(port, why);
            m_opening = false;
            reason = m_lastError;
        }
        emit failed(reason);
        return;
    }

    QString name;
    if (const struct rig_caps* caps = rig_get_caps(static_cast<rig_model_t>(model))) {
        name = QString::fromLatin1(caps->mfg_name ? caps->mfg_name : "")
               + QLatin1Char(' ')
               + QString::fromLatin1(caps->model_name ? caps->model_name : "");
        name = name.trimmed();
    }
    {
        QMutexLocker lock(&m_stateMutex);
        m_rigName = name;
        m_lastError.clear();
        m_open = true;
        m_opening = false;
    }

    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        m_pollTimer->setInterval(500);
        connect(m_pollTimer, &QTimer::timeout, this, &DecoPortRigDriver::poll);
    }
    m_pollTimer->start();
    poll();
    emit opened(name);
}

void DecoPortRigDriver::close()
{
    QMetaObject::invokeMethod(this, [this]() { doClose(); }, Qt::QueuedConnection);
}

void DecoPortRigDriver::doClose(bool clearOpening)
{
    if (m_pollTimer)
        m_pollTimer->stop();
    if (!d->rig) {
        if (clearOpening) {
            QMutexLocker lock(&m_stateMutex);
            m_opening = false;
        }
        return;
    }
    // Chiudere lasciando il PTT su sarebbe il modo peggiore di andarsene.
    rig_set_ptt(d->rig, RIG_VFO_CURR, RIG_PTT_OFF);
    rig_close(d->rig);
    rig_cleanup(d->rig);
    d->rig = nullptr;
    {
        QMutexLocker lock(&m_stateMutex);
        m_frequencyHz = 0.0;
        m_modeName.clear();
        m_ptt = false;
        m_rigName.clear();
        m_open = false;
        if (clearOpening)
            m_opening = false;
    }
    emit closed();
}

bool DecoPortRigDriver::isOpen() const
{
    QMutexLocker lock(&m_stateMutex);
    return m_open;
}

void DecoPortRigDriver::poll()
{
    if (!d->rig)
        return;

    freq_t freq = 0;
    rmode_t mode = RIG_MODE_NONE;
    pbwidth_t width = 0;
    ptt_t ptt = RIG_PTT_OFF;

    bool changed = false;
    if (rig_get_freq(d->rig, RIG_VFO_CURR, &freq) == RIG_OK && freq > 0) {
        QMutexLocker lock(&m_stateMutex);
        if (!qFuzzyCompare(m_frequencyHz + 1.0, static_cast<double>(freq) + 1.0)) {
            m_frequencyHz = static_cast<double>(freq);
            changed = true;
        }
    }
    if (rig_get_mode(d->rig, RIG_VFO_CURR, &mode, &width) == RIG_OK) {
        QString const name = QString::fromLatin1(rig_strrmode(mode));
        QMutexLocker lock(&m_stateMutex);
        if (m_modeName != name) {
            m_modeName = name;
            changed = true;
        }
    }
    if (rig_get_ptt(d->rig, RIG_VFO_CURR, &ptt) == RIG_OK) {
        bool const on = (ptt != RIG_PTT_OFF);
        QMutexLocker lock(&m_stateMutex);
        if (m_ptt != on) {
            m_ptt = on;
            changed = true;
        }
    }
    if (changed)
        emit stateChanged();
}

void DecoPortRigDriver::setFrequencyHz(double hz)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, hz]() { setFrequencyHz(hz); },
                                  Qt::QueuedConnection);
        return;
    }
    if (!d->rig || hz <= 0.0)
        return;
    rig_set_freq(d->rig, RIG_VFO_CURR, static_cast<freq_t>(hz));
    poll();
}

void DecoPortRigDriver::setModeName(const QString& name)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, name]() { setModeName(name); },
                                  Qt::QueuedConnection);
        return;
    }
    if (!d->rig || name.trimmed().isEmpty())
        return;
    // I nomi neutri di DecoPort dicono cosa deve fare la radio; qui vanno
    // tradotti in quello che Hamlib chiama con lo stesso significato.
    QString hamlibName = name.trimmed().toUpper();
    if (hamlibName == QLatin1String("DIGU"))
        hamlibName = QStringLiteral("PKTUSB");
    else if (hamlibName == QLatin1String("DIGL"))
        hamlibName = QStringLiteral("PKTLSB");
    rmode_t const mode = rig_parse_mode(hamlibName.toLatin1().constData());
    if (mode == RIG_MODE_NONE)
        return;
    rig_set_mode(d->rig, RIG_VFO_CURR, mode, RIG_PASSBAND_NOCHANGE);
    poll();
}

void DecoPortRigDriver::setPtt(bool on)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, on]() { setPtt(on); },
                                  Qt::QueuedConnection);
        return;
    }
    if (!d->rig)
        return;
    rig_set_ptt(d->rig, RIG_VFO_CURR, on ? RIG_PTT_ON : RIG_PTT_OFF);
    {
        QMutexLocker lock(&m_stateMutex);
        m_ptt = on;
    }
    emit stateChanged();
}

double DecoPortRigDriver::frequencyHz() const
{
    QMutexLocker lock(&m_stateMutex);
    return m_frequencyHz;
}

QString DecoPortRigDriver::modeName() const
{
    QMutexLocker lock(&m_stateMutex);
    return m_modeName;
}

bool DecoPortRigDriver::ptt() const
{
    QMutexLocker lock(&m_stateMutex);
    return m_ptt;
}

QString DecoPortRigDriver::rigName() const
{
    QMutexLocker lock(&m_stateMutex);
    return m_rigName;
}

QString DecoPortRigDriver::lastError() const
{
    QMutexLocker lock(&m_stateMutex);
    return m_lastError;
}
