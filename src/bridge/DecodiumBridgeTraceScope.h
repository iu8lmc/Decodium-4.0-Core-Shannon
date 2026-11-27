#pragma once
#include <QElapsedTimer>
#include <QString>
#include <utility>

void decodiumBridgeMainThreadTimelineLog(const QString&, qint64, const QString&);
static constexpr qint64 kMainThreadTraceDefaultThresholdMs = 40;

class MainThreadTraceScope
{
public:
    explicit MainThreadTraceScope(QString label,
                                  QString details = {},
                                  qint64 thresholdMs = kMainThreadTraceDefaultThresholdMs)
        : m_label(std::move(label)),
          m_details(std::move(details)),
          m_thresholdMs(thresholdMs)
    {
        m_timer.start();
    }

    ~MainThreadTraceScope()
    {
        if (m_cancelled) {
            return;
        }
        qint64 const elapsedMs = m_timer.elapsed();
        if (elapsedMs >= m_thresholdMs) {
            decodiumBridgeMainThreadTimelineLog(m_label, elapsedMs, m_details);
        }
    }

    void addDetail(const QString& detail)
    {
        QString const trimmed = detail.trimmed();
        if (trimmed.isEmpty()) {
            return;
        }
        if (!m_details.isEmpty()) {
            m_details += QLatin1Char(' ');
        }
        m_details += trimmed;
    }

    void cancel()
    {
        m_cancelled = true;
    }

private:
    QElapsedTimer m_timer;
    QString m_label;
    QString m_details;
    qint64 m_thresholdMs;
    bool m_cancelled = false;
};

