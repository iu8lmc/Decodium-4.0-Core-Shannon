#include "DecodiumAlertManager.h"
#include <QApplication>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static QSoundEffect* makeEffect(const QString& path, float volume, QObject* parent)
{
    auto* fx = new QSoundEffect(parent);
    fx->setVolume(static_cast<qreal>(volume));
    if (!path.isEmpty())
        fx->setSource(QUrl::fromLocalFile(path));
    return fx;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

DecodiumAlertManager::DecodiumAlertManager(QObject* parent)
    : QObject(parent)
{
    m_cqSound      = makeEffect(findSoundFile("cq"),      m_volume, this);
    m_myCallSound  = makeEffect(findSoundFile("mycall"),  m_volume, this);
    m_dxSound      = makeEffect(findSoundFile("dx"),      m_volume, this);
    m_sound73      = makeEffect(findSoundFile("73"),      m_volume, this);
    m_newDxccSound = makeEffect(findSoundFile("newdxcc"), m_volume, this);
    m_errorSound   = makeEffect(findSoundFile("error"),   m_volume, this);
}

// ---------------------------------------------------------------------------
// findSoundFile — cerca <name>.wav nelle cartelle candidate
// ---------------------------------------------------------------------------

QString DecodiumAlertManager::findSoundFile(const QString& name) const
{
    // Candidate directories in priority order
    const QStringList dirs = {
        QCoreApplication::applicationDirPath() + "/sounds",
        QDir::homePath() + "/.decodium/sounds"
    };

    for (const QString& dir : dirs) {
        QString path = dir + "/" + name + ".wav";
        if (QFileInfo::exists(path))
            return path;
    }
    return {};   // not found — QSoundEffect will stay silent
}

// ---------------------------------------------------------------------------
// soundForType — seleziona il QSoundEffect corretto
// ---------------------------------------------------------------------------

QSoundEffect* DecodiumAlertManager::soundForType(const QString& type)
{
    if (type == "CQ")      return m_cqSound;
    if (type == "MyCall")  return m_myCallSound;
    if (type == "DX")      return m_dxSound;
    if (type == "73")      return m_sound73;
    if (type == "NewDXCC") return m_newDxccSound;
    if (type == "Error")   return m_errorSound;
    return nullptr;
}

// ---------------------------------------------------------------------------
// playAlert
// ---------------------------------------------------------------------------

void DecodiumAlertManager::playAlert(const QString& type)
{
    if (!m_enabled)
        return;

    QSoundEffect* fx = soundForType(type);
    if (fx && fx->isLoaded() && !fx->source().isEmpty()) {
        fx->play();
    } else {
        // Fallback: beep di sistema
        QApplication::beep();
    }
}

// ---------------------------------------------------------------------------
// setVolume
// ---------------------------------------------------------------------------

void DecodiumAlertManager::setVolume(float v)
{
    m_volume = v;
    for (QSoundEffect* fx : { m_cqSound, m_myCallSound, m_dxSound,
                               m_sound73, m_newDxccSound, m_errorSound }) {
        if (fx) fx->setVolume(static_cast<qreal>(v));
    }
}
