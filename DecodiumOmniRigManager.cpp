#include "DecodiumOmniRigManager.h"

#if defined(Q_OS_WIN)
#  if defined(__has_include)
#    if __has_include(<QtAxContainer/qaxobject.h>)
#      include <QtAxContainer/qaxobject.h>
#      define DECODIUM_HAS_QAXOBJECT 1
#    elif __has_include(<ActiveQt/qaxobject.h>)
#      include <ActiveQt/qaxobject.h>
#      define DECODIUM_HAS_QAXOBJECT 1
#    elif __has_include(<QtActiveQt/qaxobject.h>)
#      include <QtActiveQt/qaxobject.h>
#      define DECODIUM_HAS_QAXOBJECT 1
#    elif __has_include(<QAxObject>)
#      include <QAxObject>
#      define DECODIUM_HAS_QAXOBJECT 1
#    else
#      define DECODIUM_HAS_QAXOBJECT 0
#    endif
#  else
#    include <QAxObject>
#    define DECODIUM_HAS_QAXOBJECT 1
#  endif
#else
#  define DECODIUM_HAS_QAXOBJECT 0
#endif
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QThread>
#include <QTimer>
#include <QDebug>
#include <QVariant>

namespace {
constexpr int OMNI_ST_NOTCONFIGURED = 0;
constexpr int OMNI_ST_DISABLED = 1;
constexpr int OMNI_ST_PORTBUSY = 2;
constexpr int OMNI_ST_NOTRESPONDING = 3;
constexpr int OMNI_ST_ONLINE = 4;

[[maybe_unused]] constexpr int OMNI_PM_FREQ = 0x00000002;
[[maybe_unused]] constexpr int OMNI_PM_FREQA = 0x00000004;
[[maybe_unused]] constexpr int OMNI_PM_FREQB = 0x00000008;
[[maybe_unused]] constexpr int OMNI_PM_RX = 0x00200000;
[[maybe_unused]] constexpr int OMNI_PM_TX = 0x00400000;
[[maybe_unused]] constexpr int OMNI_PM_CW_U = 0x00800000;
[[maybe_unused]] constexpr int OMNI_PM_CW_L = 0x01000000;
[[maybe_unused]] constexpr int OMNI_PM_SSB_U = 0x02000000;
[[maybe_unused]] constexpr int OMNI_PM_SSB_L = 0x04000000;
[[maybe_unused]] constexpr int OMNI_PM_DIG_U = 0x08000000;
[[maybe_unused]] constexpr int OMNI_PM_DIG_L = 0x10000000;
[[maybe_unused]] constexpr int OMNI_PM_AM = 0x20000000;
[[maybe_unused]] constexpr int OMNI_PM_FM = 0x40000000;

[[maybe_unused]] QString omniRigStatusName(int status)
{
    switch (status) {
    case OMNI_ST_NOTCONFIGURED: return QStringLiteral("Non configurato");
    case OMNI_ST_DISABLED: return QStringLiteral("Disabilitato");
    case OMNI_ST_PORTBUSY: return QStringLiteral("Porta occupata");
    case OMNI_ST_NOTRESPONDING: return QStringLiteral("Non risponde");
    case OMNI_ST_ONLINE: return QStringLiteral("Online");
    default: return QStringLiteral("Sconosciuto (%1)").arg(status);
    }
}

QString normalizePttMethod(QString method, const QString& fallback = QStringLiteral("CAT"))
{
    method = method.trimmed().toUpper();
    if (method == QStringLiteral("VOX") || method == QStringLiteral("CAT")
            || method == QStringLiteral("DTR") || method == QStringLiteral("RTS")) {
        return method;
    }
    return fallback;
}

QString pttMethodFromSettings(const QVariant& raw, const QString& fallback)
{
    if (!raw.isValid())
        return fallback;

    const QString text = raw.toString().trimmed().toUpper();
    const QString normalized = normalizePttMethod(text, QString());
    if (!normalized.isEmpty())
        return normalized;

    bool ok = false;
    const int legacyValue = raw.toInt(&ok);
    if (ok) {
        switch (legacyValue) {
        case 0: return QStringLiteral("VOX");
        case 1: return QStringLiteral("CAT");
        case 2: return QStringLiteral("DTR");
        case 3: return QStringLiteral("RTS");
        default: break;
        }
    }
    return fallback;
}

int boundedPollInterval(int value)
{
    return qBound(1, value, 99);
}

bool isOmniRigRigName(const QString& rigName)
{
    return rigName.trimmed().startsWith(QStringLiteral("OmniRig"), Qt::CaseInsensitive);
}

bool envFlagEnabled(const char* name)
{
    const QString value = qEnvironmentVariable(name).trimmed().toLower();
    return value == QStringLiteral("1")
        || value == QStringLiteral("true")
        || value == QStringLiteral("yes")
        || value == QStringLiteral("on");
}

bool omniRigMockRequested()
{
    QSettings s("Decodium", "Decodium3");
    s.beginGroup("CAT_OmniRig");
    const bool settingEnabled = s.value(QStringLiteral("mockEnabled"), false).toBool();
    s.endGroup();
    return settingEnabled || envFlagEnabled("DECODIUM_OMNIRIG_MOCK");
}

double omniRigMockFrequency()
{
    bool ok = false;
    const double envFreq = qEnvironmentVariable("DECODIUM_OMNIRIG_MOCK_FREQ").toDouble(&ok);
    if (ok && envFreq > 0.0) {
        return envFreq;
    }
    return 14074000.0;
}

QString omniRigMockMode()
{
    const QString mode = qEnvironmentVariable("DECODIUM_OMNIRIG_MOCK_MODE").trimmed().toUpper();
    return mode.isEmpty() ? QStringLiteral("DATA-U") : mode;
}
}

#if defined(Q_OS_WIN) && DECODIUM_HAS_QAXOBJECT
// OmniRig COM ProgID
static const char* OMNIRIG_PROGID = "OmniRig.OmniRigX";

namespace {
QAxObject* createOmniRigObject(QObject* parent)
{
    auto* object = new QAxObject(OMNIRIG_PROGID, parent);
    if (!object || object->isNull()) {
        delete object;
        return nullptr;
    }
    return object;
}

bool ensureOmniRigComServerAvailable(QObject* parent)
{
    if (auto* test = createOmniRigObject(parent)) {
        delete test;
        return true;
    }

    const QStringList paths {
        QStringLiteral("C:/Program Files (x86)/Afreet/OmniRig/OmniRig.exe"),
        QStringLiteral("C:/Program Files/Afreet/OmniRig/OmniRig.exe")
    };
    for (const QString& path : paths) {
        if (!QFileInfo::exists(path))
            continue;

        QProcess::startDetached(path, {});
        for (int i = 0; i < 10; ++i) {
            QThread::msleep(500);
            if (auto* test = createOmniRigObject(parent)) {
                delete test;
                return true;
            }
        }
        break;
    }

    return false;
}
}

// Mappa param OmniRig → stringa modo
QString DecodiumOmniRigManager::modeFromParam(int param) const
{
    if (param & OMNI_PM_CW_U)  return "CW-R";
    if (param & OMNI_PM_CW_L)  return "CW";
    if (param & OMNI_PM_SSB_U) return "USB";
    if (param & OMNI_PM_SSB_L) return "LSB";
    if (param & OMNI_PM_DIG_U) return "DATA-U";
    if (param & OMNI_PM_DIG_L) return "DATA-L";
    if (param & OMNI_PM_AM)    return "AM";
    if (param & OMNI_PM_FM)    return "FM";
    return "USB";
}

int DecodiumOmniRigManager::modeToParam(const QString& mode) const
{
    const QString m = mode.trimmed().toUpper();
    if (m == QStringLiteral("CW-R")) return OMNI_PM_CW_U;
    if (m == QStringLiteral("CW")) return OMNI_PM_CW_L;
    if (m == QStringLiteral("USB")) return OMNI_PM_SSB_U;
    if (m == QStringLiteral("LSB")) return OMNI_PM_SSB_L;
    if (m == QStringLiteral("DATA-U") || m == QStringLiteral("DATA-USB")
            || m == QStringLiteral("DIGU") || m == QStringLiteral("PKT")
            || m == QStringLiteral("PKT-U") || m == QStringLiteral("FT8")
            || m == QStringLiteral("FT4") || m == QStringLiteral("FT2")) {
        return OMNI_PM_DIG_U;
    }
    if (m == QStringLiteral("DATA-L") || m == QStringLiteral("DATA-LSB")
            || m == QStringLiteral("DIGL") || m == QStringLiteral("PKT-L")) {
        return OMNI_PM_DIG_L;
    }
    if (m == QStringLiteral("AM")) return OMNI_PM_AM;
    if (m == QStringLiteral("FM") || m == QStringLiteral("DIG-FM")) return OMNI_PM_FM;
    return 0;
}

DecodiumOmniRigManager::DecodiumOmniRigManager(QObject* parent)
    : QObject(parent)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(m_pollInterval * 1000);
    connect(m_pollTimer, &QTimer::timeout, this, &DecodiumOmniRigManager::onPollTimer);
    loadSettings();
}

DecodiumOmniRigManager::~DecodiumOmniRigManager()
{
    disconnectRig();
}

void DecodiumOmniRigManager::connectRig()
{
    if (m_connected) return;

    m_mockEnabled = omniRigMockRequested();
    if (m_mockEnabled) {
        m_frequency = omniRigMockFrequency();
        m_txFrequency = m_frequency;
        m_mode = omniRigMockMode();
        m_readableParams = OMNI_PM_FREQ | OMNI_PM_FREQA | OMNI_PM_FREQB
                         | OMNI_PM_RX | OMNI_PM_TX
                         | OMNI_PM_CW_U | OMNI_PM_CW_L
                         | OMNI_PM_SSB_U | OMNI_PM_SSB_L
                         | OMNI_PM_DIG_U | OMNI_PM_DIG_L
                         | OMNI_PM_AM | OMNI_PM_FM;
        m_writableParams = m_readableParams;
        m_connected = true;
        emit connectedChanged();
        emit frequencyChanged();
        emit txFrequencyChanged();
        emit modeChanged();
        emit statusUpdate(QStringLiteral("OmniRig mock connesso: %1 Hz, %2")
                          .arg(QString::number(m_frequency, 'f', 0), m_mode));
        m_pollTimer->start();
        return;
    }

    emit statusUpdate("Connessione a OmniRig (" + m_rigName + ")…");

    if (!ensureOmniRigComServerAvailable(this)) {
        emit errorOccurred("OmniRig non trovato. Verifica che OmniRig.exe sia installato e configurato.");
        return;
    }

    m_omniRig = createOmniRigObject(this);
    if (!m_omniRig) {
        emit errorOccurred("Impossibile avviare il server COM di OmniRig.");
        return;
    }

    // Ottieni il riferimento al rig richiesto
    if (m_rigName == "OmniRig Rig 2")
        m_rig = m_omniRig->querySubObject("Rig2");
    else
        m_rig = m_omniRig->querySubObject("Rig1");

    if (!m_rig || m_rig->isNull()) {
        delete m_rig; m_rig = nullptr;
        delete m_omniRig; m_omniRig = nullptr;
        emit errorOccurred("OmniRig: impossibile ottenere l'interfaccia del rig.");
        return;
    }

    int status = m_rig->property("Status").toInt();
    for (int i = 0; i < 5 && status != OMNI_ST_ONLINE; ++i) {
        // Decodium3 lasciava tempo a OmniRig per completare il primo poll.
        QThread::msleep(300);
        status = m_rig->property("Status").toInt();
    }

    const QString statusName = omniRigStatusName(status);
    if (status != OMNI_ST_ONLINE) {
        emit errorOccurred("OmniRig: stato rig = " + statusName +
                           ". Verifica configurazione OmniRig, radio accesa e porta libera.");
        delete m_rig; m_rig = nullptr;
        delete m_omniRig; m_omniRig = nullptr;
        return;
    }

    m_readableParams = m_rig->property("ReadableParams").toInt();
    m_writableParams = m_rig->property("WriteableParams").toInt();

    const QString ptt = normalizePttMethod(m_pttMethod);
    if (ptt == "DTR" || ptt == "RTS") {
        m_portBits = m_rig->querySubObject("PortBits");
        if (!m_portBits || m_portBits->isNull()) {
            delete m_portBits; m_portBits = nullptr;
            emit errorOccurred("OmniRig: impossibile accedere a PortBits per PTT " + ptt + ".");
            delete m_rig; m_rig = nullptr;
            delete m_omniRig; m_omniRig = nullptr;
            return;
        }

        // Stato iniziale sicuro: non keyare la radio appena si connette.
        if (ptt == "DTR")
            m_portBits->dynamicCall("SetDtr(bool)", false);
        else
            m_portBits->dynamicCall("SetRts(bool)", false);
    }

    m_connected = true;
    emit connectedChanged();
    emit statusUpdate("OmniRig connesso: " + m_rigName + " — " + statusName);

    // Lettura immediata frequenza
    onPollTimer();
    m_pollTimer->start();
}

void DecodiumOmniRigManager::disconnectRig()
{
    m_pollTimer->stop();

    if (m_pttActive)
        setRigPtt(false);

    if (m_mockEnabled) {
        m_readableParams = 0;
        m_writableParams = 0;
        if (m_connected) {
            m_connected = false;
            emit connectedChanged();
            emit statusUpdate(QStringLiteral("OmniRig mock disconnesso."));
        }
        return;
    }

    delete m_portBits; m_portBits = nullptr;
    delete m_rig;    m_rig    = nullptr;
    delete m_omniRig; m_omniRig = nullptr;
    m_readableParams = 0;
    m_writableParams = 0;

    if (m_connected) {
        m_connected = false;
        emit connectedChanged();
        emit statusUpdate("OmniRig disconnesso.");
    }
}

void DecodiumOmniRigManager::onPollTimer()
{
    if (m_mockEnabled) {
        return;
    }

    if (!m_rig || m_rig->isNull()) return;

    // Frequenza RX
    double freq = m_rig->property("FreqA").toDouble();
    if (freq <= 0) freq = m_rig->property("Freq").toDouble();
    if (freq > 0 && m_frequency != freq) {
        m_frequency = freq;
        emit frequencyChanged();
    }

    // Frequenza TX (VFO B se split)
    double txFreq = m_rig->property("FreqB").toDouble();
    if (txFreq > 0 && m_txFrequency != txFreq) {
        m_txFrequency = txFreq;
        emit txFrequencyChanged();
    }

    // Modo
    int modeParam = m_rig->property("Mode").toInt();
    QString modeStr = modeFromParam(modeParam);
    if (m_mode != modeStr) {
        m_mode = modeStr;
        emit modeChanged();
    }

    // Status: se ST_NOTRESPONDING → disconnetti
    int status = m_rig->property("Status").toInt();
    if (status == OMNI_ST_NOTRESPONDING && m_connected) {
        emit errorOccurred("OmniRig: rig non risponde.");
        disconnectRig();
    }
}

void DecodiumOmniRigManager::setRigFrequency(double hz)
{
    if (m_mockEnabled) {
        if (!m_connected) {
            emit errorOccurred(QStringLiteral("OmniRig mock: rig non connesso"));
            return;
        }
        if (m_frequency != hz) {
            m_frequency = hz;
            emit frequencyChanged();
        }
        if (m_txFrequency != hz) {
            m_txFrequency = hz;
            emit txFrequencyChanged();
        }
        emit statusUpdate("OmniRig mock: freq -> " + QString::number(hz / 1e6, 'f', 6) + " MHz");
        return;
    }

    if (!m_rig || m_rig->isNull()) {
        emit errorOccurred("OmniRig: impossibile impostare frequenza — rig non connesso");
        return;
    }
    const int freq = static_cast<int>(qRound64(hz));
    if (m_writableParams & OMNI_PM_FREQ)
        m_rig->dynamicCall("SetFreq(int)", freq);
    else if (m_writableParams & OMNI_PM_FREQA)
        m_rig->dynamicCall("SetFreqA(int)", freq);
    else if (m_writableParams & OMNI_PM_FREQB)
        m_rig->dynamicCall("SetFreqB(int)", freq);
    else
        m_rig->setProperty("FreqA", hz);

    if (m_frequency != hz) {
        m_frequency = hz;
        emit frequencyChanged();
    }
    emit statusUpdate("OmniRig: freq → " + QString::number(hz / 1e6, 'f', 6) + " MHz");
}

void DecodiumOmniRigManager::setRigPtt(bool on)
{
    if (m_mockEnabled) {
        if (!m_connected) {
            emit errorOccurred(QStringLiteral("OmniRig mock: rig non connesso"));
            return;
        }
        emit statusUpdate(on ? QStringLiteral("OmniRig mock: PTT ON")
                             : QStringLiteral("OmniRig mock: PTT OFF"));
        if (m_pttActive != on) {
            m_pttActive = on;
            emit pttActiveChanged();
        }
        return;
    }

    if (!m_rig || m_rig->isNull()) {
        emit errorOccurred("OmniRig: impossibile PTT — rig non connesso");
        return;
    }
    const QString ptt = normalizePttMethod(m_pttMethod);
    if (ptt == "VOX") {
        if (m_pttActive != on) {
            m_pttActive = on;
            emit pttActiveChanged();
        }
        return;
    }

    if (ptt == "DTR" || ptt == "RTS") {
        if (!m_portBits || m_portBits->isNull()) {
            emit errorOccurred("OmniRig: PortBits non disponibile per PTT " + ptt);
            return;
        }
        if (ptt == "DTR")
            m_portBits->dynamicCall("SetDtr(bool)", on);
        else
            m_portBits->dynamicCall("SetRts(bool)", on);
    } else {
        m_rig->dynamicCall("SetTx(int)", on ? OMNI_PM_TX : OMNI_PM_RX);
    }

    emit statusUpdate(on ? "OmniRig: PTT ON" : "OmniRig: PTT OFF");
    if (m_pttActive != on) {
        m_pttActive = on;
        emit pttActiveChanged();
    }
}

void DecodiumOmniRigManager::setRigMode(const QString& mode)
{
    if (m_mockEnabled) {
        if (!m_connected) {
            emit errorOccurred(QStringLiteral("OmniRig mock: rig non connesso"));
            return;
        }
        const QString normalized = mode.trimmed().toUpper();
        if (!normalized.isEmpty() && m_mode != normalized) {
            m_mode = normalized;
            emit modeChanged();
        }
        emit statusUpdate(QStringLiteral("OmniRig mock: mode -> %1").arg(m_mode));
        return;
    }

    if (!m_rig || m_rig->isNull()) {
        emit errorOccurred("OmniRig: impossibile impostare modo — rig non connesso");
        return;
    }

    const int param = modeToParam(mode);
    if (!param)
        return;
    if (m_writableParams && !(m_writableParams & param)) {
        emit errorOccurred("OmniRig: modo non supportato dal file rig (" + mode + ")");
        return;
    }

    m_rig->dynamicCall("SetMode(int)", param);
    const QString modeStr = modeFromParam(param);
    if (m_mode != modeStr) {
        m_mode = modeStr;
        emit modeChanged();
    }
}

void DecodiumOmniRigManager::applyPollInterval()
{
    if (m_pollTimer)
        m_pollTimer->setInterval(m_pollInterval * 1000);
}

void DecodiumOmniRigManager::saveSettings()
{
    QSettings s("Decodium", "Decodium3");
    m_pttMethod = normalizePttMethod(m_pttMethod);
    m_pollInterval = boundedPollInterval(m_pollInterval);
    s.beginGroup("CAT_OmniRig");
    s.setValue("rigName",        m_rigName);
    s.setValue("pttMethod",      m_pttMethod);
    s.setValue("pollInterval",   m_pollInterval);
    s.setValue("catAutoConnect", m_catAutoConnect);
    s.setValue("audioAutoStart", m_audioAutoStart);
    s.endGroup();
}

void DecodiumOmniRigManager::loadSettings()
{
    QSettings s("Decodium", "Decodium3");
    s.beginGroup("CAT_OmniRig");
    const bool hasRigName = s.contains("rigName");
    const bool hasPttMethod = s.contains("pttMethod");
    const bool hasPollInterval = s.contains("pollInterval");
    m_rigName        = s.value("rigName",        "OmniRig Rig 1").toString();
    m_pttMethod      = normalizePttMethod(s.value("pttMethod", "CAT").toString());
    m_pollInterval   = boundedPollInterval(s.value("pollInterval", 2).toInt());
    m_catAutoConnect = s.value("catAutoConnect",  false).toBool();
    m_audioAutoStart = s.value("audioAutoStart",  false).toBool();
    s.endGroup();

    if (!hasRigName) {
        const QString legacyRig = s.value("Rig").toString();
        if (isOmniRigRigName(legacyRig))
            m_rigName = legacyRig;
    }
    if (!hasPttMethod && s.contains("PTTMethod"))
        m_pttMethod = pttMethodFromSettings(s.value("PTTMethod"), m_pttMethod);
    if (!hasPollInterval && s.contains("Polling"))
        m_pollInterval = boundedPollInterval(s.value("Polling", m_pollInterval).toInt());

    m_mockEnabled = omniRigMockRequested();
    applyPollInterval();
}
#else
QString DecodiumOmniRigManager::modeFromParam(int) const
{
    return "USB";
}

int DecodiumOmniRigManager::modeToParam(const QString&) const
{
    return 0;
}

DecodiumOmniRigManager::DecodiumOmniRigManager(QObject* parent)
    : QObject(parent)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(m_pollInterval * 1000);
    loadSettings();
}

DecodiumOmniRigManager::~DecodiumOmniRigManager()
{
    disconnectRig();
}

void DecodiumOmniRigManager::connectRig()
{
    m_mockEnabled = omniRigMockRequested();
    if (m_mockEnabled) {
        m_frequency = omniRigMockFrequency();
        m_txFrequency = m_frequency;
        m_mode = omniRigMockMode();
        m_connected = true;
        emit connectedChanged();
        emit frequencyChanged();
        emit txFrequencyChanged();
        emit modeChanged();
        emit statusUpdate(QStringLiteral("OmniRig mock connesso: %1 Hz, %2")
                          .arg(QString::number(m_frequency, 'f', 0), m_mode));
        if (m_pollTimer) {
            m_pollTimer->start();
        }
        return;
    }

    emit errorOccurred("OmniRig support is not available in this build.");
}

void DecodiumOmniRigManager::disconnectRig()
{
    if (m_pollTimer) m_pollTimer->stop();
    if (m_connected) {
        m_connected = false;
        emit connectedChanged();
    }
}

void DecodiumOmniRigManager::onPollTimer() {}

void DecodiumOmniRigManager::setRigFrequency(double hz)
{
    if (!m_mockEnabled || !m_connected) return;
    if (m_frequency != hz) {
        m_frequency = hz;
        emit frequencyChanged();
    }
    if (m_txFrequency != hz) {
        m_txFrequency = hz;
        emit txFrequencyChanged();
    }
    emit statusUpdate("OmniRig mock: freq -> " + QString::number(hz / 1e6, 'f', 6) + " MHz");
}

void DecodiumOmniRigManager::setRigMode(const QString& mode)
{
    if (!m_mockEnabled || !m_connected) return;
    const QString normalized = mode.trimmed().toUpper();
    if (!normalized.isEmpty() && m_mode != normalized) {
        m_mode = normalized;
        emit modeChanged();
    }
    emit statusUpdate(QStringLiteral("OmniRig mock: mode -> %1").arg(m_mode));
}

void DecodiumOmniRigManager::setRigPtt(bool on)
{
    if (!m_mockEnabled || !m_connected) return;
    if (m_pttActive != on) {
        m_pttActive = on;
        emit pttActiveChanged();
    }
}

void DecodiumOmniRigManager::applyPollInterval()
{
    if (m_pollTimer)
        m_pollTimer->setInterval(m_pollInterval * 1000);
}

void DecodiumOmniRigManager::saveSettings()
{
    QSettings s("Decodium", "Decodium3");
    m_pttMethod = normalizePttMethod(m_pttMethod);
    m_pollInterval = boundedPollInterval(m_pollInterval);
    s.beginGroup("CAT_OmniRig");
    s.setValue("rigName",        m_rigName);
    s.setValue("pttMethod",      m_pttMethod);
    s.setValue("pollInterval",   m_pollInterval);
    s.setValue("catAutoConnect", m_catAutoConnect);
    s.setValue("audioAutoStart", m_audioAutoStart);
    s.endGroup();
}

void DecodiumOmniRigManager::loadSettings()
{
    QSettings s("Decodium", "Decodium3");
    s.beginGroup("CAT_OmniRig");
    const bool hasRigName = s.contains("rigName");
    const bool hasPttMethod = s.contains("pttMethod");
    const bool hasPollInterval = s.contains("pollInterval");
    m_rigName        = s.value("rigName",        "OmniRig Rig 1").toString();
    m_pttMethod      = normalizePttMethod(s.value("pttMethod", "CAT").toString());
    m_pollInterval   = boundedPollInterval(s.value("pollInterval", 2).toInt());
    m_catAutoConnect = s.value("catAutoConnect",  false).toBool();
    m_audioAutoStart = s.value("audioAutoStart",  false).toBool();
    s.endGroup();

    if (!hasRigName) {
        const QString legacyRig = s.value("Rig").toString();
        if (isOmniRigRigName(legacyRig))
            m_rigName = legacyRig;
    }
    if (!hasPttMethod && s.contains("PTTMethod"))
        m_pttMethod = pttMethodFromSettings(s.value("PTTMethod"), m_pttMethod);
    if (!hasPollInterval && s.contains("Polling"))
        m_pollInterval = boundedPollInterval(s.value("Polling", m_pollInterval).toInt());

    m_mockEnabled = omniRigMockRequested();
    applyPollInterval();
}
#endif
