#pragma once

#include <QByteArray>
#include <QFutureWatcher>
#include <QHash>
#include <QNetworkReply>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class DxccLookup;
class QNetworkAccessManager;
class QSqlDatabase;

// Callsign Intelligence is deliberately independent from the logbook.  It is
// the single lookup/cache layer used by the QSO UI, while its provider tables
// can also be consumed by future roster/award code.
class CallsignIntelligenceService final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString currentCall READ currentCall NOTIFY currentCallChanged)
    Q_PROPERTY(QVariantMap result READ result NOTIFY resultChanged)
    Q_PROPERTY(QVariantList databases READ databases NOTIFY databasesChanged)
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)
    Q_PROPERTY(QString activeProvider READ activeProvider NOTIFY resultChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString lastLookupAt READ lastLookupAt NOTIFY resultChanged)
    Q_PROPERTY(bool lookupPending READ lookupPending NOTIFY lookupPendingChanged)
    Q_PROPERTY(bool cacheHit READ cacheHit NOTIFY resultChanged)
    Q_PROPERTY(bool autoOpenOnQsoStart READ autoOpenOnQsoStart WRITE setAutoOpenOnQsoStart NOTIFY settingsChanged)
    Q_PROPERTY(bool autoCloseAfterLogging READ autoCloseAfterLogging WRITE setAutoCloseAfterLogging NOTIFY settingsChanged)
    Q_PROPERTY(bool enrichMissingFields READ enrichMissingFields WRITE setEnrichMissingFields NOTIFY settingsChanged)
    Q_PROPERTY(int cacheTtlMinutes READ cacheTtlMinutes WRITE setCacheTtlMinutes NOTIFY settingsChanged)
    Q_PROPERTY(QString operatorCallsign READ operatorCallsign WRITE setOperatorCallsign NOTIFY settingsChanged)
    Q_PROPERTY(QString eqslUsername READ eqslUsername WRITE setEqslUsername NOTIFY settingsChanged)
    Q_PROPERTY(QString eqslPassword READ eqslPassword WRITE setEqslPassword NOTIFY settingsChanged)
    Q_PROPERTY(QString lotwUsername READ lotwUsername WRITE setLotwUsername NOTIFY settingsChanged)
    Q_PROPERTY(QString clubLogApiKey READ clubLogApiKey WRITE setClubLogApiKey NOTIFY settingsChanged)
    Q_PROPERTY(QString clubLogEmail READ clubLogEmail WRITE setClubLogEmail NOTIFY settingsChanged)
    Q_PROPERTY(QString clubLogApplicationPassword READ clubLogApplicationPassword WRITE setClubLogApplicationPassword NOTIFY settingsChanged)
    Q_PROPERTY(bool offlineMode READ offlineMode WRITE setOfflineMode NOTIFY offlineModeChanged)
    Q_PROPERTY(bool databaseUpdatePending READ databaseUpdatePending NOTIFY databaseUpdatePendingChanged)

public:
    explicit CallsignIntelligenceService(QObject* parent = nullptr);
    ~CallsignIntelligenceService() override;

    QString currentCall() const { return m_currentCall; }
    QVariantMap result() const { return m_result; }
    QVariantList databases() const;
    QString databasePath() const { return m_databasePath; }
    QString activeProvider() const { return m_result.value(QStringLiteral("provider")).toString(); }
    QString status() const { return m_status; }
    QString lastLookupAt() const { return m_result.value(QStringLiteral("updatedAt")).toString(); }
    bool lookupPending() const { return m_lookupPending; }
    bool cacheHit() const { return m_result.value(QStringLiteral("cacheHit")).toBool(); }

    bool autoOpenOnQsoStart() const { return m_autoOpenOnQsoStart; }
    void setAutoOpenOnQsoStart(bool value);
    bool autoCloseAfterLogging() const { return m_autoCloseAfterLogging; }
    void setAutoCloseAfterLogging(bool value);
    bool enrichMissingFields() const { return m_enrichMissingFields; }
    void setEnrichMissingFields(bool value);
    int cacheTtlMinutes() const { return m_cacheTtlMinutes; }
    void setCacheTtlMinutes(int value);
    QString operatorCallsign() const { return m_operatorCallsign; }
    void setOperatorCallsign(const QString& value);
    QString eqslUsername() const { return m_eqslUsername.isEmpty() ? m_operatorCallsign : m_eqslUsername; }
    void setEqslUsername(const QString& value);
    QString eqslPassword() const { return m_eqslPassword; }
    void setEqslPassword(const QString& value);
    QString lotwUsername() const { return m_lotwUsername.isEmpty() ? m_operatorCallsign : m_lotwUsername; }
    void setLotwUsername(const QString& value);
    void setLotwPassword(const QString& value);
    void setQrzApiKey(const QString& value);
    QString clubLogApiKey() const { return m_clubLogApiKey; }
    void setClubLogApiKey(const QString& value);
    QString clubLogEmail() const { return m_clubLogEmail; }
    void setClubLogEmail(const QString& value);
    QString clubLogApplicationPassword() const { return m_clubLogApplicationPassword; }
    void setClubLogApplicationPassword(const QString& value);
    bool offlineMode() const { return m_offlineMode; }
    void setOfflineMode(bool offline);
    bool databaseUpdatePending() const { return m_databaseUpdatePending; }

    void setDxccLookup(DxccLookup* lookup);

    Q_INVOKABLE void lookup(const QString& callsign, bool forceRefresh = false);
    Q_INVOKABLE bool importDatabase(const QString& provider, const QString& path);
    Q_INVOKABLE void refreshDatabase(const QString& provider);
    Q_INVOKABLE void clearCache(const QString& callsign = QString());
    Q_INVOKABLE bool openProviderLookup(const QString& provider = QString(),
                                        const QString& callsign = QString());
    Q_INVOKABLE QVariantMap lookupForFields(const QString& callsign) const;

    // DXCC cty.dat is loaded by DxccLookup rather than callsign_records.
    void notifyDxccDataChanged();

    // Called by DecodiumBridge at the two semantic boundaries of a QSO.  The
    // defaults are OFF, so merely changing dxCall does not open a window.
    void notifyQsoStarted(const QString& callsign);
    void notifyQsoLogged(const QString& callsign);

    // eQSL InBox, LoTW and QRZ confirmed ADIF are downloaded/imported here, but
    // their confirmations must be merged into the active logbook owned by
    // DecodiumBridge before the provider update ends.
    void completeConfirmedAdifImport(const QString& provider, bool ok, int imported,
                                     int updated, int sourceCount,
                                     const QString& error = QString());

signals:
    void currentCallChanged();
    void resultChanged();
    void databasesChanged();
    void statusChanged();
    void lookupPendingChanged();
    void settingsChanged();
    void lookupWindowRequested();
    void lookupWindowCloseRequested();
    void enrichmentReady(const QString& callsign, const QVariantMap& fields);
    void offlineModeChanged();
    void databaseUpdatePendingChanged();
    void confirmedAdifDownloaded(const QString& provider, const QString& path);
    void externalLookupRequested(const QString& url);

private:
    struct ProviderSpec {
        QString id;
        QString label;
        QString url;
        bool enabled {true};
        bool updateable {true};
    };

    bool openDatabase();
    void createSchema();
    void loadSettings();
    void saveSetting(const QString& key, const QVariant& value);
    void setStatus(const QString& value);
    void setPending(bool value);
    void setDatabaseUpdatePending(bool value);
    QString normalizeCall(const QString& value) const;
    QVariantMap localLookup(const QString& callsign) const;
    QVariantMap cachedLookup(const QString& callsign) const;
    void cacheResult(const QVariantMap& value);
    void finishLookup(const QVariantMap& value, bool fromCache, const QString& status);
    void lookupRemoteClubLog(const QString& callsign);
    void handleRemoteLookupFinished(QNetworkReply* reply, const QString& callsign);
    void handleDatabaseReply(QNetworkReply* reply, const QString& provider,
                             bool eqslInboxAdif = false);
    void startDatabaseImport(const QString& provider, const QByteArray& data);
    void startConfirmedAdifSave(const QString& provider, const QByteArray& data);
    void startConfirmedAdifFileImport(const QString& provider, const QString& path);
    void requestQrzConfirmedPage();
    bool importBytes(const QString& provider, const QByteArray& data, const QString& sourcePath);
    bool importDelimited(const QString& provider, const QByteArray& data);
    bool importAdif(const QString& provider, const QByteArray& data);
    bool importFcc(const QByteArray& data);
    bool importClubLogOqrs(const QByteArray& data);
    bool upsertRecord(const QString& provider, const QVariantMap& record);
    void refreshDatabaseState(const QString& provider,
                              qint64 updatedAt,
                              int rowCount,
                              const QString& status,
                              const QString& error = QString());
    QVariantMap providerState(const QString& provider) const;
    QString providerUrl(const QString& provider) const;
    QString providerLabel(const QString& provider) const;
    QByteArray extractFccEnDat(const QByteArray& data) const;
    QVariantMap mergeRecord(QVariantMap target, const QVariantMap& source) const;
    QString externalUrl(const QString& provider, const QString& callsign) const;

    QString m_currentCall;
    QVariantMap m_result;
    QString m_status;
    bool m_lookupPending {false};
    bool m_offlineMode {false};
    bool m_autoOpenOnQsoStart {false};
    bool m_autoCloseAfterLogging {false};
    bool m_enrichMissingFields {false};
    int m_cacheTtlMinutes {1440};
    QString m_operatorCallsign;
    QString m_eqslUsername;
    QString m_eqslPassword;
    QString m_lotwUsername;
    QString m_lotwPassword;
    QString m_lotwLastQsl;
    QString m_pendingLotwLastQsl;
    QString m_qrzApiKey;
    QString m_clubLogApiKey;
    QString m_clubLogEmail;
    QString m_clubLogApplicationPassword;
    QString m_databasePath;
    QString m_updateProvider;
    QByteArray m_updatePayload;
    QHash<QString, ProviderSpec> m_specs;
    DxccLookup* m_dxccLookup {nullptr};
    QNetworkAccessManager* m_network {nullptr};
    QNetworkReply* m_activeReply {nullptr};
    QFutureWatcher<QVariantMap>* m_databaseImportWatcher {nullptr};
    bool m_databaseUpdatePending {false};
    QString m_qrzAfterLogId;
    int m_qrzPageCount {0};
    QByteArray m_qrzAdifPayload;
    QSqlDatabase* m_database {nullptr};
    QString m_connectionName;
};
