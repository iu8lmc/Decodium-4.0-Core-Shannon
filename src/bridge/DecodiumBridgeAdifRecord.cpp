#include "AdifExportSanitizer.h"
#include "Radio.hpp"
#include "models/Bands.hpp"
#include <QDateTime>
#include <QVariant>
#include <cmath>
#include <limits>

extern Radio::Frequency decodiumBridgeParseFrequencyHzValue(const QVariant&, bool*);

static QString bandFromFreqHz(double hz) {
    if (hz <= 0.0 || hz > static_cast<double>(std::numeric_limits<Radio::Frequency>::max())) {
        return "OOB";
    }
    Bands bands;
    QString const band = bands.find(static_cast<Radio::Frequency>(std::llround(hz)));
    return band.isEmpty() ? QStringLiteral("OOB") : band.toUpper();
}
QString decodiumBridgeBandFromFreqHz(double hz)
{
    return bandFromFreqHz(hz);
}


static QString bridgeAdifField(const QString& tag, const QString& value)
{
    if (decodium::adif::isInvalidUnsetValue(tag, value)) {
        return {};
    }
    return QStringLiteral("<%1:%2>%3 ")
        .arg(tag)
        .arg(value.toUtf8().size())
        .arg(value);
}

static bool bridgeAdifModeUsesMfskSubmode(QString const& mode)
{
    QString const normalized = mode.trimmed().toUpper();
    return normalized == QStringLiteral("FT2")
        || normalized == QStringLiteral("FT4")
        || normalized == QStringLiteral("FST4")
        || normalized == QStringLiteral("Q65")
        || normalized.startsWith(QStringLiteral("Q65-"));
}

QString bridgeAdifRecordText(const QString& dxCall, const QString& dxGrid,
                                    double freqHz, const QString& mode,
                                    const QDateTime& timeOnUtc,
                                    const QDateTime& timeOffUtc,
                                    const QString& rstSent, const QString& rstRcvd,
                                    const QString& myCall, const QString& myGrid,
                                    const QString& comments,
                                    const QString& propMode,
                                    const QString& satellite,
                                    const QString& satMode,
                                    const QString& freqRx,
                                    const QString& name,
                                    const QString& qth,
                                    int cqZone,
                                    int ituZone)
{
    double const freqMhz = freqHz / 1e6;
    QString const normalizedMode = mode.trimmed().toUpper();
    QDateTime const effectiveTimeOnUtc = timeOnUtc.isValid() ? timeOnUtc : QDateTime::currentDateTimeUtc();
    QDateTime effectiveTimeOffUtc = timeOffUtc.isValid() ? timeOffUtc : effectiveTimeOnUtc;
    if (effectiveTimeOffUtc < effectiveTimeOnUtc) {
        effectiveTimeOffUtc = effectiveTimeOnUtc;
    }
    QString const qsoDate = effectiveTimeOnUtc.toString(QStringLiteral("yyyyMMdd"));
    QString const qsoTime = effectiveTimeOnUtc.toString(QStringLiteral("HHmmss"));
    QString const qsoDateOff = effectiveTimeOffUtc.toString(QStringLiteral("yyyyMMdd"));
    QString const qsoTimeOff = effectiveTimeOffUtc.toString(QStringLiteral("HHmmss"));

    QString record = bridgeAdifField(QStringLiteral("CALL"), dxCall)
                   + bridgeAdifField(QStringLiteral("GRIDSQUARE"), dxGrid);
    if (bridgeAdifModeUsesMfskSubmode(normalizedMode)) {
        record += bridgeAdifField(QStringLiteral("MODE"), QStringLiteral("MFSK"))
                + bridgeAdifField(QStringLiteral("SUBMODE"), normalizedMode);
    } else {
        record += bridgeAdifField(QStringLiteral("MODE"), normalizedMode);
    }
    record += bridgeAdifField(QStringLiteral("RST_SENT"), rstSent.isEmpty() ? QStringLiteral("-10") : rstSent)
            + (rstRcvd.isEmpty() ? QString() : bridgeAdifField(QStringLiteral("RST_RCVD"), rstRcvd))
            + bridgeAdifField(QStringLiteral("QSO_DATE"), qsoDate)
            + bridgeAdifField(QStringLiteral("TIME_ON"), qsoTime)
            + bridgeAdifField(QStringLiteral("QSO_DATE_OFF"), qsoDateOff)
            + bridgeAdifField(QStringLiteral("TIME_OFF"), qsoTimeOff)
            + bridgeAdifField(QStringLiteral("BAND"), bandFromFreqHz(freqHz))
            + bridgeAdifField(QStringLiteral("FREQ"), QString::number(freqMhz, 'f', 6))
            + bridgeAdifField(QStringLiteral("STATION_CALLSIGN"), myCall);
    if (!myGrid.trimmed().isEmpty()) {
        record += bridgeAdifField(QStringLiteral("MY_GRIDSQUARE"), myGrid);
    }
    if (cqZone >= 1 && cqZone <= 40) {
        record += bridgeAdifField(QStringLiteral("CQZ"), QString::number(cqZone));
    }
    if (ituZone >= 1 && ituZone <= 90) {
        record += bridgeAdifField(QStringLiteral("ITUZ"), QString::number(ituZone));
    }
    if (!name.trimmed().isEmpty()) {
        record += bridgeAdifField(QStringLiteral("NAME"), name.trimmed());
    }
    if (!qth.trimmed().isEmpty()) {
        record += bridgeAdifField(QStringLiteral("QTH"), qth.trimmed());
    }
    QString const cleanComments = comments.trimmed();
    if (!cleanComments.isEmpty()) {
        record += bridgeAdifField(QStringLiteral("COMMENT"), cleanComments);
    }

    QString const cleanPropMode = propMode.trimmed();
    QString const cleanSatellite = satellite.trimmed();
    QString const cleanSatMode = satMode.trimmed();
    QString cleanFreqRx;
    QString cleanBandRx;
    bool freqRxOk = false;
    Radio::Frequency const freqRxHz = decodiumBridgeParseFrequencyHzValue(freqRx.trimmed(), &freqRxOk);
    if (freqRxOk && freqRxHz > 0) {
        cleanFreqRx = QString::number(static_cast<double>(freqRxHz) / 1000000.0, 'f', 6);
        QString const candidateBandRx = bandFromFreqHz(static_cast<double>(freqRxHz));
        if (candidateBandRx != QStringLiteral("OOB")) {
            cleanBandRx = candidateBandRx;
        }
    }
    if (!cleanPropMode.isEmpty()) {
        record += bridgeAdifField(QStringLiteral("prop_mode"), cleanPropMode);
    }
    if (!cleanSatellite.isEmpty()) {
        record += bridgeAdifField(QStringLiteral("sat_name"), cleanSatellite);
    }
    if (!cleanSatMode.isEmpty()) {
        record += bridgeAdifField(QStringLiteral("sat_mode"), cleanSatMode);
    }
    if (!cleanBandRx.isEmpty()) {
        record += bridgeAdifField(QStringLiteral("band_rx"), cleanBandRx);
    }
    if (!cleanFreqRx.isEmpty()) {
        record += bridgeAdifField(QStringLiteral("freq_rx"), cleanFreqRx);
    }
    return record;
}

