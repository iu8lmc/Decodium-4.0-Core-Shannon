#include "DecodiumOmniRigManager.h"

#include <QAxObject>
#include <QSettings>
#include <QTimer>
#include <QDebug>

// OmniRig COM ProgID
static const char* OMNIRIG_PROGID = "OmniRig.OmniRigX";

// Mappa param OmniRig → stringa modo
QString DecodiumOmniRigManager::modeFromParam(int param) const
{
    if (param & 0x0800000)  return "CW-R";    // PM_CW_U
    if (param & 0x1000000)  return "CW";      // PM_CW_L
    if (param & 0x2000000)  return "USB";     // PM_SSB_U
    if (param & 0x4000000)  return "LSB";     // PM_SSB_L
    if (param & 0x8000000)  return "DATA-U";  // PM_DIG_U
    if (param & 0x10000000) return "DATA-L";  // PM_DIG_L
    if (param & 0x20000000) return "AM";      // PM_AM
    if (param & 0x40000000) return "FM";      // PM_FM
    return "USB";
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

    emit statusUpdate("Connessione a OmniRig (" + m_rigName + ")…");

    // Crea il COM object OmniRig
    m_omniRig = new QAxObject(OMNIRIG_PROGID, this);
    if (!m_omniRig || m_omniRig->isNull()) {
        delete m_omniRig; m_omniRig = nullptr;
        emit errorOccurred("OmniRig non trovato. Verifica che OmniRig.exe sia installato e configurato.");
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

    // Verifica stato
    int status = m_rig->property("Status").toInt();
    // ST_ONLINE = 4, ST_NOTCONFIGURED = 0, ST_DISABLED = 1, ST_PORTBUSY = 2, ST_NOTRESPONDING = 3
    if (status == 0) {
        emit errorOccurred("OmniRig: rig non configurato. Configura il rig in OmniRig.exe.");
        delete m_rig; m_rig = nullptr;
        delete m_omniRig; m_omniRig = nullptr;
        return;
    }

    m_connected = true;
    emit connectedChanged();
    emit statusUpdate("OmniRig connesso: " + m_rigName +
                      " (stato: " + QString::number(status) + ")");

    // Lettura immediata frequenza
    onPollTimer();
    m_pollTimer->start();
}

void DecodiumOmniRigManager::disconnectRig()
{
    m_pollTimer->stop();

    if (m_pttActive) {
        if (m_rig && !m_rig->isNull())
            m_rig->setProperty("Ptt", 0);
        m_pttActive = false;
        emit pttActiveChanged();
    }

    delete m_rig;    m_rig    = nullptr;
    delete m_omniRig; m_omniRig = nullptr;

    if (m_connected) {
        m_connected = false;
        emit connectedChanged();
        emit statusUpdate("OmniRig disconnesso.");
    }
}

void DecodiumOmniRigManager::onPollTimer()
{
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

    // Status: se ST_NOTRESPONDING (3) → disconnetti
    int status = m_rig->property("Status").toInt();
    if (status == 3 && m_connected) {
        emit errorOccurred("OmniRig: rig non risponde.");
        disconnectRig();
    }
}

void DecodiumOmniRigManager::setRigFrequency(double hz)
{
    if (!m_rig || m_rig->isNull()) return;
    m_rig->setProperty("FreqA", hz);
}

void DecodiumOmniRigManager::setRigPtt(bool on)
{
    if (!m_rig || m_rig->isNull()) return;
    // OmniRig: Ptt property  0=RX  1=TX
    m_rig->setProperty("Ptt", on ? 1 : 0);
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
    m_rigName        = s.value("rigName",        "OmniRig Rig 1").toString();
    m_pttMethod      = s.value("pttMethod",       "CAT").toString();
    m_pollInterval   = s.value("pollInterval",    2).toInt();
    m_catAutoConnect = s.value("catAutoConnect",  false).toBool();
    m_audioAutoStart = s.value("audioAutoStart",  false).toBool();
    s.endGroup();
}
