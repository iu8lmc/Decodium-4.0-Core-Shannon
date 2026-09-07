// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SstvImageStorage.h"

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace decodium::sstv {

enum SstvGalleryCategoryFlag : quint8
{
    SstvGalleryReceived = 1U << 0U,
    SstvGalleryTransmitted = 1U << 1U,
    SstvGalleryImported = 1U << 2U,
    SstvGalleryDraft = 1U << 3U,
    SstvGalleryAllCategories = SstvGalleryReceived
        | SstvGalleryTransmitted | SstvGalleryImported | SstvGalleryDraft
};

enum class SstvGalleryTruthFilter : quint8
{
    Any = 0,
    OnlyTrue = 1,
    OnlyFalse = 2
};

enum class SstvGallerySort : quint8
{
    CapturedNewest = 0,
    CapturedOldest,
    CallsignAscending,
    CallsignDescending,
    ModeAscending,
    ModeDescending,
    FrequencyAscending,
    FrequencyDescending,
    UpdatedNewest,
    UpdatedOldest
};

struct SstvGalleryQuery final
{
    quint8 categoryMask {SstvGalleryAllCategories};
    SstvGalleryTruthFilter remote {SstvGalleryTruthFilter::Any};
    QString mode;
    QString callsign;
    QDateTime capturedFromUtc;
    QDateTime capturedToUtc;
    qint64 minimumFrequencyHz {-1};
    qint64 maximumFrequencyHz {-1};
    QStringList tags;
    bool requireAllTags {false};
    SstvGalleryTruthFilter partial {SstvGalleryTruthFilter::Any};
    // -1 means any; otherwise the integer representation of SstvUploadState.
    int uploadState {-1};
    QString search;
    SstvGallerySort sort {SstvGallerySort::CapturedNewest};
    int limit {50};
    int offset {0};

    bool validate(QString* error = nullptr) const;
};

struct SstvGalleryPage final
{
    QVector<SstvImageRecord> records;
    bool hasMore {false};
    int nextOffset {0};
};

} // namespace decodium::sstv

Q_DECLARE_METATYPE(decodium::sstv::SstvGalleryTruthFilter)
Q_DECLARE_METATYPE(decodium::sstv::SstvGallerySort)
Q_DECLARE_METATYPE(decodium::sstv::SstvGalleryQuery)
Q_DECLARE_METATYPE(decodium::sstv::SstvGalleryPage)
