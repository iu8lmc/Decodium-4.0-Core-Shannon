// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVariantMap>

namespace decodium::sstv {

// Data accepted by the native SSTV log workflow.  Deliberately no image or
// audio path is part of this contract: attachments are associated through the
// Gallery record ID and remain in Decodium's local SQLite store.
struct SstvQsoLogRequest final
{
    QString imageRecordId;
    bool createNewQso {true};
    QString existingQsoId;
    QString remoteCallsign;
    QString remoteGrid;
    qint64 frequencyHz {0};
    QDateTime timeOnUtc;
    QDateTime timeOffUtc;
    QString reportSent;
    QString reportReceived;
    QString comments;
    QString imageMode;

    bool validate(QString* error = nullptr) const;
};

struct SstvAdifValidationResult final
{
    bool ok {false};
    QString associationId;
    QMap<QString, QString> fields;
    QString error;
};

// SSTV-specific guard around Decodium's native ADIF writer.  ADIF 3.1.6
// represents an SSTV contact as MODE=SSTV and currently defines no SSTV
// SUBMODE.  This validator is intentionally applied to the final serialized
// record, so a later bridge regression cannot silently export a local path or
// invent a mode field merely because the input request was valid.
class SstvQsoLog final
{
public:
    static constexpr qsizetype kMaximumAdifBytes = 256 * 1024;
    static constexpr qsizetype kMaximumFieldBytes = 64 * 1024;
    static constexpr int kMaximumFields = 128;

    static SstvAdifValidationResult validateGeneratedAdif(
        QStringView record,
        const QStringList& forbiddenLocalTokens = {});

    // Strict path-free QML/Bridge boundary. Unknown keys are rejected and
    // new-QSO timestamps must be explicit ISO-8601 UTC strings ending in Z.
    // This keeps QVariant coercion rules out of DecodiumBridge and makes the
    // exact public logging contract independently testable.
    static bool requestFromVariantMap(
        const QVariantMap& values,
        SstvQsoLogRequest* request,
        QString* error = nullptr);

    static QString associationIdForFields(
        const QMap<QString, QString>& fields,
        QString* error = nullptr);

    static QString associationIdForExistingQso(
        QStringView callsign,
        QStringView dateTimeUtc,
        QStringView mode,
        QStringView band,
        QString* error = nullptr);

    static QString mergedComment(QStringView operatorComment,
                                 QStringView imageMode,
                                 QString* error = nullptr);

    // Shared by request validation and final serialized-record validation.
    // Public for deterministic boundary tests; it performs no filesystem I/O.
    static bool containsPathLikeValue(QStringView value) noexcept;
};

} // namespace decodium::sstv
