#include "DecodiumAlertManager.h"
#include <QApplication>
#include <QDebug>
#include <QOperatingSystemVersion>
#include <QSet>
#include <QStandardPaths>
#include <QtGlobal>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static QSoundEffect* makeEffect(const QString& path, float volume, QObject* parent)
{
    auto* fx = new QSoundEffect(parent);
    fx->setVolume(static_cast<qreal>(volume));
    if (!path.isEmpty())
        fx->setSource(QUrl::fromLocalFile(path));
    QObject::connect(fx, &QSoundEffect::statusChanged, fx, [fx, path] {
        if (fx->status() == QSoundEffect::Error) {
            qWarning().noquote() << "[AlertAudio] failed to load"
                                 << (path.isEmpty() ? QStringLiteral("<empty>") : path);
        }
    });
    return fx;
}

static bool useNativeAlertBeep()
{
#if defined(Q_OS_MAC)
    if (qEnvironmentVariableIsSet("DECODIUM_QT_ALERT_AUDIO")) {
        return false;
    }
    return QOperatingSystemVersion::current()
        >= QOperatingSystemVersion(QOperatingSystemVersion::MacOS, 15);
#else
    return false;
#endif
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

DecodiumAlertManager::DecodiumAlertManager(QObject* parent)
    : QObject(parent)
{
    if (useNativeAlertBeep()) {
        qInfo().noquote() << "[AlertAudio] using native macOS beep; set DECODIUM_QT_ALERT_AUDIO=1 to force QSoundEffect";
        return;
    }

    m_cqSound      = makeEffect(findSoundFile({QStringLiteral("CQ.wav"), QStringLiteral("cq.wav")}), m_volume, this);
    m_myCallSound  = makeEffect(findSoundFile({QStringLiteral("MyCall.wav"), QStringLiteral("mycall.wav")}), m_volume, this);
    m_dxSound      = makeEffect(findSoundFile({QStringLiteral("DXcall.wav"), QStringLiteral("DX.wav"), QStringLiteral("dx.wav")}), m_volume, this);
    m_sound73      = makeEffect(findSoundFile({QStringLiteral("Message.wav"), QStringLiteral("73.wav")}), m_volume, this);
    m_newDxccSound = makeEffect(findSoundFile({QStringLiteral("DXCC.wav"), QStringLiteral("NewDXCC.wav"), QStringLiteral("newdxcc.wav")}), m_volume, this);
    m_errorSound   = makeEffect(findSoundFile({QStringLiteral("_Testing.wav"), QStringLiteral("Message.wav"), QStringLiteral("Error.wav"), QStringLiteral("error.wav")}), m_volume, this);
}

// ---------------------------------------------------------------------------
// findSoundFile — cerca <name>.wav nelle cartelle candidate
// ---------------------------------------------------------------------------

QString DecodiumAlertManager::findSoundFile(const QString& name) const
{
    return findSoundFile(QStringList{name + QStringLiteral(".wav"), name});
}

QString DecodiumAlertManager::findSoundFile(const QStringList& names) const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString resourceDir = QDir(appDir).absoluteFilePath(QStringLiteral("../Resources/sounds"));
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/sounds");

    QStringList dirs = {
        QDir(appDir).absoluteFilePath(QStringLiteral("sounds")),
        resourceDir,
        dataDir,
        QDir::homePath() + QStringLiteral("/.decodium/sounds"),
        QDir::current().absoluteFilePath(QStringLiteral("sounds"))
    };
#ifdef CMAKE_SOURCE_DIR
    dirs << QDir(QStringLiteral(CMAKE_SOURCE_DIR)).absoluteFilePath(QStringLiteral("sounds"));
#endif

    QSet<QString> seenDirs;
    for (const QString& rawDir : dirs) {
        const QString dir = QDir::cleanPath(rawDir);
        if (dir.isEmpty() || seenDirs.contains(dir))
            continue;
        seenDirs.insert(dir);

        QDir soundDir(dir);
        if (!soundDir.exists())
            continue;

        for (const QString& name : names) {
            const QString path = soundDir.absoluteFilePath(name);
            if (QFileInfo::exists(path))
                return path;
        }

        const QFileInfoList wavFiles = soundDir.entryInfoList({QStringLiteral("*.wav")},
                                                              QDir::Files | QDir::Readable);
        for (const QString& name : names) {
            for (const QFileInfo& wav : wavFiles) {
                if (wav.fileName().compare(name, Qt::CaseInsensitive) == 0)
                    return wav.absoluteFilePath();
            }
        }
    }
    qWarning().noquote() << "[AlertAudio] sound file not found for"
                         << names.join(QStringLiteral(", "));
    return {};
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

    if (useNativeAlertBeep()) {
        Q_UNUSED(type)
        QApplication::beep();
        return;
    }

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
