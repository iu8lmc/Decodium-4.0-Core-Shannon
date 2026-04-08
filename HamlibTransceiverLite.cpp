#include "HamlibTransceiverLite.h"

#include <hamlib/rig.h>

#include <QDebug>
#include <cstring>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Map rmode_t to a human-readable QString using Hamlib's own rig_strrmode().
static QString rmodeToString(rmode_t m)
{
    const char* s = rig_strrmode(m);
    return (s && *s) ? QString::fromLatin1(s).trimmed() : QString();
}

/// Map a user-facing mode string ("USB", "LSB", ...) back to rmode_t
/// using Hamlib's rig_parse_mode().
static rmode_t stringToRmode(const QString& mode)
{
    return rig_parse_mode(mode.toLatin1().constData());
}

/// Map Hamlib's rig_port_t enum to a readable string.
static QString portTypeString(rig_port_t pt)
{
    switch (pt) {
    case RIG_PORT_SERIAL:  return QStringLiteral("serial");
    case RIG_PORT_NETWORK: return QStringLiteral("network");
    case RIG_PORT_USB:     return QStringLiteral("usb");
    default:               return QStringLiteral("other");
    }
}

/// Map handshake string to enum.
static enum serial_handshake_e parseHandshake(const QString& hs)
{
    if (hs.compare(QLatin1String("Hardware"), Qt::CaseInsensitive) == 0 ||
        hs.compare(QLatin1String("RTS/CTS"),  Qt::CaseInsensitive) == 0)
        return RIG_HANDSHAKE_HARDWARE;
    if (hs.compare(QLatin1String("XON/XOFF"), Qt::CaseInsensitive) == 0 ||
        hs.compare(QLatin1String("Software"), Qt::CaseInsensitive) == 0)
        return RIG_HANDSHAKE_XONXOFF;
    return RIG_HANDSHAKE_NONE;
}

// ---------------------------------------------------------------------------
// rig_list_foreach callback — collects RigInfo into a QList
// ---------------------------------------------------------------------------

static int collectRigCaps(const struct rig_caps* caps, rig_ptr_t data)
{
    auto* list = static_cast<QList<HamlibTransceiverLite::RigInfo>*>(data);

    HamlibTransceiverLite::RigInfo info;
    info.modelId     = static_cast<int>(caps->rig_model);
    info.manufacturer = QString::fromLatin1(caps->mfg_name  ? caps->mfg_name  : "");
    info.modelName    = QString::fromLatin1(caps->model_name ? caps->model_name : "");
    info.portType     = portTypeString(caps->port_type);

    list->append(info);
    return 1;   // 1 == continue enumeration
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

HamlibTransceiverLite::HamlibTransceiverLite(QObject* parent)
    : QObject(parent)
{
}

HamlibTransceiverLite::~HamlibTransceiverLite()
{
    close();
}

// ---------------------------------------------------------------------------
// availableRigs  (static)
// ---------------------------------------------------------------------------

QList<HamlibTransceiverLite::RigInfo> HamlibTransceiverLite::availableRigs()
{
    // Load all backends exactly once
    static bool backendsLoaded = false;
    if (!backendsLoaded) {
        rig_load_all_backends();
        backendsLoaded = true;
    }

    QList<RigInfo> list;
    list.reserve(400);
    rig_list_foreach(collectRigCaps, &list);
    return list;
}

// ---------------------------------------------------------------------------
// open / close / isOpen
// ---------------------------------------------------------------------------

bool HamlibTransceiverLite::open(int modelId, const QString& port,
                                  int baudRate, int dataBits, int stopBits,
                                  const QString& handshake)
{
    // Close any existing connection first
    close();

    m_rig = rig_init(static_cast<rig_model_t>(modelId));
    if (!m_rig) {
        m_lastError = QStringLiteral("rig_init() failed for model %1").arg(modelId);
        emit errorOccurred(m_lastError);
        return false;
    }

    m_modelId = modelId;

    // Configure the port via the public HAMLIB_RIGPORT macro
    hamlib_port_t* rp = HAMLIB_RIGPORT(m_rig);
    std::strncpy(rp->pathname,
                 port.toLocal8Bit().constData(),
                 HAMLIB_FILPATHLEN - 1);
    rp->pathname[HAMLIB_FILPATHLEN - 1] = '\0';

    rp->parm.serial.rate      = baudRate;
    rp->parm.serial.data_bits = dataBits;
    rp->parm.serial.stop_bits = stopBits;
    rp->parm.serial.handshake = parseHandshake(handshake);

    int ret = rig_open(m_rig);
    if (ret != RIG_OK) {
        m_lastError = QString::fromLatin1(rigerror(ret));
        emit errorOccurred(m_lastError);
        rig_cleanup(m_rig);
        m_rig = nullptr;
        return false;
    }

    m_isOpen = true;
    emit connected();
    return true;
}

void HamlibTransceiverLite::close()
{
    if (m_rig) {
        if (m_isOpen) {
            rig_close(m_rig);
        }
        rig_cleanup(m_rig);
        m_rig = nullptr;
    }

    if (m_isOpen) {
        m_isOpen = false;
        emit disconnected();
    }
}

bool HamlibTransceiverLite::isOpen() const
{
    return m_isOpen;
}

// ---------------------------------------------------------------------------
// Frequency
// ---------------------------------------------------------------------------

double HamlibTransceiverLite::frequency() const
{
    if (!m_rig || !m_isOpen) return 0.0;

    freq_t freq = 0;
    int ret = rig_get_freq(m_rig, RIG_VFO_CURR, &freq);
    if (ret != RIG_OK) {
        // const_cast: we store error in mutable-like member for convenience
        auto* self = const_cast<HamlibTransceiverLite*>(this);
        self->m_lastError = QString::fromLatin1(rigerror(ret));
        return 0.0;
    }
    return static_cast<double>(freq);
}

bool HamlibTransceiverLite::setFrequency(double hz)
{
    if (!m_rig || !m_isOpen) return false;

    int ret = rig_set_freq(m_rig, RIG_VFO_CURR, static_cast<freq_t>(hz));
    if (ret != RIG_OK) {
        m_lastError = QString::fromLatin1(rigerror(ret));
        emit errorOccurred(m_lastError);
        return false;
    }
    emit frequencyChanged(hz);
    return true;
}

// ---------------------------------------------------------------------------
// Mode
// ---------------------------------------------------------------------------

QString HamlibTransceiverLite::mode() const
{
    if (!m_rig || !m_isOpen) return QString();

    rmode_t rmode = RIG_MODE_NONE;
    pbwidth_t width = 0;
    int ret = rig_get_mode(m_rig, RIG_VFO_CURR, &rmode, &width);
    if (ret != RIG_OK) {
        auto* self = const_cast<HamlibTransceiverLite*>(this);
        self->m_lastError = QString::fromLatin1(rigerror(ret));
        return QString();
    }
    return rmodeToString(rmode);
}

bool HamlibTransceiverLite::setMode(const QString& mode)
{
    if (!m_rig || !m_isOpen) return false;

    rmode_t rm = stringToRmode(mode);
    if (rm == RIG_MODE_NONE) {
        m_lastError = QStringLiteral("Unknown mode: %1").arg(mode);
        emit errorOccurred(m_lastError);
        return false;
    }

    // Use RIG_PASSBAND_NORMAL (0) to let Hamlib pick a sensible default width
    int ret = rig_set_mode(m_rig, RIG_VFO_CURR, rm, RIG_PASSBAND_NORMAL);
    if (ret != RIG_OK) {
        m_lastError = QString::fromLatin1(rigerror(ret));
        emit errorOccurred(m_lastError);
        return false;
    }
    emit modeChanged(mode);
    return true;
}

// ---------------------------------------------------------------------------
// PTT
// ---------------------------------------------------------------------------

bool HamlibTransceiverLite::ptt() const
{
    if (!m_rig || !m_isOpen) return false;

    ptt_t pttVal = RIG_PTT_OFF;
    int ret = rig_get_ptt(m_rig, RIG_VFO_CURR, &pttVal);
    if (ret != RIG_OK) {
        auto* self = const_cast<HamlibTransceiverLite*>(this);
        self->m_lastError = QString::fromLatin1(rigerror(ret));
        return false;
    }
    return pttVal != RIG_PTT_OFF;
}

bool HamlibTransceiverLite::setPtt(bool on)
{
    if (!m_rig || !m_isOpen) return false;

    int ret = rig_set_ptt(m_rig, RIG_VFO_CURR,
                          on ? RIG_PTT_ON : RIG_PTT_OFF);
    if (ret != RIG_OK) {
        m_lastError = QString::fromLatin1(rigerror(ret));
        emit errorOccurred(m_lastError);
        return false;
    }
    emit pttChanged(on);
    return true;
}

// ---------------------------------------------------------------------------
// Split
// ---------------------------------------------------------------------------

bool HamlibTransceiverLite::isSplit() const
{
    if (!m_rig || !m_isOpen) return false;

    split_t split = RIG_SPLIT_OFF;
    vfo_t txVfo = RIG_VFO_CURR;
    int ret = rig_get_split_vfo(m_rig, RIG_VFO_CURR, &split, &txVfo);
    if (ret != RIG_OK) {
        auto* self = const_cast<HamlibTransceiverLite*>(this);
        self->m_lastError = QString::fromLatin1(rigerror(ret));
        return false;
    }
    return split != RIG_SPLIT_OFF;
}

bool HamlibTransceiverLite::setSplit(bool on, double txFreqHz)
{
    if (!m_rig || !m_isOpen) return false;

    int ret;

    if (on) {
        // Enable split — TX on VFO_SUB (common convention)
        ret = rig_set_split_vfo(m_rig, RIG_VFO_CURR, RIG_SPLIT_ON, RIG_VFO_SUB);
        if (ret != RIG_OK) {
            m_lastError = QString::fromLatin1(rigerror(ret));
            emit errorOccurred(m_lastError);
            return false;
        }

        // Optionally set TX frequency
        if (txFreqHz > 0) {
            ret = rig_set_split_freq(m_rig, RIG_VFO_CURR,
                                     static_cast<freq_t>(txFreqHz));
            if (ret != RIG_OK) {
                m_lastError = QString::fromLatin1(rigerror(ret));
                emit errorOccurred(m_lastError);
                return false;
            }
        }
    } else {
        ret = rig_set_split_vfo(m_rig, RIG_VFO_CURR, RIG_SPLIT_OFF, RIG_VFO_CURR);
        if (ret != RIG_OK) {
            m_lastError = QString::fromLatin1(rigerror(ret));
            emit errorOccurred(m_lastError);
            return false;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// Info / error
// ---------------------------------------------------------------------------

QString HamlibTransceiverLite::rigName() const
{
    if (!m_rig) return QString();
    const struct rig_caps* caps = m_rig->caps;
    if (!caps) return QString();
    return QStringLiteral("%1 %2")
        .arg(QString::fromLatin1(caps->mfg_name  ? caps->mfg_name  : ""))
        .arg(QString::fromLatin1(caps->model_name ? caps->model_name : ""));
}

QString HamlibTransceiverLite::lastError() const
{
    return m_lastError;
}
