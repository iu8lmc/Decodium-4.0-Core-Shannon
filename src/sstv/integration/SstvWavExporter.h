// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvTxAudioDevice.h"

#include "../tx/SstvWavStreamWriter.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QJsonObject>
#include <QString>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace decodium::sstv {

enum class SstvWavExportError : std::uint8_t
{
    None,
    InvalidRequest,
    Collision,
    Locked,
    Cancelled,
    SourceFailure,
    WriterFailure,
    CommitFailure,
    IntegrityFailure,
    MetadataFailure,
};

struct SstvWavExportRequest final
{
    QString outputPath;
    QString mode;
    QJsonObject metadata;
    bool writeMetadataSidecar {false};
    bool replaceExisting {false};
    std::size_t pullSamples {16'384U};
};

struct SstvWavExportResult final
{
    bool ok {false};
    SstvWavExportError code {SstvWavExportError::InvalidRequest};
    QString error;
    QString warning;
    QString wavPath;
    QString metadataPath;
    QByteArray sha256;
    qint64 fileSizeBytes {0};
    bool metadataCommitted {false};
    SstvWavStreamWriter::Metrics metrics;
};

// Thread-confined atomic exporter used by the Decodium SSTV Studio worker.
// It consumes the same bounded SstvPcm16Source used by live SoundOutput and
// has no radio/PTT dependency.  Cancellation is the only concurrent input and
// is honoured through the final pre-commit checkpoint; an atomic rename that
// has already succeeded is never deleted as a late cancellation rollback.
// The WAV is the primary transaction.  An optional sidecar is committed by a
// second QSaveFile and reports a bounded warning without invalidating the WAV.
class SstvWavExporter final
{
    Q_DECLARE_TR_FUNCTIONS(SstvWavExporter)

public:
    static constexpr std::size_t MinimumPullSamples = 256U;
    static constexpr std::size_t MaximumPullSamples = 65'536U;
    static constexpr qsizetype MaximumMetadataBytes = 256 * 1'024;
    static constexpr std::size_t MaximumMetadataNodes = 8'192U;
    static constexpr int MaximumMetadataDepth = 64;
    static constexpr int MaximumModeCharacters = 64;
    static constexpr int MaximumErrorCharacters = 1'024;
    static constexpr int MaximumPathCharacters = 4'096;

    SstvWavExporter() = delete;

    static SstvWavExportResult exportAtomic(
        std::unique_ptr<SstvPcm16Source> source,
        const SstvWavExportRequest& request,
        const std::shared_ptr<std::atomic_bool>& cancelRequested = {});

    static QString metadataPathForWav(const QString& wavPath);
};

} // namespace decodium::sstv

Q_DECLARE_METATYPE(decodium::sstv::SstvWavExportError)
Q_DECLARE_METATYPE(decodium::sstv::SstvWavExportResult)
