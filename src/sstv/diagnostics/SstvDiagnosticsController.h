// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QThread>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace decodium::sstv {

class SstvDiagnosticsExportWorker;

// Privacy-bounded SSTV diagnostics facade. Runtime owners supply only scalar
// maps; every value is allowlisted again before it becomes observable or is
// written to disk. Report I/O is performed on a dedicated worker thread.
class SstvDiagnosticsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool exporting READ exporting NOTIFY exportingChanged)
    Q_PROPERTY(QVariantMap applicationInfo READ applicationInfo
               NOTIFY reportDataChanged)
    Q_PROPERTY(QVariantMap platformInfo READ platformInfo
               NOTIFY reportDataChanged)
    Q_PROPERTY(QVariantMap modeRegistryInfo READ modeRegistryInfo
               NOTIFY reportDataChanged)
    Q_PROPERTY(QVariantMap capabilities READ capabilities
               NOTIFY reportDataChanged)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY reportDataChanged)
    Q_PROPERTY(QVariantMap rxMetrics READ rxMetrics NOTIFY reportDataChanged)
    Q_PROPERTY(QVariantMap txMetrics READ txMetrics NOTIFY reportDataChanged)
    Q_PROPERTY(QVariantMap storageMetrics READ storageMetrics
               NOTIFY reportDataChanged)
    Q_PROPERTY(QVariantMap shareMetrics READ shareMetrics
               NOTIFY reportDataChanged)
    Q_PROPERTY(QVariantMap hamdrmMetrics READ hamdrmMetrics
               NOTIFY reportDataChanged)
    Q_PROPERTY(QVariantMap calibrationResults READ calibrationResults
               NOTIFY reportDataChanged)
    Q_PROPERTY(QVariantMap testToneResults READ testToneResults
               NOTIFY reportDataChanged)
    Q_PROPERTY(QVariantList recentEvents READ recentEvents
               NOTIFY recentEventsChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString errorString READ errorString NOTIFY errorStringChanged)

public:
    static constexpr int kReportSchemaVersion = 1;
    static constexpr qsizetype kMaximumReportBytes = 1024 * 1024;

    explicit SstvDiagnosticsController(QObject* parent = nullptr);
    ~SstvDiagnosticsController() override;

    bool ready() const noexcept;
    bool exporting() const noexcept;
    QVariantMap applicationInfo() const;
    QVariantMap platformInfo() const;
    QVariantMap modeRegistryInfo() const;
    QVariantMap capabilities() const;
    QVariantMap settings() const;
    QVariantMap rxMetrics() const;
    QVariantMap txMetrics() const;
    QVariantMap storageMetrics() const;
    QVariantMap shareMetrics() const;
    QVariantMap hamdrmMetrics() const;
    QVariantMap calibrationResults() const;
    QVariantMap testToneResults() const;
    QVariantList recentEvents() const;
    QString statusText() const;
    QString errorString() const;

    // This is deliberately a C++ integration API rather than Q_INVOKABLE:
    // untrusted QML cannot inject arbitrary report content. Unknown top-level
    // sections fail closed; unknown nested fields are discarded by allowlists.
    bool setInputSnapshot(const QVariantMap& snapshot,
                          QString* errorMessage = nullptr);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void exportReport(const QUrl& destination,
                                  bool replaceExisting = false);
    Q_INVOKABLE void clearDiagnosticEvents();
    Q_INVOKABLE void requestTestTone();
    Q_INVOKABLE void shutdown();

signals:
    void readyChanged();
    void exportingChanged();
    void reportDataChanged();
    void recentEventsChanged();
    void statusTextChanged();
    void errorStringChanged();
    void exportFinished(bool success, const QString& message);
    void testToneRequested();
    // The owner supplies a fresh, privacy-bounded scalar snapshot.  A direct
    // owner-thread connection makes refresh/export observe current runtime
    // counters without exposing a writable QML input API.
    void inputSnapshotRequested();

    void writeReportRequested(quint64 requestId,
                              const QByteArray& report,
                              const QString& destinationPath,
                              bool replaceExisting);

private slots:
    void handleWriteFinished(quint64 requestId,
                             bool success,
                             const QString& errorCode);

private:
    QByteArray buildReport(QString* errorMessage) const;
    void refreshEventSnapshot();
    void setExporting(bool value);
    void setStatusText(const QString& value);
    void setErrorString(const QString& value);

    bool m_ready {false};
    bool m_exporting {false};
    bool m_shutdown {false};
    quint64 m_nextRequestId {1};
    quint64 m_activeRequestId {0};
    QVariantMap m_applicationInfo;
    QVariantMap m_platformInfo;
    QVariantMap m_modeRegistryInfo;
    QVariantMap m_capabilities;
    QVariantMap m_settings;
    QVariantMap m_rxMetrics;
    QVariantMap m_txMetrics;
    QVariantMap m_storageMetrics;
    QVariantMap m_shareMetrics;
    QVariantMap m_hamdrmMetrics;
    QVariantMap m_calibrationResults;
    QVariantMap m_testToneResults;
    QVariantList m_recentEvents;
    QString m_statusText;
    QString m_errorString;
    QThread m_workerThread;
    SstvDiagnosticsExportWorker* m_worker {nullptr};
};

} // namespace decodium::sstv
