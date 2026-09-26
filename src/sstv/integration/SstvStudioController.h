// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvWavExporter.h"

#include "../tx/SstvImagePreprocessor.h"

#include <QFutureWatcher>
#include <QElapsedTimer>
#include <QImage>
#include <QMap>
#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>

#include <cstdint>
#include <atomic>
#include <memory>
#include <mutex>

class QSettings;

namespace decodium::sstv {

struct SstvStudioLoopbackResult final
{
    QImage image;
    QVariantMap metrics;
    QString error;
    bool cancelled {false};
};

struct SstvStudioLoadResult final
{
    QImage image;
    QString sourceName;
    QString error;
};

// Owner-thread facade for Decodium's QML transmit studio.  Image decoding and
// preparation run in bounded QtConcurrent jobs; QML only observes immutable
// snapshots through the application's asynchronous image provider.
class SstvStudioController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool sourceReady READ sourceReady NOTIFY sourceChanged)
    Q_PROPERTY(bool preparedReady READ preparedReady NOTIFY preparedChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY sourceChanged)
    Q_PROPERTY(QString sourceImageSource READ sourceImageSource NOTIFY sourceChanged)
    Q_PROPERTY(QString preparedImageSource READ preparedImageSource NOTIFY preparedChanged)
    Q_PROPERTY(QString modeId READ modeId WRITE setModeId NOTIFY modeChanged)
    Q_PROPERTY(QString modeName READ modeName NOTIFY modeChanged)
    Q_PROPERTY(QSize outputSize READ outputSize NOTIFY modeChanged)
    Q_PROPERTY(double estimatedDurationSeconds READ estimatedDurationSeconds NOTIFY modeChanged)
    Q_PROPERTY(QVariantList modes READ modes CONSTANT)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY preparedChanged)
    Q_PROPERTY(bool wavExportBusy READ wavExportBusy NOTIFY wavExportChanged)
    Q_PROPERTY(QUrl wavExportFolder READ wavExportFolder NOTIFY wavExportChanged)
    Q_PROPERTY(QVariantList wavSampleRates READ wavSampleRates CONSTANT)
    Q_PROPERTY(QString wavExportPath READ wavExportPath NOTIFY wavExportChanged)
    Q_PROPERTY(QString wavExportWarning READ wavExportWarning NOTIFY wavExportChanged)
    Q_PROPERTY(QVariantList templates READ templates NOTIFY templatesChanged)
    Q_PROPERTY(bool loopbackBusy READ loopbackBusy NOTIFY loopbackChanged)
    Q_PROPERTY(bool loopbackReady READ loopbackReady NOTIFY loopbackChanged)
    Q_PROPERTY(QString loopbackImageSource READ loopbackImageSource NOTIFY loopbackChanged)
    Q_PROPERTY(QString loopbackState READ loopbackState NOTIFY loopbackChanged)
    Q_PROPERTY(QString loopbackError READ loopbackError NOTIFY loopbackChanged)
    Q_PROPERTY(double loopbackProgress READ loopbackProgress NOTIFY loopbackChanged)
    Q_PROPERTY(QVariantMap loopbackMetrics READ loopbackMetrics NOTIFY loopbackChanged)

public:
    static constexpr qsizetype MaximumTemplates = 32;
    static constexpr qsizetype MaximumTemplateNameCharacters = 64;
    static constexpr qsizetype MaximumTemplateBytes = 32 * 1'024;
    static constexpr qsizetype MaximumTemplateCatalogBytes = 256 * 1'024;

    explicit SstvStudioController(QObject* parent = nullptr);
    explicit SstvStudioController(const QString& templateSettingsFile,
                                  QObject* parent = nullptr);
    ~SstvStudioController() override;

    SstvStudioController(const SstvStudioController&) = delete;
    SstvStudioController& operator=(const SstvStudioController&) = delete;

    bool busy() const noexcept;
    bool sourceReady() const noexcept;
    bool preparedReady() const noexcept;
    QString sourceName() const;
    QString sourceImageSource() const;
    QString preparedImageSource() const;
    QString modeId() const;
    QString modeName() const;
    QSize outputSize() const;
    double estimatedDurationSeconds() const noexcept;
    QVariantList modes() const;
    QString error() const;
    QStringList warnings() const;
    bool wavExportBusy() const noexcept;
    QUrl wavExportFolder() const;
    QVariantList wavSampleRates() const;
    QString wavExportPath() const;
    QString wavExportWarning() const;
    QVariantList templates() const;
    bool loopbackBusy() const noexcept;
    bool loopbackReady() const noexcept;
    QString loopbackImageSource() const;
    QString loopbackState() const;
    QString loopbackError() const;
    double loopbackProgress() const noexcept;
    QVariantMap loopbackMetrics() const;

    void setModeId(const QString& id);
    void setWavExportRoot(const QString& absolutePath);

    Q_INVOKABLE bool loadSource(const QUrl& localFile);
    Q_INVOKABLE bool pasteSource();
    Q_INVOKABLE bool generateCalibrationPattern();
    Q_INVOKABLE bool prepareImage(const QVariantMap& controls = {});
    Q_INVOKABLE QUrl suggestedWavUrl(const QString& callsign = {}) const;
    Q_INVOKABLE bool exportWav(const QUrl& destination,
                               int sampleRate,
                               bool writeMetadataSidecar,
                               bool replaceExisting,
                               const QString& fskId = {});
    Q_INVOKABLE bool saveTemplate(const QString& name,
                                  const QVariantMap& controls);
    Q_INVOKABLE bool deleteTemplate(const QString& name);
    Q_INVOKABLE QVariantMap templateDefinition(const QString& name) const;
    Q_INVOKABLE bool startLoopback();
    Q_INVOKABLE void cancelLoopback();
    Q_INVOKABLE void clearSource();
    Q_INVOKABLE void cancelWork();

    std::shared_ptr<const QImage> sourceSnapshot() const noexcept;
    std::shared_ptr<const QImage> preparedSnapshot() const noexcept;
    std::shared_ptr<const QImage> loopbackSnapshot() const noexcept;

signals:
    void stateChanged();
    void sourceChanged();
    void preparedChanged();
    void modeChanged();
    void wavExportChanged();
    void templatesChanged();
    void loopbackChanged();

private:
    struct ModeDescriptor final
    {
        QString id;
        QString name;
        QSize size;
        double durationSeconds {0.0};
    };

    static const QList<ModeDescriptor>& executableModes();
    static const ModeDescriptor* findMode(const QString& id);
    static bool parsePreparation(const QVariantMap& controls,
                                 const QSize& outputSize,
                                 SstvImagePreparation& preparation,
                                 QString& error);
    static bool boundedImage(const QImage& image) noexcept;
    static bool supportedWavSampleRate(int sampleRate) noexcept;
    static QString wavFileToken(const QString& value,
                                const QString& fallback);
    static bool validateTemplateControls(const QVariantMap& controls,
                                         const QSize& outputSize,
                                         QString& error);
    static SstvStudioLoopbackResult runLoopback(
        std::shared_ptr<const QImage> prepared,
        QString modeId,
        std::shared_ptr<std::atomic_bool> cancel,
        std::shared_ptr<std::atomic<std::uint64_t>> produced,
        std::shared_ptr<std::atomic<std::uint64_t>> total);

    bool acceptSource(QImage image, QString sourceName);
    void setError(QString value);
    void setBusy(bool value);
    void finishLoad();
    void finishPreparation();
    void finishWavExport();
    void finishLoopback();
    void clearPrepared();
    void loadTemplates();
    bool persistTemplates();
    void initialise();

    mutable std::mutex m_imageMutex;
    std::shared_ptr<const QImage> m_source;
    std::shared_ptr<const QImage> m_prepared;
    std::shared_ptr<const QImage> m_loopback;
    std::uint64_t m_sourceRevision {0U};
    std::uint64_t m_preparedRevision {0U};
    std::uint64_t m_loopbackRevision {0U};

    QFutureWatcher<SstvStudioLoadResult> m_loadWatcher;
    QFutureWatcher<SstvPreparedImage> m_preparationWatcher;
    QFutureWatcher<SstvWavExportResult> m_wavExportWatcher;
    QFutureWatcher<SstvStudioLoopbackResult> m_loopbackWatcher;
    std::shared_ptr<std::atomic_bool> m_wavExportCancel;
    std::shared_ptr<std::atomic_bool> m_loopbackCancel;
    std::shared_ptr<std::atomic<std::uint64_t>> m_loopbackProduced;
    std::shared_ptr<std::atomic<std::uint64_t>> m_loopbackTotal;
    QTimer m_loopbackProgressTimer;
    QElapsedTimer m_loopbackDiagnosticElapsed;
    std::unique_ptr<QSettings> m_templateSettings;
    QMap<QString, QVariantMap> m_templates;
    QString m_sourceName;
    QString m_modeId {QStringLiteral("martin-m1")};
    QString m_error;
    QStringList m_warnings;
    QString m_wavExportRoot;
    QString m_wavExportPath;
    QString m_wavExportWarning;
    QString m_loopbackState {QStringLiteral("Idle")};
    QString m_loopbackError;
    QVariantMap m_loopbackMetrics;
    bool m_busy {false};
    bool m_wavExportBusy {false};
    bool m_loopbackBusy {false};
    bool m_discardPendingResult {false};
    bool m_discardWavResult {false};
    bool m_discardLoopbackResult {false};
};

} // namespace decodium::sstv
