#include "DecodiumBridge.h"
#include "Sequencer/MessageTokenRules.hpp"
#include <QDateTime>
#include <QtMath>
#include <cmath>

QString decodiumBridgeLogPromptDateTimeText(const QDateTime&);

QVariantMap DecodiumBridge::pendingLogQsoPreview() const
{
    if (m_promptLogSnapshotValid) {
        QVariantMap preview;
        preview.insert(QStringLiteral("call"), m_promptLogCall);
        preview.insert(QStringLiteral("grid"), m_promptLogGrid);
        preview.insert(QStringLiteral("sent"), m_promptLogRptSent);
        preview.insert(QStringLiteral("rcvd"), m_promptLogRptRcvd);
        preview.insert(QStringLiteral("freq"), m_promptLogDialFreq);
        preview.insert(QStringLiteral("mode"), !m_logModeOverride.isEmpty() ? m_logModeOverride
                                               : (m_promptLogMode.isEmpty() ? m_mode : m_promptLogMode));
        preview.insert(QStringLiteral("timeOn"), decodiumBridgeLogPromptDateTimeText(m_promptLogOn));
        preview.insert(QStringLiteral("timeOff"), decodiumBridgeLogPromptDateTimeText(m_promptLogOff.isValid() ? m_promptLogOff : m_promptLogOn));
        preview.insert(QStringLiteral("comment"),
                       m_promptLogCommentOverrideValid
                           ? m_promptLogComment
                           : defaultLogCommentForQso(preview.value(QStringLiteral("mode")).toString(),
                                                     m_promptLogRptSent,
                                                     m_promptLogRptRcvd,
                                                     m_promptLogGrid));
        return preview;
    }

    QString logDxCall = m_dxCall.trimmed();
    QString logDxGrid = m_dxGrid.trimmed();
    QString logRptSent = m_reportSent.trimmed();
    QString logRptRcvd = m_reportReceived.trimmed();
    double logFreqHz = m_frequency;
    QDateTime logOnUtc = effectiveQsoLogTimeOnUtc();
    if (!logOnUtc.isValid()) {
        logOnUtc = QDateTime::currentDateTimeUtc();
    }
    QDateTime logOffUtc = logOnUtc;

    if (m_pendingAutoLogValid) {
        if (!m_pendingAutoLogCall.isEmpty()) logDxCall = m_pendingAutoLogCall;
        if (!m_pendingAutoLogGrid.isEmpty()) logDxGrid = m_pendingAutoLogGrid;
        if (!m_pendingAutoLogRptSent.isEmpty()) logRptSent = m_pendingAutoLogRptSent;
        if (!m_pendingAutoLogRptRcvd.isEmpty()) logRptRcvd = m_pendingAutoLogRptRcvd;
        if (m_pendingAutoLogDialFreq > 0.0) logFreqHz = m_pendingAutoLogDialFreq;
        if (m_pendingAutoLogOn.isValid()) logOnUtc = m_pendingAutoLogOn;
        if (m_pendingAutoLogOff.isValid()) logOffUtc = m_pendingAutoLogOff;
    }

    if (logDxCall.isEmpty() && m_lateAutoLogValid
        && m_lateAutoLogExpires >= QDateTime::currentDateTimeUtc()) {
        logDxCall = m_lateAutoLogCall;
        logDxGrid = m_lateAutoLogGrid;
        logRptSent = m_lateAutoLogRptSent;
        logRptRcvd = m_lateAutoLogRptRcvd;
        if (m_lateAutoLogDialFreq > 0.0) logFreqHz = m_lateAutoLogDialFreq;
        if (m_lateAutoLogOn.isValid()) logOnUtc = m_lateAutoLogOn;
        if (m_lateAutoLogOff.isValid()) logOffUtc = m_lateAutoLogOff;
    }

    if (logRptSent.isEmpty()) {
        logRptSent = decodium::seq::signalReportFromMessage(m_lastTransmittedMessage);
    }
    if (logRptSent.isEmpty()) {
        logRptSent = decodium::seq::signalReportFromMessage(buildCurrentTxMessage());
    }

    if (logOffUtc < logOnUtc) {
        logOffUtc = logOnUtc;
    }

    QVariantMap preview;
    preview.insert(QStringLiteral("call"), logDxCall);
    preview.insert(QStringLiteral("grid"), logDxGrid);
    preview.insert(QStringLiteral("sent"), logRptSent);
    preview.insert(QStringLiteral("rcvd"), logRptRcvd);
    preview.insert(QStringLiteral("freq"), logFreqHz);
    preview.insert(QStringLiteral("mode"), m_mode);
    preview.insert(QStringLiteral("timeOn"), decodiumBridgeLogPromptDateTimeText(logOnUtc));
    preview.insert(QStringLiteral("timeOff"), decodiumBridgeLogPromptDateTimeText(logOffUtc));
    preview.insert(QStringLiteral("comment"),
                   m_promptLogCommentOverrideValid
                       ? m_promptLogComment
                       : defaultLogCommentForQso(m_mode, logRptSent, logRptRcvd, logDxGrid));
    return preview;
}

QString DecodiumBridge::defaultLogCommentForQso(const QString& mode,
                                                const QString& rstSent,
                                                const QString& rstRcvd,
                                                const QString& dxGrid) const
{
    if (getSetting(QStringLiteral("dBReportsToComments"), false).toBool()) {
        QString comment = mode.trimmed().toUpper();
        if (!rstSent.trimmed().isEmpty()) {
            comment += QStringLiteral("  Sent: ") + rstSent.trimmed();
        }
        if (!rstRcvd.trimmed().isEmpty()) {
            comment += QStringLiteral("  Rcvd: ") + rstRcvd.trimmed();
        }
        double const distanceKm = calcDistance(m_grid, dxGrid);
        if (std::isfinite(distanceKm) && distanceKm > 0.0) {
            bool const useMiles = getSetting(QStringLiteral("Miles"), false).toBool();
            double const distance = useMiles ? distanceKm * 0.621371192 : distanceKm;
            QString const unit = useMiles ? QStringLiteral("miles") : QStringLiteral("km");
            comment += QStringLiteral("  Distance: %1 %2").arg(qRound(distance)).arg(unit);
        }
        return comment.simplified();
    }

    if (getSetting(QStringLiteral("SpecOpInComments"), false).toBool()) {
        QString const contestName = getSetting(QStringLiteral("Contest_Name"), QString()).toString().trimmed();
        bool const individualContestName =
            getSetting(QStringLiteral("Individual_Contest_Name"), false).toBool();
        switch (m_specialOperationActivity) {
        case 1: return individualContestName && !contestName.isEmpty()
                    ? contestName + QStringLiteral(" Contest")
                    : QStringLiteral("NA VHF Contest");
        case 2: return individualContestName && !contestName.isEmpty()
                    ? contestName + QStringLiteral(" Contest")
                    : QStringLiteral("EU VHF Contest");
        case 3: return individualContestName && !contestName.isEmpty()
                    ? contestName + QStringLiteral(" Contest")
                    : QStringLiteral("ARRL Field Day");
        case 4: return individualContestName && !contestName.isEmpty()
                    ? contestName + QStringLiteral(" Contest")
                    : QStringLiteral("FT Roundup messages");
        case 5: return individualContestName && !contestName.isEmpty()
                    ? contestName + QStringLiteral(" Contest")
                    : QStringLiteral("WW Digi Contest");
        case 6: return getSetting(QStringLiteral("SuperFox"), false).toBool()
                    ? QStringLiteral("SF/H mode")
                    : QStringLiteral("F/H mode");
        case 7: return getSetting(QStringLiteral("SuperFox"), false).toBool()
                    ? QStringLiteral("SF/H mode")
                    : QStringLiteral("F/H mode");
        case 8: return individualContestName && !contestName.isEmpty()
                    ? contestName + QStringLiteral(" Contest")
                    : QStringLiteral("ARRL Digi Contest");
        default:
            break;
        }
    }

    return getSetting(QStringLiteral("LogComments"), QString()).toString().trimmed();
}

