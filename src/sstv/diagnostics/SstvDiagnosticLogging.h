// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDateTime>
#include <QLoggingCategory>
#include <QMutex>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

Q_DECLARE_LOGGING_CATEGORY(sstvCoreLog)
Q_DECLARE_LOGGING_CATEGORY(sstvRxLog)
Q_DECLARE_LOGGING_CATEGORY(sstvTxLog)
Q_DECLARE_LOGGING_CATEGORY(sstvVisLog)
Q_DECLARE_LOGGING_CATEGORY(sstvSyncLog)
Q_DECLARE_LOGGING_CATEGORY(sstvStorageLog)
Q_DECLARE_LOGGING_CATEGORY(sstvShareLog)
Q_DECLARE_LOGGING_CATEGORY(sstvHamDrmLog)
Q_DECLARE_LOGGING_CATEGORY(sstvSecurityLog)

namespace decodium::sstv {

// All diagnostic data crosses this allowlist before it can enter the bounded
// in-memory event ring or a diagnostic export. Unknown keys and non-scalar
// values are dropped rather than guessed or recursively serialized.
class SstvDiagnosticRedactor final
{
public:
    static QVariantMap capabilities(const QVariantMap& input);
    static QVariantMap settings(const QVariantMap& input);
    static QVariantMap metrics(const QString& section,
                               const QVariantMap& input);
    static QVariantMap eventFields(const QVariantMap& input);
    static bool safeEventName(const QString& value) noexcept;
    static bool containsForbiddenText(const QString& value) noexcept;
};

struct SstvDiagnosticEvent final
{
    quint64 sequence {0};
    QDateTime timestampUtc;
    QString category;
    QString severity;
    QString event;
    QVariantMap fields;

    QVariantMap toVariantMap() const;
};

// Process-wide SSTV-only ring. It is intentionally independent of the normal
// Qt message handler: only explicitly structured, already-redacted events are
// retained. This avoids collecting arbitrary application logs or personal
// metadata while still making transitions useful in an export.
class SstvDiagnosticLogBuffer final
{
public:
    static constexpr int kMaximumEvents = 512;
    static constexpr int kMaximumExportedEvents = 256;

    static SstvDiagnosticLogBuffer& instance();

    void record(const QLoggingCategory& category,
                QtMsgType severity,
                const QString& event,
                const QVariantMap& fields = {});
    QVariantList snapshot(int maximumEvents = kMaximumExportedEvents) const;
    int size() const;
    void clear();

private:
    SstvDiagnosticLogBuffer() = default;

    mutable QMutex m_mutex;
    QVector<SstvDiagnosticEvent> m_events;
    quint64 m_nextSequence {1};
};

void recordSstvDiagnosticEvent(const QLoggingCategory& category,
                               QtMsgType severity,
                               const QString& event,
                               const QVariantMap& fields = {});

} // namespace decodium::sstv
