#include "DecodiumUpdater.hpp"
#include "DecodiumUpdateSupport.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>

#ifndef FORK_RELEASE_VERSION
#define FORK_RELEASE_VERSION "0.0.0"
#endif

namespace {

// Un'unica linea di release con due punti di pubblicazione. Il repository di
// elisir80 e' primario; iu8lmc e' il fallback consultato soltanto quando il
// primario non offre una versione piu' nuova o non restituisce una release
// valida. Asset, note e pagina web restano quelli della release selezionata.
constexpr auto kPrimaryRepository = "elisir80/Decodium-4.0-Core-Shannon";
constexpr auto kPrimaryReleasesApi =
    "https://api.github.com/repos/elisir80/Decodium-4.0-Core-Shannon/releases/latest";
constexpr auto kPrimaryReleasesPage =
    "https://github.com/elisir80/Decodium-4.0-Core-Shannon/releases/latest";
constexpr auto kSecondaryRepository = "iu8lmc/Decodium-4.0-Core-Shannon";
constexpr auto kSecondaryReleasesApi =
    "https://api.github.com/repos/iu8lmc/Decodium-4.0-Core-Shannon/releases/latest";
constexpr auto kSecondaryReleasesPage =
    "https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/latest";

// Chiavi nello store impostazioni (dalla 1.0.482 e' un INI, non piu' il registro).
constexpr auto kKeyCheckOnStartup = "Update/CheckOnStartup";
constexpr auto kKeyLastCheckUtc   = "Update/LastCheckUtc";
constexpr auto kKeySkippedVersion = "Update/SkippedVersion";

QSettings settingsStore()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("Decodium"), QStringLiteral("Decodium3"));
}

const char* releasesApi(bool secondary)
{
    return secondary ? kSecondaryReleasesApi : kPrimaryReleasesApi;
}

const char* releasesPage(bool secondary)
{
    return secondary ? kSecondaryReleasesPage : kPrimaryReleasesPage;
}

const char* repositoryName(bool secondary)
{
    return secondary ? kSecondaryRepository : kPrimaryRepository;
}

QString currentPlatformKey()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
}

QFileDevice::Permissions executableAppImagePermissions()
{
    return QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
           | QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser
           | QFileDevice::ReadGroup | QFileDevice::ExeGroup
           | QFileDevice::ReadOther | QFileDevice::ExeOther;
}

}  // namespace

DecodiumUpdater::DecodiumUpdater(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_currentVersion(QStringLiteral(FORK_RELEASE_VERSION))
    , m_releaseRepository(QString::fromLatin1(kPrimaryRepository))
    , m_releasePageUrl(QString::fromLatin1(kPrimaryReleasesPage))
{
    m_checkOnStartup = settingsStore().value(kKeyCheckOnStartup, true).toBool();
    if (currentPlatformKey() == QLatin1String("linux")) {
        const QString appImagePath = qEnvironmentVariable("APPIMAGE").trimmed();
        m_appImageRuntime = !appImagePath.isEmpty() && QFileInfo(appImagePath).isFile();
    }
}

void DecodiumUpdater::setBusy(bool b)
{
    if (m_busy == b)
        return;
    m_busy = b;
    emit busyChanged();
}

void DecodiumUpdater::setProgress(int p)
{
    if (m_progress == p)
        return;
    m_progress = p;
    emit progressChanged();
}

void DecodiumUpdater::setStatus(const QString& s)
{
    if (m_statusText == s)
        return;
    m_statusText = s;
    emit statusTextChanged();
}

void DecodiumUpdater::setOfflineMode(bool offline)
{
    if (m_offlineMode == offline) {
        return;
    }
    m_offlineMode = offline;
    if (offline) {
        for (QNetworkReply* reply : m_nam->findChildren<QNetworkReply*>()) {
            if (reply) reply->abort();
        }
        setBusy(false);
        setProgress(0);
        setStatus(tr("Offline: update checks disabled"));
    } else {
        setStatus(tr("Online: update checks enabled"));
    }
    emit offlineModeChanged();
}

void DecodiumUpdater::setCheckOnStartup(bool on)
{
    if (m_checkOnStartup == on)
        return;
    m_checkOnStartup = on;
    QSettings s = settingsStore();
    s.setValue(kKeyCheckOnStartup, on);
    s.sync();
    emit checkOnStartupChanged();
}

void DecodiumUpdater::checkOnStartupIfDue()
{
    if (m_offlineMode || !m_checkOnStartup)
        return;
    // Non tempestare GitHub (e l'utente) a ogni avvio: al massimo una volta al
    // giorno. Chi vuole puo' sempre forzare il controllo dal menu.
    const QDateTime last = settingsStore().value(kKeyLastCheckUtc).toDateTime();
    if (last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < 24 * 3600)
        return;
    check(/*silent=*/true);
}

void DecodiumUpdater::check(bool silent)
{
    if (m_offlineMode) {
        setStatus(tr("Offline: update checks disabled"));
        return;
    }
    if (m_busy)
        return;
    setBusy(true);
    setProgress(-1);
    setStatus(tr("Checking for updates..."));

    m_bestCheckedVersion.clear();
    m_anyRepositoryCheckedSuccessfully = false;
    requestReleaseCheck(silent, /*secondary=*/false);
}

void DecodiumUpdater::requestReleaseCheck(bool silent, bool secondary)
{
    qInfo().noquote()
        << "[UPDATE] Checking GitHub releases"
        << "repository=" << repositoryName(secondary)
        << "role=" << (secondary ? "secondary" : "primary");

    QNetworkRequest req{QUrl(QString::fromLatin1(releasesApi(secondary)))};
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", "Decodium/4.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, silent, secondary]() {
        onCheckFinished(reply, silent, secondary);
    });
}

void DecodiumUpdater::recordCheckCompleted()
{
    QSettings settings = settingsStore();
    settings.setValue(kKeyLastCheckUtc, QDateTime::currentDateTimeUtc());
    settings.sync();
    setBusy(false);
    setProgress(0);
}

void DecodiumUpdater::finishCheckWithoutUpdate(bool silent)
{
    recordCheckCompleted();
    m_available = false;
    m_latestVersion = m_bestCheckedVersion.isEmpty()
        ? m_currentVersion
        : m_bestCheckedVersion;
    m_releaseNotes.clear();
    m_downloadUrl.clear();
    m_assetName.clear();
    m_releaseRepository = QString::fromLatin1(kPrimaryRepository);
    m_releasePageUrl = QString::fromLatin1(kPrimaryReleasesPage);
    emit stateChanged();
    setStatus(tr("Decodium is up to date (%1).").arg(m_currentVersion));
    if (!silent)
        emit upToDate(m_currentVersion);
}

void DecodiumUpdater::onCheckFinished(QNetworkReply* reply, bool silent, bool secondary)
{
    reply->deleteLater();
    if (m_offlineMode) {
        setBusy(false);
        setProgress(0);
        setStatus(tr("Offline: update checks disabled"));
        return;
    }

    QString failure;
    QJsonObject root;
    QString version;
    if (reply->error() != QNetworkReply::NoError) {
        failure = reply->errorString();
    } else {
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject()) {
            root = doc.object();
            QString tag = root.value(QStringLiteral("tag_name")).toString();
            if (!tag.isEmpty()) {
                version = tag;
                if (version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
                    version.remove(0, 1);
            }
        }
        if (version.isEmpty())
            failure = tr("Could not read the release information.");
    }

    if (!version.isEmpty()) {
        m_anyRepositoryCheckedSuccessfully = true;
        if (m_bestCheckedVersion.isEmpty()
            || decodium::update::isVersionNewer(version, m_bestCheckedVersion)) {
            m_bestCheckedVersion = version;
        }
    } else {
        qWarning().noquote()
            << "[UPDATE] GitHub release check failed"
            << "repository=" << repositoryName(secondary)
            << "error=" << failure;
    }

    const auto decision = decodium::update::releaseCheckDecision(
        version, m_currentVersion, !secondary);
    if (decision == decodium::update::ReleaseCheckDecision::CheckFallback) {
        qInfo().noquote()
            << "[UPDATE] Primary has no newer release; checking fallback"
            << "primary_version=" << (version.isEmpty() ? QStringLiteral("unavailable") : version)
            << "current_version=" << m_currentVersion;
        requestReleaseCheck(silent, /*secondary=*/true);
        return;
    }

    if (decision == decodium::update::ReleaseCheckDecision::NoUpdate) {
        if (m_anyRepositoryCheckedSuccessfully) {
            finishCheckWithoutUpdate(silent);
        } else {
            recordCheckCompleted();
            setStatus(tr("Could not check for updates: %1").arg(failure));
            // All'avvio un problema di rete non e' affar dell'utente: e' rumore.
            if (!silent)
                emit errorOccurred(m_statusText);
        }
        return;
    }

    m_latestVersion = version;
    m_releaseRepository = QString::fromLatin1(repositoryName(secondary));
    m_releaseNotes  = root.value(QStringLiteral("body")).toString();
    m_releasePageUrl = QString::fromLatin1(releasesPage(secondary));

    m_downloadUrl.clear();
    m_assetName.clear();
    const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    const QString platform = currentPlatformKey();
    const QString architecture = QSysInfo::currentCpuArchitecture();
    int bestAssetScore = 0;
    for (const QJsonValue& v : assets) {
        const QJsonObject a = v.toObject();
        const QString name = a.value(QStringLiteral("name")).toString();
        const int score = decodium::update::assetMatchScore(
            name, platform, architecture);
        if (score > bestAssetScore) {
            bestAssetScore = score;
            m_assetName   = name;
            m_downloadUrl = a.value(QStringLiteral("browser_download_url")).toString();
        }
    }

    qInfo().noquote()
        << "[UPDATE] Selected GitHub release"
        << "repository=" << m_releaseRepository
        << "version=" << m_latestVersion
        << "asset=" << (m_assetName.isEmpty() ? QStringLiteral("release-page") : m_assetName);

    const QString skipped = settingsStore().value(kKeySkippedVersion).toString();
    m_available = true;
    recordCheckCompleted();
    emit stateChanged();

    setStatus(tr("Version %1 is available.").arg(m_latestVersion));

    // "Salta questa versione" vale solo per quella versione: se ne esce una
    // nuova, l'utente va avvisato lo stesso.
    if (silent && skipped == m_latestVersion)
        return;

    emit updateFound(m_latestVersion);
}

void DecodiumUpdater::skipThisVersion()
{
    if (m_latestVersion.isEmpty())
        return;
    QSettings s = settingsStore();
    s.setValue(kKeySkippedVersion, m_latestVersion);
    s.sync();
    setStatus(tr("Version %1 will be skipped.").arg(m_latestVersion));
}

void DecodiumUpdater::downloadAndInstall()
{
    if (m_offlineMode) {
        setStatus(tr("Offline: downloads disabled"));
        return;
    }
    if (m_busy)
        return;

    // Nessun asset per questa piattaforma: non lasciamo l'utente a mani vuote,
    // apriamo la pagina della release.
    if (m_downloadUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(m_releasePageUrl));
        return;
    }

    QString target;
    QFileDevice* output = nullptr;
    QSaveFile* atomicFile = nullptr;
    QFile* regularFile = nullptr;
    bool replaceRunningAppImage = false;
    bool appImageSavedToDownloads = false;
    QFileDevice::Permissions targetPermissions;

    if (currentPlatformKey() == QLatin1String("linux")) {
        // APPIMAGE is the canonical path supplied by the AppImage runtime.  A
        // QSaveFile writes a sibling temporary file and commits it with one rename,
        // so a failed/interrupted download can never destroy the working image.
        const QString runningPath = qEnvironmentVariable("APPIMAGE").trimmed();
        const QFileInfo runningInfo(runningPath);
        if (!runningPath.isEmpty() && runningInfo.isFile()) {
            target = runningInfo.absoluteFilePath();
            targetPermissions = runningInfo.permissions() | executableAppImagePermissions();
            auto* candidate = new QSaveFile(target);
            candidate->setDirectWriteFallback(false);
            if (candidate->open(QIODevice::WriteOnly)) {
                atomicFile = candidate;
                output = candidate;
                replaceRunningAppImage = true;
            } else {
                delete candidate;
            }
        }

        // A read-only installation directory is common for system-managed files.
        // Never fall back to /tmp: the package would disappear on reboot and look
        // as though the update had succeeded.  Put it in Downloads instead.
        if (!output) {
            QString dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
            if (dir.isEmpty())
                dir = QDir::home().absoluteFilePath(QStringLiteral("Downloads"));
            if (!QDir().mkpath(dir)) {
                setStatus(tr("Cannot create the Downloads folder."));
                emit errorOccurred(m_statusText);
                return;
            }
            target = QDir(dir).absoluteFilePath(m_assetName);
            targetPermissions = executableAppImagePermissions();
            auto* candidate = new QSaveFile(target);
            candidate->setDirectWriteFallback(false);
            if (!candidate->open(QIODevice::WriteOnly)) {
                delete candidate;
                setStatus(tr("Cannot write to the Downloads folder."));
                emit errorOccurred(m_statusText);
                return;
            }
            atomicFile = candidate;
            output = candidate;
            appImageSavedToDownloads = true;
        }
    } else {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QDir().mkpath(dir);
        target = QDir(dir).absoluteFilePath(m_assetName);
        QFile::remove(target);  // un download interrotto in precedenza sarebbe corrotto

        regularFile = new QFile(target);
        if (!regularFile->open(QIODevice::WriteOnly)) {
            delete regularFile;
            setStatus(tr("Cannot write to the temporary folder."));
            emit errorOccurred(m_statusText);
            return;
        }
        output = regularFile;
    }

    setBusy(true);
    setProgress(0);
    setStatus(tr("Downloading %1...").arg(m_latestVersion));

    QNetworkRequest req{QUrl(m_downloadUrl)};
    req.setRawHeader("User-Agent", "Decodium/4.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_nam->get(req);

    connect(reply, &QNetworkReply::readyRead, this, [reply, output]() {
        output->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 got, qint64 total) {
                setProgress(total > 0 ? int((got * 100) / total) : -1);
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, output, atomicFile, regularFile, target,
             targetPermissions, replaceRunningAppImage, appImageSavedToDownloads]() {
        reply->deleteLater();
        output->write(reply->readAll());
        const qint64 size = output->size();

        const auto discardDownload = [atomicFile, regularFile, target]() {
            if (atomicFile) {
                atomicFile->cancelWriting();
                atomicFile->deleteLater();
            } else if (regularFile) {
                regularFile->close();
                regularFile->deleteLater();
                QFile::remove(target);
            }
        };

        if (reply->error() != QNetworkReply::NoError) {
            discardDownload();
            setBusy(false);
            setStatus(tr("Download failed: %1").arg(reply->errorString()));
            emit errorOccurred(m_statusText);
            return;
        }
        // Un file troncato manderebbe in errore l'installer con un messaggio
        // incomprensibile: meglio accorgersene qui.
        if (size < 1024 * 1024) {
            discardDownload();
            setBusy(false);
            setStatus(tr("The downloaded file is incomplete. Please try again."));
            emit errorOccurred(m_statusText);
            return;
        }

        if (atomicFile) {
            QString installError;
            if (!decodium::update::commitAtomicFile(
                    *atomicFile, targetPermissions, &installError)) {
                atomicFile->deleteLater();
                setBusy(false);
                setStatus(tr("The AppImage could not be installed safely: %1")
                              .arg(installError));
                emit errorOccurred(m_statusText);
                return;
            }
            atomicFile->deleteLater();
        } else if (regularFile) {
            regularFile->flush();
            regularFile->close();
            regularFile->deleteLater();
        }

        setBusy(false);

        if (replaceRunningAppImage) {
            setStatus(tr("The AppImage was updated successfully. Restarting Decodium..."));
            QStringList arguments = QCoreApplication::arguments();
            if (!arguments.isEmpty())
                arguments.removeFirst();
            if (!QProcess::startDetached(target, arguments,
                                         QFileInfo(target).absolutePath())) {
                setStatus(tr("The AppImage was updated at %1. Please restart Decodium manually.")
                              .arg(QDir::toNativeSeparators(target)));
                QDesktopServices::openUrl(
                    QUrl::fromLocalFile(QFileInfo(target).absolutePath()));
                return;
            }
            QCoreApplication::quit();
            return;
        }

        if (appImageSavedToDownloads) {
            setStatus(tr("The AppImage was saved to %1. Launch it manually to complete the update.")
                          .arg(QDir::toNativeSeparators(target)));
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(QFileInfo(target).absolutePath()));
            return;
        }

        launchInstaller(target);
    });
}

void DecodiumUpdater::launchInstaller(const QString& path)
{
#if defined(Q_OS_WIN)
    setStatus(tr("Starting the installer..."));
    // Decodium DEVE uscire: l'installer disinstalla la versione precedente e non
    // puo' rimpiazzare un eseguibile in uso. Lo avviamo staccato e chiudiamo.
    if (!QProcess::startDetached(path, QStringList{})) {
        setStatus(tr("Could not start the installer."));
        emit errorOccurred(m_statusText);
        return;
    }
    QCoreApplication::quit();
#else
    // Fuori da Windows non c'e' un installer da lanciare: mostriamo il file.
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}
