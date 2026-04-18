#include "DecodiumDiagnostics.h"

#include <QSysInfo>
#include <QScreen>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QOperatingSystemVersion>

DecodiumDiagnostics::DecodiumDiagnostics(QObject *parent)
    : QObject(parent)
{
}

void DecodiumDiagnostics::addQmlWarning(const QString &warning)
{
    if (m_qmlErrors.size() >= MAX_QML_ERRORS)
        m_qmlErrors.removeFirst();
    m_qmlErrors.append(QDateTime::currentDateTime().toString(Qt::ISODate)
                       + QStringLiteral(" ") + warning);
    emit errorCountChanged();
    emit diagnosticsReady();

    // Auto-analyze for known bugs
    matchKnownBugs();

    // Notify UI of new error
    emit newErrorDetected(warning.left(120));
}

void DecodiumDiagnostics::addLogLine(const QString &line)
{
    if (m_recentLog.size() >= MAX_LOG_LINES)
        m_recentLog.removeFirst();
    m_recentLog.append(line);
}

void DecodiumDiagnostics::setAudioState(const QString &state) { m_audioState = state; emit diagnosticsReady(); }
void DecodiumDiagnostics::setRigState(const QString &state)   { m_rigState = state;   emit diagnosticsReady(); }

QString DecodiumDiagnostics::systemInfo() const
{
    QStringList info;
    info << QStringLiteral("OS: %1 %2").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture());
    info << QStringLiteral("Qt: %1").arg(qVersion());
    info << QStringLiteral("App: Decodium %1").arg(QCoreApplication::applicationVersion());

    // Screens
    auto const screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        auto *s = screens[i];
        info << QStringLiteral("Screen %1: %2x%3 @%4 DPI (scale %5)")
                .arg(i).arg(s->size().width()).arg(s->size().height())
                .arg(s->logicalDotsPerInch(), 0, 'f', 0)
                .arg(s->devicePixelRatio(), 0, 'f', 2);
    }

    // Audio devices
    auto const inputs = QMediaDevices::audioInputs();
    for (auto const &d : inputs)
        info << QStringLiteral("AudioIn: %1").arg(d.description());
    auto const outputs = QMediaDevices::audioOutputs();
    for (auto const &d : outputs)
        info << QStringLiteral("AudioOut: %1").arg(d.description());

    return info.join('\n');
}

QString DecodiumDiagnostics::buildReport(const QString &userDescription) const
{
    QStringList report;
    report << QStringLiteral("## Bug Report — Decodium %1").arg(QCoreApplication::applicationVersion());
    report << QString();
    report << QStringLiteral("### User Description");
    report << userDescription;
    report << QString();
    report << QStringLiteral("### System Info");
    report << QStringLiteral("```");
    report << systemInfo();
    report << QStringLiteral("```");

    if (!m_audioState.isEmpty()) {
        report << QString();
        report << QStringLiteral("### Audio State");
        report << QStringLiteral("```");
        report << m_audioState;
        report << QStringLiteral("```");
    }

    if (!m_rigState.isEmpty()) {
        report << QString();
        report << QStringLiteral("### Rig/CAT State");
        report << QStringLiteral("```");
        report << m_rigState;
        report << QStringLiteral("```");
    }

    if (!m_qmlErrors.isEmpty()) {
        report << QString();
        report << QStringLiteral("### QML Errors (%1)").arg(m_qmlErrors.size());
        report << QStringLiteral("```");
        // Last 20 errors
        int start = qMax(0, m_qmlErrors.size() - 20);
        for (int i = start; i < m_qmlErrors.size(); ++i)
            report << m_qmlErrors[i];
        report << QStringLiteral("```");
    }

    if (!m_knownBugMatch.isEmpty()) {
        report << QString();
        report << QStringLiteral("### Auto-Analysis");
        report << m_knownBugMatch;
    }

    if (!m_recentLog.isEmpty()) {
        report << QString();
        report << QStringLiteral("### Recent Log (last 30 lines)");
        report << QStringLiteral("```");
        int start = qMax(0, m_recentLog.size() - 30);
        for (int i = start; i < m_recentLog.size(); ++i)
            report << m_recentLog[i];
        report << QStringLiteral("```");
    }

    return report.join('\n');
}

void DecodiumDiagnostics::submitToGitHub(const QString &title, const QString &body)
{
    m_submitting = true;
    emit submittingChanged();

    QJsonObject json;
    json[QStringLiteral("title")] = title;
    json[QStringLiteral("body")]  = body;
    json[QStringLiteral("labels")] = QJsonArray({QStringLiteral("bug"), QStringLiteral("user-report")});

    QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com/repos/iu8lmc/Decodium-4.0-Core-Shannon/issues")));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    // Use built-in token for public issue submission (read/write issues only)
    auto const token = qEnvironmentVariable("DECODIUM_GITHUB_TOKEN");
    if (!token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + token).toUtf8());

    auto *reply = m_nam.post(req, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_submitting = false;
        emit submittingChanged();

        if (reply->error() == QNetworkReply::NoError) {
            auto const doc = QJsonDocument::fromJson(reply->readAll());
            auto const url = doc.object()[QStringLiteral("html_url")].toString();
            m_lastSubmitResult = QStringLiteral("Issue created: ") + url;
            emit submitFinished(true, url);
        } else {
            m_lastSubmitResult = QStringLiteral("Submit failed: ") + reply->errorString()
                + QStringLiteral("\n\nCopy the report and send it manually to:\nhttps://github.com/iu8lmc/Decodium-4.0-Core-Shannon/issues");
            emit submitFinished(false, QString());
        }
        emit diagnosticsReady();
    });
}

void DecodiumDiagnostics::analyzeErrors()
{
    matchKnownBugs();
    emit diagnosticsReady();
}

void DecodiumDiagnostics::clearErrors()
{
    m_qmlErrors.clear();
    m_knownBugMatch.clear();
    emit errorCountChanged();
    emit diagnosticsReady();
}

void DecodiumDiagnostics::matchKnownBugs()
{
    // Pattern-match known bugs against current errors
    struct KnownBug {
        const char *pattern;
        const char *description;
        const char *status;  // "FIXED v1.0.23" or "OPEN"
    };

    static const KnownBug knownBugs[] = {
        {"Syntax error",
         "QML syntax error — usually caused by manual editing of .qml files",
         "FIXED: reinstall from installer to restore original files"},
        {"rootObjects empty",
         "QML failed to load — missing Qt plugin or corrupted installation",
         "FIXED v1.0.23: installer now clears QML cache"},
        {"Another instance is already running",
         "Multiple instances detected — lock file from previous crash",
         "FIXED v1.0.23: stale lock timeout set to 30s"},
        {"Cannot assign to non-existent property",
         "QML component property mismatch — version mismatch between QML and C++",
         "Check: reinstall from latest installer"},
        {"TypeError.*undefined",
         "QML undefined property access — bridge not ready during startup",
         "FIXED v1.0.23: added null guards on bridge properties"},
        {"CAT buffer overflow",
         "Serial port receiving corrupted data without terminator",
         "FIXED v1.0.22: 4KB buffer guard added"},
        {"DPAPI CryptUnprotectData failed",
         "Windows credential decryption failed — registry may be corrupted",
         "OPEN: clear HKCU\\Software\\Decodium\\SecureStore and re-enter credentials"},
        {"SoundInput.*error",
         "Audio input device error — device may be disconnected or in use",
         "Check: verify audio device in Settings"},
        {"rig_set_ptt.*failed",
         "PTT command failed — radio may be off or port misconfigured",
         "Check: verify CAT settings and radio connection"},
    };

    QStringList matches;
    auto const allErrors = m_qmlErrors.join('\n') + '\n' + m_recentLog.join('\n');

    for (auto const &bug : knownBugs) {
        QRegularExpression re(QString::fromLatin1(bug.pattern), QRegularExpression::CaseInsensitiveOption);
        if (re.match(allErrors).hasMatch()) {
            matches << QStringLiteral("**%1**\n  Status: %2\n")
                       .arg(QString::fromLatin1(bug.description),
                            QString::fromLatin1(bug.status));
        }
    }

    m_knownBugMatch = matches.isEmpty()
        ? QStringLiteral("No known bug patterns matched. This may be a new issue.")
        : matches.join('\n');
}
