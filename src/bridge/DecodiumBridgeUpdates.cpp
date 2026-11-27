#include "DecodiumBridge.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

static constexpr bool kDecodiumUpdateCheckerEnabled = false;

void DecodiumBridge::checkForUpdates()
{
    if (offlineMode()) {
        emit statusMessage(tr("Update checks are paused by Decodium Offline mode."));
        return;
    }
    if (!kDecodiumUpdateCheckerEnabled) {
        if (m_updateAvailable) {
            m_updateAvailable = false;
            emit updateAvailableChanged();
        }
        if (!m_latestVersion.isEmpty()) {
            m_latestVersion.clear();
            emit latestVersionChanged();
        }
        emit statusMessage(tr("Update checks are disabled in this build."));
        return;
    }

    QNetworkAccessManager* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl("https://api.github.com/repos/elisir80/Decodium-4.0-Core-Shannon/releases/latest"));
    req.setRawHeader("Accept", "application/vnd.github.v3+json");
    req.setRawHeader("User-Agent", "Decodium/4.0");

    QNetworkReply* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) return;

        QByteArray data = reply->readAll();
        // Parse "tag_name" from JSON without full JSON dependency
        QString json = QString::fromUtf8(data);
        QRegularExpression re("\"tag_name\"\\s*:\\s*\"([^\"]+)\"");
        auto m = re.match(json);
        if (!m.hasMatch()) return;

        QString latestTag = m.captured(1).remove('v');
        QString currentVer = version();

        m_latestVersion = latestTag;
        emit latestVersionChanged();

        // Simple version compare: split by '.' and compare numerically
        auto splitVer = [](const QString& v) -> QList<int> {
            QList<int> parts;
            for (const auto& p : v.split('.')) parts << p.toInt();
            while (parts.size() < 3) parts << 0;
            return parts;
        };
        auto cv = splitVer(currentVer);
        auto lv = splitVer(latestTag);
        bool newer = (lv[0] > cv[0]) ||
                     (lv[0] == cv[0] && lv[1] > cv[1]) ||
                     (lv[0] == cv[0] && lv[1] == cv[1] && lv[2] > cv[2]);
        if (newer != m_updateAvailable) {
            m_updateAvailable = newer;
            emit updateAvailableChanged();
        }
        if (newer)
            emit statusMessage(tr("Update available: v%1").arg(latestTag));
    });
}

