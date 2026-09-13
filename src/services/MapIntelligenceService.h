#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QThreadPool>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>

class MapLayerModel;
class MapBaseMapService;
class MapExternalOverlayService;
class MapOperationsService;
class MapPskFeedService;
class QTimer;

class MapIntelligenceService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QObject* layerModel READ layerModel CONSTANT)
    Q_PROPERTY(QObject* baseMapService READ baseMapService CONSTANT)
    Q_PROPERTY(QObject* externalOverlayService READ externalOverlayService CONSTANT)
    Q_PROPERTY(QObject* operationsService READ operationsService CONSTANT)
    Q_PROPERTY(QObject* pskFeedService READ pskFeedService CONSTANT)
    Q_PROPERTY(QVariantList coverageCells READ coverageCells NOTIFY coverageChanged)
    Q_PROPERTY(QVariantList roster READ roster NOTIFY rosterChanged)
    Q_PROPERTY(QVariantList rosterPreferences READ rosterPreferences NOTIFY rosterPreferencesChanged)
    Q_PROPERTY(QVariantList awards READ awards NOTIFY awardsChanged)
    Q_PROPERTY(QVariantList awardMissing READ awardMissing NOTIFY awardsChanged)
    Q_PROPERTY(QVariantList alerts READ alerts NOTIFY alertsChanged)
    Q_PROPERTY(QVariantList spotHeatmap READ spotHeatmap NOTIFY spotAnalyticsChanged)
    Q_PROPERTY(QVariantList spotTimeline READ spotTimeline NOTIFY spotAnalyticsChanged)
    Q_PROPERTY(QVariantList spotPaths READ spotPaths NOTIFY spotAnalyticsChanged)
    Q_PROPERTY(QVariantList bandActivity READ bandActivity NOTIFY bandActivityChanged)
    Q_PROPERTY(QVariantList bandActivityTimeline READ bandActivityTimeline NOTIFY bandActivityChanged)
    Q_PROPERTY(QVariantMap bandActivitySummary READ bandActivitySummary NOTIFY bandActivityChanged)
    Q_PROPERTY(QVariantList propagationStatistics READ propagationStatistics NOTIFY propagationStatisticsChanged)
    Q_PROPERTY(QVariantMap propagationSummary READ propagationSummary NOTIFY propagationStatisticsChanged)
    Q_PROPERTY(QVariantMap sourceDecayMinutes READ sourceDecayMinutes WRITE setSourceDecayMinutes NOTIFY mapTemporalSettingsChanged)
    Q_PROPERTY(QVariantList temporalLegend READ temporalLegend NOTIFY mapTemporalSettingsChanged)
    Q_PROPERTY(int bandActivityWindowHours READ bandActivityWindowHours WRITE setBandActivityWindowHours NOTIFY bandActivityWindowHoursChanged)
    Q_PROPERTY(QVariantList rosterRules READ rosterRules NOTIFY rosterRulesChanged)
    Q_PROPERTY(QVariantMap statistics READ statistics NOTIFY statisticsChanged)
    Q_PROPERTY(QString selectedGrid READ selectedGrid NOTIFY gridDetailsChanged)
    Q_PROPERTY(QVariantMap selectedGridSummary READ selectedGridSummary NOTIFY gridDetailsChanged)
    Q_PROPERTY(QVariantList selectedGridLive READ selectedGridLive NOTIFY gridDetailsChanged)
    Q_PROPERTY(QVariantList selectedGridQsos READ selectedGridQsos NOTIFY gridDetailsChanged)
    Q_PROPERTY(bool gridDetailsLoading READ gridDetailsLoading NOTIFY gridDetailsLoadingChanged)
    Q_PROPERTY(QStringList availableBands READ availableBands NOTIFY filtersChanged)
    Q_PROPERTY(QStringList availableModes READ availableModes NOTIFY filtersChanged)
    Q_PROPERTY(QStringList availablePeriods READ availablePeriods CONSTANT)
    Q_PROPERTY(QStringList availableContinents READ availableContinents NOTIFY filtersChanged)
    Q_PROPERTY(QStringList availableDxcc READ availableDxcc NOTIFY filtersChanged)
    Q_PROPERTY(QStringList availableSources READ availableSources NOTIFY filtersChanged)
    Q_PROPERTY(QStringList availablePropagationModes READ availablePropagationModes CONSTANT)
    Q_PROPERTY(QVariantList availablePropagationTypes READ availablePropagationTypes CONSTANT)
    Q_PROPERTY(QStringList availableRosterStatuses READ availableRosterStatuses CONSTANT)
    Q_PROPERTY(QStringList availableRosterHuntScopes READ availableRosterHuntScopes CONSTANT)
    Q_PROPERTY(QStringList availableRosterScopes READ availableRosterScopes CONSTANT)
    Q_PROPERTY(QStringList availableRosterDxccScopes READ availableRosterDxccScopes CONSTANT)
    Q_PROPERTY(QStringList availableRosterRuleTypes READ availableRosterRuleTypes CONSTANT)
    Q_PROPERTY(QStringList availableRosterWantedTypes READ availableRosterWantedTypes CONSTANT)
    Q_PROPERTY(QStringList availableAwardPrograms READ availableAwardPrograms CONSTANT)
    Q_PROPERTY(QStringList availableAwardGoals READ availableAwardGoals CONSTANT)
    Q_PROPERTY(QStringList availableAwardConfirmations READ availableAwardConfirmations CONSTANT)
    Q_PROPERTY(QStringList availableAwardEndorsements READ availableAwardEndorsements NOTIFY awardsChanged)
    Q_PROPERTY(QStringList availablePskDisplayModes READ availablePskDisplayModes CONSTANT)
    Q_PROPERTY(QStringList availableSpotAgeFilters READ availableSpotAgeFilters CONSTANT)
    Q_PROPERTY(QStringList availableCorrelationFilters READ availableCorrelationFilters CONSTANT)
    Q_PROPERTY(QStringList availableRosterColumns READ availableRosterColumns CONSTANT)
    Q_PROPERTY(QStringList availableCallLookupProviders READ availableCallLookupProviders CONSTANT)
    Q_PROPERTY(QString bandFilter READ bandFilter WRITE setBandFilter NOTIFY bandFilterChanged)
    Q_PROPERTY(QString modeFilter READ modeFilter WRITE setModeFilter NOTIFY modeFilterChanged)
    Q_PROPERTY(QString periodFilter READ periodFilter WRITE setPeriodFilter NOTIFY periodFilterChanged)
    Q_PROPERTY(QString continentFilter READ continentFilter WRITE setContinentFilter NOTIFY continentFilterChanged)
    Q_PROPERTY(QString dxccFilter READ dxccFilter WRITE setDxccFilter NOTIFY dxccFilterChanged)
    Q_PROPERTY(QString sourceFilter READ sourceFilter WRITE setSourceFilter NOTIFY sourceFilterChanged)
    Q_PROPERTY(QString propagationFilter READ propagationFilter WRITE setPropagationFilter NOTIFY propagationFilterChanged)
    Q_PROPERTY(bool cqOnly READ cqOnly WRITE setCqOnly NOTIFY cqOnlyChanged)
    Q_PROPERTY(QString rosterSort READ rosterSort WRITE setRosterSort NOTIFY rosterSortChanged)
    Q_PROPERTY(bool rosterSortDescending READ rosterSortDescending WRITE setRosterSortDescending NOTIFY rosterSortDescendingChanged)
    Q_PROPERTY(QString rosterStatusFilter READ rosterStatusFilter WRITE setRosterStatusFilter NOTIFY rosterStatusFilterChanged)
    Q_PROPERTY(QString rosterHuntScope READ rosterHuntScope WRITE setRosterHuntScope NOTIFY rosterHuntScopeChanged)
    Q_PROPERTY(QString rosterScope READ rosterScope WRITE setRosterScope NOTIFY rosterScopeChanged)
    Q_PROPERTY(QString rosterDxccScope READ rosterDxccScope WRITE setRosterDxccScope NOTIFY rosterDxccScopeChanged)
    Q_PROPERTY(int rosterRetentionMinutes READ rosterRetentionMinutes WRITE setRosterRetentionMinutes NOTIFY rosterRetentionMinutesChanged)
    Q_PROPERTY(bool rosterCqOnly READ rosterCqOnly WRITE setRosterCqOnly NOTIFY rosterCqOnlyChanged)
    Q_PROPERTY(QString rosterTextFilter READ rosterTextFilter WRITE setRosterTextFilter NOTIFY rosterTextFilterChanged)
    Q_PROPERTY(QString rosterTextMode READ rosterTextMode WRITE setRosterTextMode NOTIFY rosterTextModeChanged)
    Q_PROPERTY(QString activeAwardProgram READ activeAwardProgram WRITE setActiveAwardProgram NOTIFY activeAwardProgramChanged)
    Q_PROPERTY(QString awardGoal READ awardGoal WRITE setAwardGoal NOTIFY awardGoalChanged)
    Q_PROPERTY(QString awardEndorsement READ awardEndorsement WRITE setAwardEndorsement NOTIFY awardFiltersChanged)
    Q_PROPERTY(QString awardConfirmation READ awardConfirmation WRITE setAwardConfirmation NOTIFY awardFiltersChanged)
    Q_PROPERTY(QString awardCallsign READ awardCallsign WRITE setAwardCallsign NOTIFY awardFiltersChanged)
    Q_PROPERTY(QString awardFromDate READ awardFromDate WRITE setAwardFromDate NOTIFY awardFiltersChanged)
    Q_PROPERTY(QString awardToDate READ awardToDate WRITE setAwardToDate NOTIFY awardFiltersChanged)
    Q_PROPERTY(bool rosterUsesLoTW READ rosterUsesLoTW WRITE setRosterUsesLoTW NOTIFY rosterFiltersChanged)
    Q_PROPERTY(int rosterMaxLoTWDays READ rosterMaxLoTWDays WRITE setRosterMaxLoTWDays NOTIFY rosterFiltersChanged)
    Q_PROPERTY(bool rosterUsesEQSL READ rosterUsesEQSL WRITE setRosterUsesEQSL NOTIFY rosterFiltersChanged)
    Q_PROPERTY(bool rosterUsesOQRS READ rosterUsesOQRS WRITE setRosterUsesOQRS NOTIFY rosterFiltersChanged)
    Q_PROPERTY(bool rosterSpottedMeOnly READ rosterSpottedMeOnly WRITE setRosterSpottedMeOnly NOTIFY rosterFiltersChanged)
    Q_PROPERTY(bool rosterMinSnrEnabled READ rosterMinSnrEnabled WRITE setRosterMinSnrEnabled NOTIFY rosterFiltersChanged)
    Q_PROPERTY(int rosterMinSnr READ rosterMinSnr WRITE setRosterMinSnr NOTIFY rosterFiltersChanged)
    Q_PROPERTY(bool rosterMaxDtEnabled READ rosterMaxDtEnabled WRITE setRosterMaxDtEnabled NOTIFY rosterFiltersChanged)
    Q_PROPERTY(double rosterMaxDt READ rosterMaxDt WRITE setRosterMaxDt NOTIFY rosterFiltersChanged)
    Q_PROPERTY(bool rosterTreatRr73AsCq READ rosterTreatRr73AsCq WRITE setRosterTreatRr73AsCq NOTIFY rosterTreatRr73AsCqChanged)
    Q_PROPERTY(QStringList rosterWantedTypes READ rosterWantedTypes WRITE setRosterWantedTypes NOTIFY rosterWantedTypesChanged)
    Q_PROPERTY(QVariantList rosterWantedMatrix READ rosterWantedMatrix NOTIFY rosterMatricesChanged)
    Q_PROPERTY(QVariantList rosterExceptionMatrix READ rosterExceptionMatrix NOTIFY rosterMatricesChanged)
    Q_PROPERTY(int gridPrecision READ gridPrecision WRITE setGridPrecision NOTIFY gridPrecisionChanged)
    Q_PROPERTY(int liveDecayMinutes READ liveDecayMinutes WRITE setLiveDecayMinutes NOTIFY liveDecayMinutesChanged)
    Q_PROPERTY(bool splitGridEnabled READ splitGridEnabled WRITE setSplitGridEnabled NOTIFY splitGridEnabledChanged)
    Q_PROPERTY(bool coveragePushPinsEnabled READ coveragePushPinsEnabled WRITE setCoveragePushPinsEnabled NOTIFY coveragePushPinsEnabledChanged)
    Q_PROPERTY(bool timeZoneOverlayEnabled READ timeZoneOverlayEnabled WRITE setTimeZoneOverlayEnabled NOTIFY timeZoneOverlayEnabledChanged)
    Q_PROPERTY(QString pskDisplayMode READ pskDisplayMode WRITE setPskDisplayMode NOTIFY pskDisplayModeChanged)
    Q_PROPERTY(int pskOpacityPercent READ pskOpacityPercent WRITE setPskOpacityPercent NOTIFY pskOpacityPercentChanged)
    Q_PROPERTY(QString spotAgeFilter READ spotAgeFilter WRITE setSpotAgeFilter NOTIFY spotAgeFilterChanged)
    Q_PROPERTY(QString spotCorrelationFilter READ spotCorrelationFilter WRITE setSpotCorrelationFilter NOTIFY spotCorrelationFilterChanged)
    Q_PROPERTY(QStringList rosterVisibleColumns READ rosterVisibleColumns WRITE setRosterVisibleColumns NOTIFY rosterVisibleColumnsChanged)
    Q_PROPERTY(QString callLookupProvider READ callLookupProvider WRITE setCallLookupProvider NOTIFY callLookupProviderChanged)
    Q_PROPERTY(bool alertNewGridEnabled READ alertNewGridEnabled WRITE setAlertNewGridEnabled NOTIFY alertRulesChanged)
    Q_PROPERTY(bool alertNewDxccEnabled READ alertNewDxccEnabled WRITE setAlertNewDxccEnabled NOTIFY alertRulesChanged)
    Q_PROPERTY(bool alertCqEnabled READ alertCqEnabled WRITE setAlertCqEnabled NOTIFY alertRulesChanged)
    Q_PROPERTY(QString alertCallPattern READ alertCallPattern WRITE setAlertCallPattern NOTIFY alertRulesChanged)
    Q_PROPERTY(bool workedLayerEnabled READ workedLayerEnabled WRITE setWorkedLayerEnabled NOTIFY workedLayerEnabledChanged)
    Q_PROPERTY(bool confirmedLayerEnabled READ confirmedLayerEnabled WRITE setConfirmedLayerEnabled NOTIFY confirmedLayerEnabledChanged)
    Q_PROPERTY(bool liveLayerEnabled READ liveLayerEnabled WRITE setLiveLayerEnabled NOTIFY liveLayerEnabledChanged)
    Q_PROPERTY(bool activeLayerEnabled READ activeLayerEnabled WRITE setActiveLayerEnabled NOTIFY activeLayerEnabledChanged)
    Q_PROPERTY(bool missingLayerEnabled READ missingLayerEnabled WRITE setMissingLayerEnabled NOTIFY missingLayerEnabledChanged)
    Q_PROPERTY(bool pskLayerEnabled READ pskLayerEnabled WRITE setPskLayerEnabled NOTIFY pskLayerEnabledChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool offlineMode READ offlineMode NOTIFY offlineModeChanged)
    Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY sourcePathChanged)
    Q_PROPERTY(QString databasePath READ databasePath CONSTANT)
    Q_PROPERTY(int qsoCount READ qsoCount NOTIFY coverageChanged)
    Q_PROPERTY(int workedGridCount READ workedGridCount NOTIFY coverageChanged)
    Q_PROPERTY(int confirmedGridCount READ confirmedGridCount NOTIFY coverageChanged)
    Q_PROPERTY(int activeGridCount READ activeGridCount NOTIFY coverageChanged)
    Q_PROPERTY(int missingGridCount READ missingGridCount NOTIFY coverageChanged)
    Q_PROPERTY(int liveSpotCount READ liveSpotCount NOTIFY rosterChanged)
    Q_PROPERTY(int rosterCount READ rosterCount NOTIFY rosterChanged)
    Q_PROPERTY(int rosterWantedCount READ rosterWantedCount NOTIFY rosterChanged)
    Q_PROPERTY(int rosterNewCount READ rosterNewCount NOTIFY rosterChanged)
    Q_PROPERTY(int rosterUnconfirmedCount READ rosterUnconfirmedCount NOTIFY rosterChanged)
    Q_PROPERTY(int rosterPreferenceCount READ rosterPreferenceCount NOTIFY rosterPreferencesChanged)
    Q_PROPERTY(int unreadAlertCount READ unreadAlertCount NOTIFY alertsChanged)

public:
    explicit MapIntelligenceService(QObject* parent = nullptr,
                                    const QString& databasePath = {});
    ~MapIntelligenceService() override;

    QObject* layerModel() const;
    QObject* baseMapService() const;
    QObject* externalOverlayService() const;
    QObject* operationsService() const;
    QObject* pskFeedService() const;
    QVariantList coverageCells() const { return m_coverageCells; }
    QVariantList roster() const { return m_roster; }
    QVariantList rosterPreferences() const { return m_rosterPreferences; }
    QVariantList awards() const { return m_awards; }
    QVariantList awardMissing() const { return m_awardMissing; }
    QVariantList alerts() const { return m_alerts; }
    QVariantList spotHeatmap() const { return m_spotHeatmap; }
    QVariantList spotTimeline() const { return m_spotTimeline; }
    QVariantList spotPaths() const { return m_spotPaths; }
    QVariantList bandActivity() const { return m_bandActivity; }
    QVariantList bandActivityTimeline() const { return m_bandActivityTimeline; }
    QVariantMap bandActivitySummary() const { return m_bandActivitySummary; }
    QVariantList propagationStatistics() const { return m_propagationStatistics; }
    QVariantMap propagationSummary() const { return m_propagationSummary; }
    QVariantMap sourceDecayMinutes() const { return m_sourceDecayMinutes; }
    QVariantList temporalLegend() const;
    int bandActivityWindowHours() const { return m_bandActivityWindowHours; }
    QVariantList rosterRules() const { return m_rosterRules; }
    QVariantMap statistics() const { return m_statistics; }
    QStringList availableBands() const { return m_availableBands; }
    QStringList availableModes() const { return m_availableModes; }
    QStringList availablePeriods() const;
    QStringList availableContinents() const { return m_availableContinents; }
    QStringList availableDxcc() const { return m_availableDxcc; }
    QStringList availableSources() const { return m_availableSources; }
    QStringList availablePropagationModes() const;
    QVariantList availablePropagationTypes() const;
    QStringList availableRosterStatuses() const;
    QStringList availableRosterHuntScopes() const;
    QStringList availableRosterScopes() const;
    QStringList availableRosterDxccScopes() const;
    QStringList availableRosterRuleTypes() const;
    QStringList availableRosterWantedTypes() const;
    QStringList availableAwardPrograms() const;
    QStringList availableAwardGoals() const;
    QStringList availableAwardConfirmations() const;
    QStringList availableAwardEndorsements() const;
    QStringList availablePskDisplayModes() const;
    QStringList availableSpotAgeFilters() const;
    QStringList availableCorrelationFilters() const;
    QStringList availableRosterColumns() const;
    QStringList availableCallLookupProviders() const;
    QString bandFilter() const { return m_bandFilter; }
    QString modeFilter() const { return m_modeFilter; }
    QString periodFilter() const { return m_periodFilter; }
    QString continentFilter() const { return m_continentFilter; }
    QString dxccFilter() const { return m_dxccFilter; }
    QString sourceFilter() const { return m_sourceFilter; }
    QString propagationFilter() const { return m_propagationFilter; }
    bool cqOnly() const { return m_cqOnly; }
    QString rosterSort() const { return m_rosterSort; }
    bool rosterSortDescending() const { return m_rosterSortDescending; }
    QString rosterStatusFilter() const { return m_rosterStatusFilter; }
    QString rosterHuntScope() const { return m_rosterHuntScope; }
    QString rosterScope() const { return m_rosterScope; }
    QString rosterDxccScope() const { return m_rosterDxccScope; }
    int rosterRetentionMinutes() const { return m_rosterRetentionMinutes; }
    bool rosterCqOnly() const { return m_rosterCqOnly; }
    QString rosterTextFilter() const { return m_rosterTextFilter; }
    QString rosterTextMode() const { return m_rosterTextMode; }
    QString activeAwardProgram() const { return m_activeAwardProgram; }
    QString awardGoal() const { return m_awardGoal; }
    QString awardEndorsement() const { return m_awardEndorsement; }
    QString awardConfirmation() const { return m_awardConfirmation; }
    QString awardCallsign() const { return m_awardCallsign; }
    QString awardFromDate() const { return m_awardFromDate; }
    QString awardToDate() const { return m_awardToDate; }
    bool rosterUsesLoTW() const { return m_rosterUsesLoTW; }
    int rosterMaxLoTWDays() const { return m_rosterMaxLoTWDays; }
    bool rosterUsesEQSL() const { return m_rosterUsesEQSL; }
    bool rosterUsesOQRS() const { return m_rosterUsesOQRS; }
    bool rosterSpottedMeOnly() const { return m_rosterSpottedMeOnly; }
    bool rosterMinSnrEnabled() const { return m_rosterMinSnrEnabled; }
    int rosterMinSnr() const { return m_rosterMinSnr; }
    bool rosterMaxDtEnabled() const { return m_rosterMaxDtEnabled; }
    double rosterMaxDt() const { return m_rosterMaxDt; }
    bool rosterTreatRr73AsCq() const { return m_rosterTreatRr73AsCq; }
    QStringList rosterWantedTypes() const { return m_rosterWantedTypes; }
    QVariantList rosterWantedMatrix() const { return m_rosterWantedMatrix; }
    QVariantList rosterExceptionMatrix() const { return m_rosterExceptionMatrix; }
    int gridPrecision() const { return m_gridPrecision; }
    int liveDecayMinutes() const { return m_liveDecayMinutes; }
    bool splitGridEnabled() const { return m_splitGridEnabled; }
    bool coveragePushPinsEnabled() const { return m_coveragePushPinsEnabled; }
    bool timeZoneOverlayEnabled() const { return m_timeZoneOverlayEnabled; }
    QString pskDisplayMode() const { return m_pskDisplayMode; }
    int pskOpacityPercent() const { return m_pskOpacityPercent; }
    QString spotAgeFilter() const { return m_spotAgeFilter; }
    QString spotCorrelationFilter() const { return m_spotCorrelationFilter; }
    QStringList rosterVisibleColumns() const { return m_rosterVisibleColumns; }
    QString callLookupProvider() const { return m_callLookupProvider; }
    bool alertNewGridEnabled() const { return m_alertNewGridEnabled; }
    bool alertNewDxccEnabled() const { return m_alertNewDxccEnabled; }
    bool alertCqEnabled() const { return m_alertCqEnabled; }
    QString alertCallPattern() const { return m_alertCallPattern; }
    bool workedLayerEnabled() const;
    bool confirmedLayerEnabled() const;
    bool liveLayerEnabled() const;
    bool activeLayerEnabled() const;
    bool missingLayerEnabled() const;
    bool pskLayerEnabled() const;
    bool liveEntryMatchesCurrentFilters(const QVariantMap& entry,
                                        qint64 dialFrequencyHz,
                                        const QString& band) const;
    bool loading() const { return m_loading; }
    QString sourcePath() const { return m_sourcePath; }
    QString databasePath() const { return m_databasePath; }
    int qsoCount() const { return m_qsoCount; }
    int workedGridCount() const { return m_workedGridCount; }
    int confirmedGridCount() const { return m_confirmedGridCount; }
    int activeGridCount() const { return m_activeGridCount; }
    int missingGridCount() const { return m_missingGridCount; }
    int liveSpotCount() const { return m_liveSpotCount; }
    int rosterCount() const { return m_roster.size(); }
    int rosterWantedCount() const { return m_rosterWantedCount; }
    int rosterNewCount() const { return m_rosterNewCount; }
    int rosterUnconfirmedCount() const { return m_rosterUnconfirmedCount; }
    int rosterPreferenceCount() const { return m_rosterPreferences.size(); }
    int unreadAlertCount() const { return m_unreadAlertCount; }
    bool offlineMode() const { return m_offlineMode; }

    void setOfflineMode(bool offline);
    QString selectedGrid() const { return m_selectedGrid; }
    QVariantMap selectedGridSummary() const { return m_selectedGridSummary; }
    QVariantList selectedGridLive() const { return m_selectedGridLive; }
    QVariantList selectedGridQsos() const { return m_selectedGridQsos; }
    bool gridDetailsLoading() const { return m_gridDetailsLoading; }

    void setBandFilter(const QString& value);
    void setModeFilter(const QString& value);
    void setPeriodFilter(const QString& value);
    void setContinentFilter(const QString& value);
    void setDxccFilter(const QString& value);
    void setSourceFilter(const QString& value);
    void setPropagationFilter(const QString& value);
    void setCqOnly(bool enabled);
    void setRosterSort(const QString& value);
    void setRosterSortDescending(bool descending);
    void setRosterStatusFilter(const QString& value);
    void setRosterHuntScope(const QString& value);
    void setRosterScope(const QString& value);
    void setRosterDxccScope(const QString& value);
    void setRosterRetentionMinutes(int minutes);
    void setRosterCqOnly(bool enabled);
    void setRosterTextFilter(const QString& value);
    void setRosterTextMode(const QString& value);
    void setActiveAwardProgram(const QString& value);
    void setAwardGoal(const QString& value);
    void setAwardEndorsement(const QString& value);
    void setAwardConfirmation(const QString& value);
    void setAwardCallsign(const QString& value);
    void setAwardFromDate(const QString& value);
    void setAwardToDate(const QString& value);
    void setRosterUsesLoTW(bool enabled);
    void setRosterMaxLoTWDays(int days);
    void setRosterUsesEQSL(bool enabled);
    void setRosterUsesOQRS(bool enabled);
    void setRosterSpottedMeOnly(bool enabled);
    void setRosterMinSnrEnabled(bool enabled);
    void setRosterMinSnr(int snr);
    void setRosterMaxDtEnabled(bool enabled);
    void setRosterMaxDt(double dt);
    void setRosterTreatRr73AsCq(bool enabled);
    void setRosterWantedTypes(const QStringList& types);
    void setGridPrecision(int precision);
    void setLiveDecayMinutes(int minutes);
    void setSplitGridEnabled(bool enabled);
    void setCoveragePushPinsEnabled(bool enabled);
    void setTimeZoneOverlayEnabled(bool enabled);
    void setPskDisplayMode(const QString& mode);
    void setPskOpacityPercent(int percent);
    void setSpotAgeFilter(const QString& value);
    void setSpotCorrelationFilter(const QString& value);
    void setBandActivityWindowHours(int hours);
    void setSourceDecayMinutes(const QVariantMap& values);
    void setRosterVisibleColumns(const QStringList& columns);
    void setCallLookupProvider(const QString& provider);
    void setAlertNewGridEnabled(bool enabled);
    void setAlertNewDxccEnabled(bool enabled);
    void setAlertCqEnabled(bool enabled);
    void setAlertCallPattern(const QString& pattern);
    void setWorkedLayerEnabled(bool enabled);
    void setConfirmedLayerEnabled(bool enabled);
    void setLiveLayerEnabled(bool enabled);
    void setActiveLayerEnabled(bool enabled);
    void setMissingLayerEnabled(bool enabled);
    void setPskLayerEnabled(bool enabled);

    Q_INVOKABLE void reloadFromAdif(const QString& path);
    Q_INVOKABLE QString reserveMapConfigurationPath() const;
    Q_INVOKABLE bool exportMapConfiguration(const QString& path,
                                            const QVariantMap& viewport = {});
    Q_INVOKABLE QVariantMap importMapConfiguration(const QString& path);
    Q_INVOKABLE void appendAdifRecord(const QByteArray& record);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clearLiveSpots();
    Q_INVOKABLE void clearAlerts();
    Q_INVOKABLE void markAlertsRead();
    Q_INVOKABLE void setRosterCallWatched(const QString& call, bool watched);
    Q_INVOKABLE void setRosterCallIgnored(const QString& call, bool ignored);
    Q_INVOKABLE void setRosterDxccIgnored(const QString& dxcc, bool ignored);
    Q_INVOKABLE void removeRosterPreference(const QString& type,
                                            const QString& value);
    Q_INVOKABLE void clearRosterPreferences();
    Q_INVOKABLE void selectGrid(const QString& grid);
    Q_INVOKABLE void clearGridSelection();
    Q_INVOKABLE void ingestPskSpots(const QVariantList& rows,
                                     const QString& senderCall,
                                     const QString& senderGrid);
    Q_INVOKABLE void replacePskHeardBySpots(const QVariantList& rows,
                                            const QString& senderCall,
                                            const QString& senderGrid);
    Q_INVOKABLE void configurePskFeed(const QString& callsign, const QString& grid);
    Q_INVOKABLE void setRosterRule(const QString& type, const QString& value,
                                   const QString& action,
                                   const QString& band = {},
                                   const QString& mode = {});
    Q_INVOKABLE void removeRosterRule(const QString& type, const QString& value,
                                      const QString& band = {},
                                      const QString& mode = {});
    Q_INVOKABLE void setRosterStationCall(const QString& call);

    void ingestDecodeEntry(const QVariantMap& entry,
                           qint64 dialFrequencyHz,
                           const QString& band);

signals:
    void coverageChanged();
    void rosterChanged();
    void rosterPreferencesChanged();
    void awardsChanged();
    void alertsChanged();
    void statisticsChanged();
    void filtersChanged();
    void bandFilterChanged();
    void modeFilterChanged();
    void periodFilterChanged();
    void continentFilterChanged();
    void dxccFilterChanged();
    void sourceFilterChanged();
    void propagationFilterChanged();
    void propagationStatisticsChanged();
    void cqOnlyChanged();
    void rosterSortChanged();
    void rosterSortDescendingChanged();
    void rosterStatusFilterChanged();
    void rosterHuntScopeChanged();
    void rosterScopeChanged();
    void rosterDxccScopeChanged();
    void rosterRetentionMinutesChanged();
    void rosterCqOnlyChanged();
    void rosterTextFilterChanged();
    void rosterTextModeChanged();
    void activeAwardProgramChanged();
    void awardGoalChanged();
    void awardFiltersChanged();
    void rosterFiltersChanged();
    void rosterTreatRr73AsCqChanged();
    void rosterWantedTypesChanged();
    void rosterMatricesChanged();
    void gridPrecisionChanged();
    void liveDecayMinutesChanged();
    void splitGridEnabledChanged();
    void coveragePushPinsEnabledChanged();
    void timeZoneOverlayEnabledChanged();
    void pskDisplayModeChanged();
    void pskOpacityPercentChanged();
    void spotAgeFilterChanged();
    void spotCorrelationFilterChanged();
    void rosterVisibleColumnsChanged();
    void spotAnalyticsChanged();
    void bandActivityChanged();
    void bandActivityWindowHoursChanged();
    void mapTemporalSettingsChanged();
    void rosterRulesChanged();
    void callLookupProviderChanged();
    void alertRulesChanged();
    void workedLayerEnabledChanged();
    void confirmedLayerEnabledChanged();
    void liveLayerEnabledChanged();
    void activeLayerEnabledChanged();
    void missingLayerEnabledChanged();
    void pskLayerEnabledChanged();
    void loadingChanged();
    void offlineModeChanged();
    void sourcePathChanged();
    void gridDetailsChanged();
    void gridDetailsLoadingChanged();

private slots:
    void scheduleQuery();
    void flushPendingLiveSpots();
    void flushPendingSnapshot();
    void flushSnapshotNotifications();

private:
    enum SnapshotNotification : int {
        SnapshotNotifyNone          = 0,
        SnapshotNotifyFilters       = 1 << 0,
        SnapshotNotifyCoverage      = 1 << 1,
        SnapshotNotifyRoster        = 1 << 2,
        SnapshotNotifyPreferences   = 1 << 3,
        SnapshotNotifyAwards        = 1 << 4,
        SnapshotNotifyAlerts        = 1 << 5,
        SnapshotNotifySpotAnalytics = 1 << 6,
        SnapshotNotifyBandActivity  = 1 << 7,
        SnapshotNotifyPropagation   = 1 << 8,
        SnapshotNotifyRules         = 1 << 9,
        SnapshotNotifyMatrices      = 1 << 10,
        SnapshotNotifyStatistics    = 1 << 11
    };

    struct QsoRecord {
        QString sourceKey;
        QString call;
        QString grid;
        QString grid4;
        QString grid6;
        QStringList vuccGrids;
        QString band;
        QString mode;
        QString propagationMode;
        QString satelliteName;
        QString satelliteMode;
        QString qsoDate;
        QString timeOn;
        QString source {QStringLiteral("ADIF")};
        QString operatorCall;
        QString dxcc;
        QString continent;
        QString state;
        QString county;
        QString potaReference;
        QString iotaReference;
        QString wpxPrefix;
        double frequencyMhz {0.0};
        double receiveFrequencyMhz {0.0};
        qint64 qsoEpoch {0};
        int cqZone {0};
        int ituZone {0};
        int dxccNumber {0};
        bool confirmed {false};
        bool lotwConfirmed {false};
        bool eqslConfirmed {false};
        bool oqrs {false};
    };

    struct LiveSpot {
        QString uniqueKey;
        QString call;
        QString grid;
        QString grid4;
        QString grid6;
        // Provenance is deliberately independent from the spot source: a
        // decoded messages can be correlated with PSK data without making
        // their over-the-air locator any less authoritative.
        QString gridOrigin {QStringLiteral("UNKNOWN")};
        QString band;
        QString mode;
        QString propagationMode;
        QString message;
        QString observedUtc;
        qint64 observedMs {0};
        qint64 frequencyHz {0};
        double distanceKm {-1.0};
        int snr {0};
        double dt {0.0};
        QString source;
        int dxccNumber {0};
        QString dxcc;
        QString continent;
        QString state;
        QString county;
        QString potaReference;
        QString iotaReference;
        QString wpxPrefix;
        QString targetCall;
        QString activityType;
        QString receiverCall;
        QString receiverGrid;
        QString provider;
        QString direction {QStringLiteral("RX")};
        int cqZone {0};
        int ituZone {0};
        bool isCq {false};
    };

    struct PendingDecode {
        QVariantMap entry;
        qint64 dialFrequencyHz {0};
        QString band;
    };

    struct Snapshot {
        QVariantList coverage;
        QVariantList roster;
        QVariantList rosterPreferences;
        QVariantList awards;
        QVariantList awardMissing;
        QVariantList alerts;
        QVariantList spotHeatmap;
        QVariantList spotTimeline;
        QVariantList spotPaths;
        QVariantList bandActivity;
        QVariantList bandActivityTimeline;
        QVariantMap bandActivitySummary;
        QVariantList propagationStatistics;
        QVariantMap propagationSummary;
        QVariantList rosterRules;
        QVariantList rosterWantedMatrix;
        QVariantList rosterExceptionMatrix;
        QVariantMap statistics;
        QStringList bands {QStringLiteral("All")};
        QStringList modes {QStringLiteral("All")};
        QStringList continents {QStringLiteral("All")};
        QStringList dxcc {QStringLiteral("All")};
        QStringList sources {QStringLiteral("All")};
        int qsoCount {0};
        int workedGridCount {0};
        int confirmedGridCount {0};
        int activeGridCount {0};
        int missingGridCount {0};
        int liveSpotCount {0};
        int pskListenerCount {0};
        int rosterWantedCount {0};
        int rosterNewCount {0};
        int rosterUnconfirmedCount {0};
        int unreadAlertCount {0};
        QString error;
    };

    struct QueryOptions {
        QString band;
        QString mode;
        QString period;
        QString continent;
        QString dxcc;
        QString source;
        QString propagation;
        QString rosterSort;
        QString rosterStatus;
        QString rosterHuntScope;
        QString rosterScope;
        QString rosterDxccScope;
        QString rosterMyCall;
        QString rosterMyDxcc;
        QString activeAwardProgram;
        QString awardGoal;
        int rosterRetentionMinutes {5};
        int gridPrecision {4};
        int liveDecayMinutes {15};
        QVariantMap sourceDecayMinutes;
        bool cqOnly {false};
        bool rosterSortDescending {true};
        bool rosterCqOnly {false};
        bool splitGridEnabled {true};
        QString rosterText;
        QString rosterTextMode;
        bool pskLayerEnabled {true};
        QString pskDisplayMode {QStringLiteral("Overlay")};
        double pskOpacity {0.65};
        QString spotAgeFilter {QStringLiteral("15 min")};
        QString spotCorrelationFilter {QStringLiteral("All")};
        int bandActivityWindowHours {6};
        bool rosterUsesLoTW {false};
        int rosterMaxLoTWDays {810};
        bool rosterUsesEQSL {false};
        bool rosterUsesOQRS {false};
        bool rosterSpottedMeOnly {false};
        bool rosterMinSnrEnabled {false};
        int rosterMinSnr {-25};
        bool rosterMaxDtEnabled {false};
        double rosterMaxDt {0.5};
        bool rosterTreatRr73AsCq {false};
        QStringList rosterWantedTypes;
        QString awardEndorsement;
        QString awardConfirmation {QStringLiteral("Any")};
        QString awardCallsign;
        QString awardFromDate;
        QString awardToDate;
    };

    struct AlertRules {
        bool newGridEnabled {true};
        bool newDxccEnabled {true};
        bool cqEnabled {true};
        QString callPattern;
    };

    struct GridDetails {
        QVariantMap summary;
        QVariantList live;
        QVariantList qsos;
        QString error;
    };

    static QList<QsoRecord> parseAdif(const QByteArray& data);
    static LiveSpot liveSpotFromEntry(const QVariantMap& entry,
                                      qint64 dialFrequencyHz,
                                      const QString& band);
    static Snapshot queryDatabase(const QString& databasePath,
                                  const QueryOptions& options);
    static GridDetails queryGridDetails(const QString& databasePath,
                                        const QString& grid);
    static bool importAdifIntoDatabase(const QString& databasePath,
                                       const QString& sourcePath,
                                       const QByteArray& data,
                                       const QString& fingerprint,
                                       const QString& defaultOperatorCall,
                                       QString* error);
    static bool appendQsoRecords(const QString& databasePath,
                                 const QList<QsoRecord>& records,
                                 QString* error);
    static bool appendLiveSpots(const QString& databasePath,
                                const QList<LiveSpot>& spots,
                                const AlertRules& rules,
                                QString* error);
    void queueSnapshotNotifications(int flags);
    static bool clearLiveSpotRows(const QString& databasePath, QString* error);
    static bool clearPskHeardByRows(const QString& databasePath, QString* error);
    static bool clearAlertRows(const QString& databasePath, QString* error);
    static bool markAlertRowsRead(const QString& databasePath, QString* error);
    static bool updateRosterPreference(const QString& databasePath,
                                       const QString& call,
                                       bool watched,
                                       bool ignored,
                                       QString* error);
    static bool updateRosterIgnore(const QString& databasePath,
                                   const QString& type,
                                   const QString& value,
                                   bool ignored,
                                   QString* error);
    static bool removeRosterPreferenceRow(const QString& databasePath,
                                          const QString& type,
                                          const QString& value,
                                          QString* error);
    static bool clearRosterPreferenceRows(const QString& databasePath,
                                          QString* error);
    static bool updateRosterRuleRow(const QString& databasePath,
                                    const QString& type, const QString& value,
                                    const QString& action, const QString& band,
                                    const QString& mode, QString* error);
    static bool removeRosterRuleRow(const QString& databasePath,
                                    const QString& type, const QString& value,
                                    const QString& band, const QString& mode,
                                    QString* error);

    void queueSnapshotQuery(quint64 generation);
    void queuePskSpots(const QVariantList& rows,
                       const QString& senderCall,
                       const QString& senderGrid,
                       bool replaceHeardBySnapshot);
    void applySnapshot(quint64 generation, Snapshot snapshot);
    void applySnapshotNow(Snapshot snapshot);
    void applyGridDetails(quint64 generation, const QString& grid,
                          GridDetails details);
    void rebuildVisibleCoverage();
    void setLoading(bool loading);
    void setGridDetailsLoading(bool loading);
    void saveSetting(const QString& key, const QVariant& value) const;

    MapLayerModel* m_layerModel {nullptr};
    MapBaseMapService* m_baseMapService {nullptr};
    MapExternalOverlayService* m_externalOverlayService {nullptr};
    MapOperationsService* m_operationsService {nullptr};
    MapPskFeedService* m_pskFeedService {nullptr};
    QThreadPool m_workerPool;
    QTimer* m_queryTimer {nullptr};
    QTimer* m_liveFlushTimer {nullptr};
    QTimer* m_snapshotFlushTimer {nullptr};
    QTimer* m_snapshotNotificationTimer {nullptr};
    QList<PendingDecode> m_pendingDecodes;
    Snapshot m_pendingSnapshot;
    quint64 m_pendingSnapshotGeneration {0};
    bool m_snapshotPending {false};
    int m_pendingSnapshotNotificationFlags {SnapshotNotifyNone};
    QVariantList m_rawCoverage;
    QVariantList m_coverageCells;
    QVariantList m_roster;
    QVariantList m_rosterPreferences;
    QVariantList m_awards;
    QVariantList m_awardMissing;
    QVariantList m_alerts;
    QVariantList m_spotHeatmap;
    QVariantList m_spotTimeline;
    QVariantList m_spotPaths;
    QVariantList m_bandActivity;
    QVariantList m_bandActivityTimeline;
    QVariantMap m_bandActivitySummary;
    QVariantList m_propagationStatistics;
    QVariantMap m_propagationSummary;
    QVariantList m_rosterRules;
    QVariantList m_rosterWantedMatrix;
    QVariantList m_rosterExceptionMatrix;
    QVariantMap m_statistics;
    QVariantMap m_selectedGridSummary;
    QVariantList m_selectedGridLive;
    QVariantList m_selectedGridQsos;
    QStringList m_availableBands {QStringLiteral("All")};
    QStringList m_availableModes {QStringLiteral("All")};
    QStringList m_availableContinents {QStringLiteral("All")};
    QStringList m_availableDxcc {QStringLiteral("All")};
    QStringList m_availableSources {QStringLiteral("All")};
    QString m_bandFilter {QStringLiteral("All")};
    QString m_modeFilter {QStringLiteral("All")};
    QString m_periodFilter {QStringLiteral("All time")};
    QString m_continentFilter {QStringLiteral("All")};
    QString m_dxccFilter {QStringLiteral("All")};
    QString m_sourceFilter {QStringLiteral("All")};
    QString m_propagationFilter {QStringLiteral("MIXED")};
    QString m_rosterSort {QStringLiteral("Time")};
    QString m_rosterStatusFilter {QStringLiteral("All")};
    QString m_rosterHuntScope {QStringLiteral("All time")};
    QString m_rosterScope {QStringLiteral("All bands")};
    QString m_rosterDxccScope {QStringLiteral("All")};
    QString m_activeAwardProgram {QStringLiteral("None")};
    QString m_awardGoal {QStringLiteral("Confirmed")};
    QString m_awardEndorsement;
    QString m_awardConfirmation {QStringLiteral("Any")};
    QString m_awardCallsign;
    QString m_awardFromDate;
    QString m_awardToDate;
    bool m_rosterUsesLoTW {false};
    int m_rosterMaxLoTWDays {810};
    bool m_rosterUsesEQSL {false};
    bool m_rosterUsesOQRS {false};
    bool m_rosterSpottedMeOnly {false};
    bool m_rosterMinSnrEnabled {false};
    int m_rosterMinSnr {-25};
    bool m_rosterMaxDtEnabled {false};
    double m_rosterMaxDt {0.5};
    bool m_rosterTreatRr73AsCq {false};
    QStringList m_rosterWantedTypes {
        QStringLiteral("CALL"), QStringLiteral("GRID"), QStringLiteral("DXCC"),
        QStringLiteral("WPX"), QStringLiteral("POTA"), QStringLiteral("CQ"),
        QStringLiteral("ITU"), QStringLiteral("STATE"), QStringLiteral("COUNTY"),
        QStringLiteral("CONTINENT")};
    QString m_rosterMyCall;
    QString m_rosterMyDxcc;
    QString m_sourcePath;
    QString m_databasePath;
    QString m_selectedGrid;
    bool m_cqOnly {false};
    bool m_rosterSortDescending {true};
    bool m_rosterCqOnly {false};
    QString m_rosterTextFilter;
    QString m_rosterTextMode {QStringLiteral("No filter")};
    bool m_loading {false};
    bool m_offlineMode {false};
    bool m_gridDetailsLoading {false};
    int m_rosterRetentionMinutes {5};
    int m_gridPrecision {4};
    int m_liveDecayMinutes {15};
    QVariantMap m_sourceDecayMinutes {
        {QStringLiteral("decoder"), 15},
        {QStringLiteral("psk"), 60},
        {QStringLiteral("oams"), 30}
    };
    bool m_splitGridEnabled {true};
    bool m_coveragePushPinsEnabled {false};
    bool m_timeZoneOverlayEnabled {false};
    QString m_pskDisplayMode {QStringLiteral("Overlay")};
    int m_pskOpacityPercent {65};
    QString m_spotAgeFilter {QStringLiteral("15 min")};
    QString m_spotCorrelationFilter {QStringLiteral("All")};
    int m_bandActivityWindowHours {6};
    QStringList m_rosterVisibleColumns {
        QStringLiteral("Grid"), QStringLiteral("Grid source"),
        QStringLiteral("Band"), QStringLiteral("Mode"),
        QStringLiteral("SNR"), QStringLiteral("DXCC"), QStringLiteral("Age")};
    QString m_callLookupProvider {QStringLiteral("QRZ")};
    bool m_alertNewGridEnabled {true};
    bool m_alertNewDxccEnabled {true};
    bool m_alertCqEnabled {true};
    QString m_alertCallPattern;
    int m_qsoCount {0};
    int m_workedGridCount {0};
    int m_confirmedGridCount {0};
    int m_activeGridCount {0};
    int m_missingGridCount {0};
    int m_liveSpotCount {0};
    int m_rosterWantedCount {0};
    int m_rosterNewCount {0};
    int m_rosterUnconfirmedCount {0};
    int m_unreadAlertCount {0};
    std::atomic<quint64> m_queryGeneration {0};
    std::atomic<quint64> m_importGeneration {0};
    std::atomic<quint64> m_gridDetailsGeneration {0};
};
