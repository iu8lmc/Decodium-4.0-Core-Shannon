// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvRxRuntime.h"
#include "SstvWavExporter.h"

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <QUrl>

#include <atomic>
#include <cstdint>
#include <memory>

namespace decodium::sstv {

// Owner-thread facade for the two expensive retained-audio operations used by
// Receive: preparing a private WAV for a same-runtime re-decode, and exporting
// a diagnostic WAV selected by the user. Ring snapshots, float conversion and
// filesystem I/O run on QtConcurrent; the GUI and audio callback only exchange
// bounded settings/status values.
class SstvRxAudioJobController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString stateName READ stateName NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(QString lastOutputPath READ lastOutputPath NOTIFY stateChanged)

public:
    enum class State : std::uint8_t
    {
        Idle,
        PreparingRedecode,
        ExportingRawAudio,
        Completed,
        Cancelled,
        Error,
        Shutdown,
    };
    Q_ENUM(State)

    explicit SstvRxAudioJobController(SstvRxRuntime* runtime,
                                      QObject* parent = nullptr);
    ~SstvRxAudioJobController() override;

    SstvRxAudioJobController(const SstvRxAudioJobController&) = delete;
    SstvRxAudioJobController& operator=(
        const SstvRxAudioJobController&) = delete;

    bool busy() const noexcept;
    State state() const noexcept;
    QString stateName() const;
    QString lastError() const;
    QString lastOutputPath() const;
    std::uint64_t lastAcquisitionId() const noexcept;
    SstvRxRedecodeParameters preparedRedecodeParameters() const;

    bool prepareRecentRedecode(SstvRxRedecodeParameters parameters);
    bool exportRawAudio(const QUrl& destination,
                        std::uint64_t acquisitionId = 0U);
    void discardPreparedRedecode();
    Q_INVOKABLE void cancel();
    void shutdown();

Q_SIGNALS:
    void stateChanged();
    void redecodePrepared(bool ok, QUrl privateWav, QString error);
    void rawAudioExportFinished(bool ok,
                                QString wavPath,
                                quint64 acquisitionId,
                                QString error);

private Q_SLOTS:
    void finishJob();

private:
    enum class Operation : std::uint8_t
    {
        None,
        Redecode,
        RawExport,
    };

    struct JobResult final
    {
        Operation operation {Operation::None};
        bool ok {false};
        bool cancelled {false};
        QString path;
        QString error;
        std::uint64_t acquisitionId {0U};
        SstvRxRedecodeParameters parameters;
    };

    static JobResult runJob(
        SstvRxRuntime* runtime,
        Operation operation,
        QString outputPath,
        std::uint64_t acquisitionId,
        SstvRxRedecodeParameters parameters,
        std::shared_ptr<std::atomic_bool> cancelRequested) noexcept;
    static QString displayName(State state);
    bool isOnOwnerThread() const noexcept;
    void setState(State state, QString error = {});
    bool startJob(Operation operation,
                  QString outputPath,
                  std::uint64_t acquisitionId,
                  SstvRxRedecodeParameters parameters);

    SstvRxRuntime* const m_runtime;
    QFutureWatcher<JobResult> m_watcher;
    std::unique_ptr<QTemporaryDir> m_temporaryDirectory;
    std::shared_ptr<std::atomic_bool> m_cancelRequested;
    State m_state {State::Idle};
    Operation m_operation {Operation::None};
    QString m_lastError;
    QString m_lastOutputPath;
    std::uint64_t m_lastAcquisitionId {0U};
    SstvRxRedecodeParameters m_preparedParameters;
};

} // namespace decodium::sstv
