#pragma once

#include <QObject>
#include <QHash>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QThread;

class UsStateDataManager : public QObject
{
    Q_OBJECT
public:
    explicit UsStateDataManager(QObject* parent = nullptr);
    ~UsStateDataManager() override;

    bool ready() const { return m_ready; }
    bool updating() const { return m_updating; }
    int gridCount() const { return m_callToGrid.size(); }
    int stateCount() const { return m_grid6ToState.size(); }

    QString cacheDirPath() const { return m_cacheDir; }
    QString stateForCall(const QString& call, const QString& gridHint = QString()) const;
    static QString normalizeCall(const QString& call);
    static QString normalizeGrid(const QString& grid);

public slots:
    void ensureLoadedAsync();
    void updateNow();

signals:
    void dataChanged();
    void updatingChanged();
    void statusMessage(const QString& message);

private:
    struct ParsedData
    {
        QHash<QString, QString> callToGrid;
        QHash<QString, QString> grid6ToState;
        QHash<QString, QString> grid4ToState;
        QString error;
    };

    void setUpdating(bool updating);
    void refreshUpdating();
    void startDownload(bool force);
    void handleDownloadFinished(QNetworkReply* reply);
    void parseAsync();
    void applyParsedData(const ParsedData& data);
    QString lookupStateForGrid(const QString& grid) const;

    static ParsedData parseFiles(const QString& gridPath, const QString& statePath);
    QString m_cacheDir;
    QString m_gridPath;
    QString m_statePath;
    QNetworkAccessManager* m_net {nullptr};
    int m_pendingDownloads {0};
    bool m_ready {false};
    bool m_updating {false};
    bool m_parseInProgress {false};
    bool m_downloadFailed {false};
    bool m_redownloadAfterParseFailureAttempted {false};
    QThread* m_parseWorker {nullptr};
    QHash<QString, QString> m_callToGrid;
    QHash<QString, QString> m_grid6ToState;
    QHash<QString, QString> m_grid4ToState;
};
