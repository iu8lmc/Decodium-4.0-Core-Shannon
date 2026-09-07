#pragma once

#include <QObject>
#include <QByteArray>
#include <QDateTime>
#include <QPalette>
#include <QString>
#include <QStringList>
#include <memory>

class MainWindow;
class MultiSettings;
class QApplication;

class DecodiumLegacyBackend final : public QObject
{
    Q_OBJECT

public:
    explicit DecodiumLegacyBackend(QObject* parent = nullptr);
    ~DecodiumLegacyBackend() override;

    bool available() const { return m_available; }
    QString failureReason() const { return m_failureReason; }

    QString callsign() const;
    QString grid() const;
    QString mode() const;
    int trPeriodSeconds() const;
    QString rigName() const;
    double dialFrequency() const;
    int rxFrequency() const;
    int txFrequency() const;
    QString audioInputDeviceName() const;
    QString audioOutputDeviceName() const;
    int audioInputChannel() const;
    int audioOutputChannel() const;
    int rxInputLevel() const;
    QString waterfallPalette() const;
    bool alt12Enabled() const;
    bool txFirst() const;
    bool monitoring() const;
    bool monitoringStartPending() const { return m_startupMonitorEnablePending; }
    bool txEnabled() const;
    bool transmitting() const;
    bool tuning() const;
    bool catConnected() const;
    bool rigControlEnabled() const { return m_rigControlEnabled; }
    double signalLevel() const;
    int bandActivityRevision() const;
    QStringList bandActivityLines() const;
    QStringList takeBandActivityDelta(bool* reset);
    int rxFrequencyRevision() const;
    QStringList rxFrequencyLines() const;
    QStringList takeRxFrequencyDelta(bool* reset);
    QString txMessage(int index) const;
    int currentTx() const;
    QString adifLogPath() const;
    QString allTxtPath() const;
    int txOutputAttenuation() const;
    int specialOperationActivity() const;
    bool superFoxEnabled() const;
    QStringList foxCallerQueueLines() const;

    // The embedded widget backend has its own legacy configuration store.  It
    // must never use that store as the station-identity authority while it is
    // hosted by the QML application, otherwise a mode change can regenerate
    // messages with a callsign from an unrelated legacy profile.
    void setStationIdentity(const QString& callsign, const QString& grid);
    void setMode(const QString& mode);
    void setFt2DecodeEnabled(bool enabled);
    void setDialFrequency(double frequencyHz);
    void setMonitoring(bool enabled);
    void rearmMonitoring(const QString& reason);
    void setAutoSeq(bool enabled);
    void setTxEnabled(bool enabled);
    void setHoldTxFreq(bool enabled);
    bool holdTxFreq() const;
    void setTxWatchdogMinutes(int minutes);
    void setAutoCq(bool enabled);
    void setDecodeDepthBits(int bits);
    void setFt8DeepThreadPenalty(bool enabled);
    void setCqOnly(bool enabled);
    void setRxFrequency(int frequencyHz);
    void setTxFrequency(int frequencyHz);
    void setSplitMode(const QString& mode);
    void setRigControlEnabled(bool enabled);
    void setRigPtt(bool enabled);
    void setAudioInputDeviceName(const QString& name);
    void setAudioOutputDeviceName(const QString& name);
    void setAudioInputChannel(int channel);
    void setAudioOutputChannel(int channel);
    void setRxInputLevel(int value);
    void setTxOutputAttenuation(int value);
    void setDxCall(const QString& call);
    void setDxGrid(const QString& grid);
    void setTxMessage(int index, const QString& message);
    void selectTxMessage(int index);
    void generateStandardMessages();
    void startTune(bool enabled);
    void stopTransmission();
    void armCurrentTx();
    void logQso();
    void setAutoSpotEnabled(bool enabled);
    void setNextLogClusterSpotState(bool available, bool checked);
    void setNextLogPromptAlreadyAccepted();
    void setNextLogPromptFields(const QString& comment,
                                bool commentValid,
                                const QString& propMode,
                                const QString& satellite,
                                const QString& satMode,
                                bool satelliteValid);
    void setNextLogPromptTimes(const QDateTime& timeOnUtc,
                               const QDateTime& timeOffUtc,
                               bool valid);
    void clearBandActivity();
    void clearRxFrequency();
    void setWaterfallPalette(const QString& palette);
    void openSettings(int tabIndex = -1);
    void openTimeSyncSettings();
    void retryRigConnection();
    void setAlt12Enabled(bool enabled);
    void setTxFirst(bool enabled);
    void setSpecialOperationActivity(int activity);
    void setSuperFoxEnabled(bool enabled);
    void setEmbeddedUiUpdatesEnabled(bool enabled);
    void refreshUdpReporting();

Q_SIGNALS:
    void waterfallRowReady(QByteArray const& rowLevels,
                           int startFrequencyHz,
                           int spanHz,
                           int rxFrequencyHz,
                           int txFrequencyHz,
                           QString const& mode) const;
    // First producer-boundary relay for bounded DirectConnection consumers.
    void audioSamplesProduced(QByteArray const& pcmSamples) const;
    void audioSamplesReady(QByteArray const& pcmSamples) const;
    void preferencesRequested() const;
    void quitRequested() const;
    void warningRaised(QString const& title,
                       QString const& summary,
                       QString const& details) const;
    void rigErrorRaised(QString const& title,
                        QString const& summary,
                        QString const& details) const;
    void rigPttRequested(bool enabled) const;
    void adifLogged(QByteArray const& adif) const;

private:
    void applyEmbeddedWidgetTheme();
    void restoreEmbeddedWidgetTheme();
    void restoreApplicationIdentity();

    QApplication* m_app {nullptr};
    QString m_originalApplicationName;
    QString m_originalOrganizationName;
    QString m_originalOrganizationDomain;
    QString m_originalApplicationVersion;
    QString m_originalStyleSheet;
    QPalette m_originalPalette;
    bool m_originalQuitOnLastWindowClosed {true};
    bool m_available {false};
    QString m_failureReason;
    bool m_rigControlEnabled {true};
    bool m_monitoringControlClaimed {false};
    qint64 m_backendStartupMs {0};
    bool m_startupMonitorRequested {false};
    bool m_startupMonitorEnablePending {false};
    std::unique_ptr<MultiSettings> m_multiSettings;
    MainWindow* m_mainWindow {nullptr};
};
