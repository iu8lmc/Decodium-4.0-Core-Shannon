// SPDX-License-Identifier: GPL-3.0-or-later

#include "SstvGalleryQuery.h"

#include <QSet>

#include <limits>

namespace decodium::sstv {
namespace {

bool fail(QString* error, const QString& detail)
{
    if (error) {
        *error = detail;
    }
    return false;
}

bool validFilterText(const QString& text, qsizetype maximum)
{
    if (text.size() > maximum || text != text.trimmed()) {
        return false;
    }
    for (const QChar character : text) {
        const ushort code = character.unicode();
        if (code == 0U || code < 0x20U || code == 0x7fU) {
            return false;
        }
    }
    return true;
}

bool validTruthFilter(SstvGalleryTruthFilter value)
{
    return value == SstvGalleryTruthFilter::Any
        || value == SstvGalleryTruthFilter::OnlyTrue
        || value == SstvGalleryTruthFilter::OnlyFalse;
}

bool validSort(SstvGallerySort value)
{
    return value >= SstvGallerySort::CapturedNewest
        && value <= SstvGallerySort::UpdatedOldest;
}

} // namespace

bool SstvGalleryQuery::validate(QString* error) const
{
    if (categoryMask == 0U
        || (categoryMask & ~SstvGalleryAllCategories) != 0U) {
        return fail(error, QStringLiteral("invalid gallery category mask"));
    }
    if (!validTruthFilter(remote) || !validTruthFilter(partial)) {
        return fail(error, QStringLiteral("invalid gallery boolean filter"));
    }
    if (!validFilterText(mode, 64) || !validFilterText(callsign, 64)
        || !validFilterText(search, 256)) {
        return fail(error, QStringLiteral("invalid gallery text filter"));
    }
    if ((capturedFromUtc.isValid()
         && capturedFromUtc.timeSpec() != Qt::UTC)
        || (capturedToUtc.isValid()
            && capturedToUtc.timeSpec() != Qt::UTC)
        || (capturedFromUtc.isValid() && capturedToUtc.isValid()
            && capturedFromUtc > capturedToUtc)) {
        return fail(error, QStringLiteral("invalid gallery UTC date range"));
    }
    constexpr qint64 maximumFrequency = 10'000'000'000'000LL;
    if (minimumFrequencyHz < -1 || maximumFrequencyHz < -1
        || minimumFrequencyHz > maximumFrequency
        || maximumFrequencyHz > maximumFrequency
        || (minimumFrequencyHz >= 0 && maximumFrequencyHz >= 0
            && minimumFrequencyHz > maximumFrequencyHz)) {
        return fail(error, QStringLiteral("invalid gallery frequency range"));
    }
    if (tags.size() > 32) {
        return fail(error, QStringLiteral("too many gallery tag filters"));
    }
    QSet<QString> foldedTags;
    for (const QString& tag : tags) {
        const QString canonical = tag.trimmed().normalized(
            QString::NormalizationForm_C);
        if (canonical.isEmpty() || canonical != tag
            || !validFilterText(tag, 64)) {
            return fail(error, QStringLiteral("invalid gallery tag filter"));
        }
        const QString folded = canonical.toCaseFolded();
        if (folded.size() > 64) {
            return fail(error, QStringLiteral(
                "case-folded gallery tag exceeds its length limit"));
        }
        if (foldedTags.contains(folded)) {
            return fail(error, QStringLiteral("duplicate gallery tag filter"));
        }
        foldedTags.insert(folded);
    }
    if (uploadState < -1
        || (uploadState >= 0
            && !isValidSstvUploadState(
                static_cast<SstvUploadState>(uploadState)))) {
        return fail(error, QStringLiteral("invalid gallery upload-state filter"));
    }
    if (!validSort(sort)) {
        return fail(error, QStringLiteral("invalid gallery sort order"));
    }
    if (limit <= 0 || limit > 200) {
        return fail(error, QStringLiteral("gallery page size must be between 1 and 200"));
    }
    if (offset < 0 || offset > 10'000'000
        || offset > std::numeric_limits<int>::max() - limit - 1) {
        return fail(error, QStringLiteral("invalid gallery page offset"));
    }
    return true;
}

} // namespace decodium::sstv
