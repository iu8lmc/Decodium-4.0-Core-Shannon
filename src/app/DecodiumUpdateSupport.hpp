#ifndef DECODIUMUPDATESUPPORT_HPP
#define DECODIUMUPDATESUPPORT_HPP

#include <QString>
#include <QStringList>
#include <QSaveFile>
#include <QList>

namespace decodium {
namespace update {

enum class ReleaseCheckDecision
{
    UseUpdate,
    CheckFallback,
    NoUpdate
};

inline QList<int> versionParts(const QString& version)
{
    QList<int> parts;
    const auto tokens = version.split(QLatin1Char('.'));
    for (const QString& token : tokens)
        parts << token.toInt();
    while (parts.size() < 3)
        parts << 0;
    return parts;
}

inline bool isVersionNewer(const QString& candidate, const QString& current)
{
    if (candidate.trimmed().isEmpty())
        return false;

    const QList<int> candidateParts = versionParts(candidate);
    const QList<int> currentParts = versionParts(current);
    for (int i = 0; i < 3; ++i) {
        if (candidateParts[i] != currentParts[i])
            return candidateParts[i] > currentParts[i];
    }
    return false;
}

inline ReleaseCheckDecision releaseCheckDecision(const QString& candidate,
                                                  const QString& current,
                                                  bool fallbackAvailable)
{
    if (isVersionNewer(candidate, current))
        return ReleaseCheckDecision::UseUpdate;
    return fallbackAvailable
        ? ReleaseCheckDecision::CheckFallback
        : ReleaseCheckDecision::NoUpdate;
}

inline QString normalizedArchitecture(QString architecture)
{
    architecture = architecture.trimmed().toLower();
    if (architecture == QLatin1String("amd64")
        || architecture == QLatin1String("x64")
        || architecture == QLatin1String("x86-64")) {
        return QStringLiteral("x86_64");
    }
    if (architecture == QLatin1String("arm64")
        || architecture == QLatin1String("arm64-v8a")) {
        return QStringLiteral("aarch64");
    }
    return architecture;
}

// Higher is better. Zero means that the package must not be used on the
// requested platform/architecture. Architecture-specific packages always win
// over generic packages, independently of the order returned by GitHub.
inline int assetMatchScore(const QString& assetName,
                           const QString& platform,
                           const QString& architecture)
{
    const QString name = assetName.trimmed().toLower();
    const QString os = platform.trimmed().toLower();
    const QString arch = normalizedArchitecture(architecture);

    if (os == QLatin1String("windows")) {
        return name.endsWith(QLatin1String(".exe"))
                   && name.contains(QLatin1String("setup")) ? 100 : 0;
    }

    const bool x86Asset = name.contains(QLatin1String("x86_64"))
                          || name.contains(QLatin1String("amd64"));
    const bool arm64Asset = name.contains(QLatin1String("aarch64"))
                            || name.contains(QLatin1String("arm64"));

    if (os == QLatin1String("macos")) {
        if (!name.endsWith(QLatin1String(".dmg")))
            return 0;
    } else if (os == QLatin1String("linux")) {
        if (!name.endsWith(QLatin1String(".appimage")))
            return 0;
    } else {
        return 0;
    }

    if (arch == QLatin1String("x86_64")) {
        if (arm64Asset)
            return 0;
        return x86Asset ? 120 : 20;
    }
    if (arch == QLatin1String("aarch64")) {
        if (x86Asset)
            return 0;
        return arm64Asset ? 120 : 20;
    }

    // On an unknown architecture only an explicitly generic package is safe.
    return (x86Asset || arm64Asset) ? 0 : 20;
}

inline int bestAssetIndex(const QStringList& assetNames,
                          const QString& platform,
                          const QString& architecture)
{
    int bestIndex = -1;
    int bestScore = 0;
    for (int i = 0; i < assetNames.size(); ++i) {
        const int score = assetMatchScore(assetNames.at(i), platform, architecture);
        if (score > bestScore) {
            bestIndex = i;
            bestScore = score;
        }
    }
    return bestIndex;
}

inline bool commitAtomicFile(QSaveFile& file,
                             QFileDevice::Permissions permissions,
                             QString* errorMessage = nullptr)
{
    if (!file.setPermissions(permissions)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    return true;
}

} // namespace update
} // namespace decodium

#endif // DECODIUMUPDATESUPPORT_HPP
