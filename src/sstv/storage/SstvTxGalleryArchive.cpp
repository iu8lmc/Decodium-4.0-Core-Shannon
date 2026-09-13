// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvTxGalleryArchive.h"

#include <QUuid>

#include <limits>

namespace decodium::sstv {
namespace {

QString defaultSource(SstvImageCategory)
{
    return QStringLiteral("sstv-studio");
}

QString defaultNote(SstvImageCategory category)
{
    return category == SstvImageCategory::Draft
        ? QStringLiteral("Native SSTV Studio image prepared")
        : QStringLiteral("Native SSTV Studio transmission accepted");
}

} // namespace

std::optional<SstvImageSaveRequest> makeSstvTxGalleryArchiveRequest(
    const QImage& image,
    SstvImageCategory category,
    const SstvTxGalleryArchiveContext& context)
{
    if (image.isNull()
        || (category != SstvImageCategory::Draft
            && category != SstvImageCategory::Transmitted)
        || !context.eventAtUtc.isValid()
        || context.mode.trimmed().isEmpty()) {
        return std::nullopt;
    }

    SstvImageSaveRequest request;
    request.image = image;
    request.record.id = context.id.trimmed().toLower();
    if (request.record.id.isEmpty()) {
        request.record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    request.record.category = category;
    request.record.capturedAtUtc = context.eventAtUtc.toUTC();
    request.record.eventAtUtc = request.record.capturedAtUtc;
    request.record.mode = context.mode.trimmed();
    request.record.fskId = context.fskId.trimmed().toUpper();
    request.record.localCallsign = context.localCallsign.trimmed().toUpper();
    request.record.localGrid = context.localGrid.trimmed().toUpper();
    request.record.source = context.source.trimmed();
    if (request.record.source.isEmpty()) {
        request.record.source = defaultSource(category);
    }
    request.record.frequencyHz = context.frequencyHz;
    request.record.audioFrequencyHz = context.audioFrequencyHz;
    request.record.sourceSampleRateHz = context.sourceSampleRateHz;
    request.record.digital = context.digital;
    request.record.completionPercent = 100;
    request.record.complete = true;
    request.record.qualityMetrics = context.qualityMetrics;
    request.record.remote = false;
    request.record.note = context.note.trimmed();
    if (request.record.note.isEmpty()) {
        request.record.note = defaultNote(category);
    }
    request.fileNameTemplate = context.fileNameTemplate.trimmed();
    if (request.fileNameTemplate.isEmpty()) {
        request.fileNameTemplate = QStringLiteral(
            "{date}_{time}_{mode}_{remoteCall}_{id}");
    }
    return request;
}

void advanceSstvTxDraftGeneration(quint64& generation,
                                  quint64& queuedGeneration) noexcept
{
    if (generation == std::numeric_limits<quint64>::max()) {
        generation = 1U;
        queuedGeneration = 0U;
        return;
    }
    ++generation;
}

bool sstvTxDraftNeedsArchive(quint64& generation,
                             quint64 queuedGeneration) noexcept
{
    if (generation == 0U) {
        generation = 1U;
    }
    return queuedGeneration != generation;
}

} // namespace decodium::sstv
