#pragma once
// DecodiumDiagnostics — In-app bug reporting and diagnostic collector
// Collects system info, QML errors, audio state, rig state, and recent logs.
// Exposes properties to QML for the BugReportDialog.

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QQmlEngine>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class DecodiumDiagnostics : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString systemInfo      READ systemInfo      NOTIFY diagnosticsReady)
    Q_PROPERTY(QString qmlErrors       READ qmlErrors       NOTIFY diagnosticsReady)
    Q_PROPERTY(QString recentLog       READ recentLog       NOTIFY diagnosticsReady)
    Q_PROPERTY(QString audioState      READ audioState       NOTIFY diagnosticsReady)
    Q_PROPERTY(QString rigState        READ rigState         NOTIFY diagnosticsReady)
    Q_PROPERTY(QString knownBugMatch   READ knownBugMatch    NOTIFY diagnosticsReady)
    Q_PROPERTY(int     errorCount      READ errorCount       NOTIFY errorCountChanged)
    Q_PROPERTY(bool    submitting      READ submitting       NOTIFY submittingChanged)
    Q_PROPERTY(QString lastSubmitResult READ lastSubmitResult NOTIFY submitFinished)

public:
    explicit DecodiumDiagnostics(QObject *parent = nullptr);

    // Called from main_qml.cpp to feed QML warnings
    void addQmlWarning(const QString &warning);
    // Called from DecodiumBridge to feed log lines
    void addLogLine(const QString &line);
    // Set audio/rig state from bridge
    void setAudioState(const QString &state);
    void setRigState(const QString &state);

    QString systemInfo() const;
    QString qmlErrors() const { return m_qmlErrors.join('\n'); }
    QString recentLog() const { return m_recentLog.join('\n'); }
    QString audioState() const { return m_audioState; }
    QString rigState() const { return m_rigState; }
    QString knownBugMatch() const { return m_knownBugMatch; }
    int errorCount() const { return m_qmlErrors.size(); }
    bool submitting() const { return m_submitting; }
    QString lastSubmitResult() const { return m_lastSubmitResult; }

    // Build full diagnostic report
    Q_INVOKABLE QString buildReport(const QString &userDescription) const;

    // Submit to GitHub Issues (needs GITHUB_TOKEN env or built-in)
    Q_INVOKABLE void submitToGitHub(const QString &title, const QString &body);

    // Analyze current errors against known bugs
    Q_INVOKABLE void analyzeErrors();

    // Clear error log
    Q_INVOKABLE void clearErrors();

signals:
    void diagnosticsReady();
    void errorCountChanged();
    void submittingChanged();
    void submitFinished(bool success, const QString &url);
    void newErrorDetected(const QString &summary);

private:
    void matchKnownBugs();

    QStringList m_qmlErrors;
    QStringList m_recentLog;
    QString     m_audioState;
    QString     m_rigState;
    QString     m_knownBugMatch;
    bool        m_submitting {false};
    QString     m_lastSubmitResult;
    QNetworkAccessManager m_nam;

    static constexpr int MAX_LOG_LINES = 200;
    static constexpr int MAX_QML_ERRORS = 50;
};
