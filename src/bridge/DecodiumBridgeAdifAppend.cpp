#include "DecodiumBridge.h"
#include "DecodeUiFilterPolicy.h"
#include "MapIntelligenceService.h"
#include "CallsignIntelligenceService.h"
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>

QString bridgeAdifRecordText(const QString& dxCall, const QString& dxGrid,
                                    double freqHz, const QString& mode,
                                    const QDateTime& timeOnUtc,
                                    const QDateTime& timeOffUtc,
                                    const QString& rstSent, const QString& rstRcvd,
                                    const QString& myCall, const QString& myGrid,
                                    const QString& comments = QString(),
                                    const QString& propMode = QString(),
                                    const QString& satellite = QString(),
                                    const QString& satMode = QString(),
                                    const QString& freqRx = QString(),
                                    const QString& name = QString(),
                                    const QString& qth = QString(),
                                    int cqZone = 0,
                                    int ituZone = 0);

bool DecodiumBridge::appendAdifRecord(const QString& dxCall, const QString& dxGrid,
                                       double freqHz, const QString& mode,
                                       const QDateTime& timeOnUtc,
                                       const QDateTime& timeOffUtc,
                                       const QString& rstSent, const QString& rstRcvd,
                                       const QString& comments,
                                       const QString& propMode,
                                       const QString& satellite,
                                       const QString& satMode,
                                       const QString& freqRx,
                                       int cqZone,
                                       int ituZone)
{
    QString const logPath = ensureAdifLogPath();
    QDir().mkpath(QFileInfo(logPath).absolutePath());

    QFile f(logPath);
    bool newFile = !f.exists();
    if (!f.open(QIODevice::Append | QIODevice::Text)) {
        return false;
    }

    QTextStream ts(&f);
    if (newFile) ts << "Decodium3 ADIF Log\n<EOH>\n";

    QString const adifRecord =
        bridgeAdifRecordText(dxCall, dxGrid, freqHz, mode, timeOnUtc, timeOffUtc,
                             rstSent, rstRcvd, m_callsign, m_grid,
                             comments, propMode, satellite, satMode, freqRx,
                             m_nextLogName, m_nextLogQth, cqZone, ituZone)
        + QStringLiteral("<EOR>\n");
    ts << adifRecord;
    ts.flush();
    const bool writeOk = ts.status() == QTextStream::Ok && f.flush();
    if (!writeOk) {
        return false;
    }
    f.close();
    if (m_callsignIntelligence)
        m_callsignIntelligence->enqueueClubLogQso(adifRecord.toUtf8(), m_callsign);

    m_workedCalls.insert(dxCall.toUpper());
    QString const qsoDate = (timeOnUtc.isValid() ? timeOnUtc.toUTC() : QDateTime::currentDateTimeUtc())
                                .date().toString(QStringLiteral("yyyyMMdd"));
    appendWorkedQso(dxCall, dxGrid, freqHz, mode, qsoDate);
    refreshWorkedBeforeDecodeEntriesForCall(dxCall);
    if (m_qsoCountCache >= 0) {
        ++m_qsoCountCache;
    }
    invalidateQsoSearchCache();
    warmLogCacheAsync();
    decodium::decode_ui::WorkedFilterOptions const workedFilters = workedFilterOptions();
    if (workedFilters.hideWorkedBand
        || workedFilters.hideWorkedToday
        || workedFilters.hideWorkedEver) {
        invalidateDecodeUiPredicateCaches();
        rebuildBandActivityModel();
        rebuildRxDecodeModel();
    }
    emit qsoCountChanged();
    emit workedCountChanged();
    if (m_mapIntelligenceService) {
        m_mapIntelligenceService->appendAdifRecord(adifRecord.toUtf8());
    }
    return true;
}

bool DecodiumBridge::openActiveLogbookFolder() const
{
    QFileInfo const info(effectiveAdifLogPath());
    QDir dir = info.exists() ? info.absoluteDir() : QDir(info.absolutePath());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
}

