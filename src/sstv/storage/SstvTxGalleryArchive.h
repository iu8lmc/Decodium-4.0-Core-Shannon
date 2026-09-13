// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvImageStorage.h"

#include <QDateTime>
#include <QImage>
#include <QJsonObject>
#include <QString>

#include <optional>

namespace decodium::sstv {

// Immutable native SSTV Studio context captured at the exact moment an image
// becomes a draft or an on-air transmission is accepted.  The storage worker
// owns PNG encoding and validation; this value object deliberately contains
// no path or worker state.
struct SstvTxGalleryArchiveContext final
{
    QString id;
    QDateTime eventAtUtc;
    QString mode;
    QString fskId;
    QString localCallsign;
    QString localGrid;
    QString source;
    bool digital {false};
    qint64 frequencyHz {0};
    // Native analog SSTV occupies the standard 1200--2300 Hz audio range;
    // 1900 Hz is its nominal centre, rather than Decodium's weak-signal TX
    // offset, which does not retune the SSTV encoder.
    qint64 audioFrequencyHz {1'900};
    int sourceSampleRateHz {48'000};
    QJsonObject qualityMetrics;
    QString note;
    QString fileNameTemplate;
};

// Builds the path-free request used by SstvStorageWorker::storeAndInsertImage
// for the two native Studio lifecycle categories.  Invalid images, timestamps,
// modes, or non-Studio categories are rejected before any worker dispatch.
std::optional<SstvImageSaveRequest> makeSstvTxGalleryArchiveRequest(
    const QImage& image,
    SstvImageCategory category,
    const SstvTxGalleryArchiveContext& context);

// Bridge-side lifecycle helpers for the one-Draft-per-preparation invariant.
// They deliberately use plain counters so the UI/bridge can keep state on its
// owner thread without sharing a QObject or storage-worker affinity.
void advanceSstvTxDraftGeneration(quint64& generation,
                                  quint64& queuedGeneration) noexcept;
bool sstvTxDraftNeedsArchive(quint64& generation,
                             quint64 queuedGeneration) noexcept;

} // namespace decodium::sstv
