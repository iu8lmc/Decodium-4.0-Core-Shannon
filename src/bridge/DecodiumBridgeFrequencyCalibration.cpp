#include "DecodiumBridge.h"
#include "Radio.hpp"
#include "models/Bands.hpp"
#include "models/StationList.hpp"
#include <cmath>
#include <limits>

extern void decodiumBridgeLogbookLog(const QString&);

double DecodiumBridge::frequencyCalibrationSlopePpm() const
{
    bool ok = false;
    double const value = readSettingFromLegacyIni(QStringLiteral("CalibrationSlopePPM")).toDouble(&ok);
    return ok ? qBound(-200.0, value, 200.0) : 0.0;
}

double DecodiumBridge::frequencyCalibrationInterceptHz() const
{
    bool ok = false;
    double const value = readSettingFromLegacyIni(QStringLiteral("CalibrationIntercept")).toDouble(&ok);
    return ok ? qBound(-1000.0, value, 1000.0) : 0.0;
}

void DecodiumBridge::setFrequencyCalibrationSlopePpm(double value)
{
    value = qBound(-200.0, value, 200.0);
    syncSettingToLegacyIni(QStringLiteral("CalibrationSlopePPM"), value);
    emit settingValueChanged(QStringLiteral("CalibrationSlopePPM"), value);
}

void DecodiumBridge::setFrequencyCalibrationInterceptHz(double value)
{
    value = qBound(-1000.0, value, 1000.0);
    syncSettingToLegacyIni(QStringLiteral("CalibrationIntercept"), value);
    emit settingValueChanged(QStringLiteral("CalibrationIntercept"), value);
}

// 1.0.194 — Station Frequency Offset apply: per-band offset (additivo)
// dalle Station Frequencies (antenna offsets, sub-band correction).
// Lookup banda da freq via Bands::find, poi cerca record matching nella
// tabella `stations` (QSettings legacy ini). 0 se OOB o no match.
double DecodiumBridge::stationOffsetForFrequencyHz(double hz) const
{
    Bands bands;
    QString const bandName = bands.find(static_cast<Radio::Frequency>(hz));
    if (bandName.isEmpty()) return 0.0;
    QVariant const raw = readSettingFromLegacyIni(QStringLiteral("stations"));
    if (!raw.isValid()) return 0.0;
    StationList::Stations const stations = raw.value<StationList::Stations>();
    for (StationList::Station const& s : stations) {
        if (s.band_name_ == bandName) {
            return static_cast<double>(s.offset_);
        }
    }
    return 0.0;
}

double DecodiumBridge::logicalFrequencyFromCatDial(
    double rigHz,
    QString* resolvedBand,
    qint64* resolvedOffsetHz) const
{
    if (resolvedBand) {
        resolvedBand->clear();
    }
    if (resolvedOffsetHz) {
        *resolvedOffsetHz = 0;
    }
    if (rigHz <= 0.0) {
        return 0.0;
    }

    double const scale = 1.0 + frequencyCalibrationSlopePpm() * 1e-6;
    if (qFuzzyIsNull(scale)) {
        return 0.0;
    }
    double const uncalibratedRigHz =
        (rigHz - frequencyCalibrationInterceptHz()) / scale;
    if (uncalibratedRigHz <= 0.0) {
        return 0.0;
    }

    Bands bands;
    auto acceptCandidate = [&](QString const& expectedBand,
                               Radio::FrequencyDelta offsetHz,
                               double* logicalHzOut) {
        double const logicalHz = uncalibratedRigHz - static_cast<double>(offsetHz);
        if (logicalHz <= 0.0
            || logicalHz > static_cast<double>(std::numeric_limits<Radio::Frequency>::max())) {
            return false;
        }
        QString const candidateBand =
            bands.find(static_cast<Radio::Frequency>(std::llround(logicalHz)));
        if (candidateBand.isEmpty()
            || (!expectedBand.isEmpty()
                && candidateBand.compare(expectedBand, Qt::CaseInsensitive) != 0)) {
            return false;
        }
        if (logicalHzOut) {
            *logicalHzOut = logicalHz;
        }
        if (resolvedBand) {
            *resolvedBand = candidateBand;
        }
        if (resolvedOffsetHz) {
            *resolvedOffsetHz = offsetHz;
        }
        return true;
    };

    // Preserve the active transverter mapping while the operator tunes within
    // the same logical band.  This makes CAT polling a true inverse of the
    // preceding local QSY instead of allowing the IF frequency to overwrite
    // the displayed on-air frequency.
    double logicalHz = 0.0;
    if (!m_currentCatLogicalBand.isEmpty()
        && acceptCandidate(m_currentCatLogicalBand,
                           m_currentCatStationOffsetHz,
                           &logicalHz)) {
        return logicalHz;
    }

    // On startup there may be no active mapping yet.  Infer one only when a
    // configured station offset maps the physical rig dial into that exact
    // ADIF band; otherwise the report is an ordinary non-transverter dial.
    QVariant const raw = readSettingFromLegacyIni(QStringLiteral("stations"));
    StationList::Stations const stations =
        raw.isValid() ? raw.value<StationList::Stations>() : StationList::Stations {};
    for (StationList::Station const& station : stations) {
        if (acceptCandidate(station.band_name_, station.offset_, &logicalHz)) {
            return logicalHz;
        }
    }

    if (uncalibratedRigHz > static_cast<double>(std::numeric_limits<Radio::Frequency>::max())) {
        return 0.0;
    }
    if (resolvedBand) {
        *resolvedBand = bands.find(
            static_cast<Radio::Frequency>(std::llround(uncalibratedRigHz)));
    }
    return uncalibratedRigHz;
}

// 1.0.192/194 — Completamento Frequency Calibration + Station Offset chain:
//   step 1: applica per-band station offset (antenna/sub-band correction)
//   step 2: applica global slope+intercept (drift conoscuto rig)
// Formula finale:
//   hz_corrected = (hz + station_offset_for_band(hz))
//                + slope_ppm * (hz + station_offset) * 1e-6 + intercept_Hz
// Skip path se tutto 0 (no-op overhead minimo).
double DecodiumBridge::applyFrequencyCalibration(double hz) const
{
    double const slope = frequencyCalibrationSlopePpm();
    double const intercept = frequencyCalibrationInterceptHz();
    double const stationOffset = stationOffsetForFrequencyHz(hz);
    if (qFuzzyIsNull(slope) && qFuzzyIsNull(intercept) && qFuzzyIsNull(stationOffset)) {
        return hz;
    }
    double const withStation = hz + stationOffset;
    double const corrected = withStation + slope * withStation * 1e-6 + intercept;
    // Log solo se delta significativo (>0.5 Hz) per evitare spam su slope ~0
    if (qAbs(corrected - hz) > 0.5) {
        decodiumBridgeLogbookLog(QStringLiteral("[CAT-CAL] slope=%1ppm intercept=%2Hz "
                                 "stationOffset=%3Hz: requested=%4 → corrected=%5 (delta=%6Hz)")
                      .arg(slope, 0, 'f', 3)
                      .arg(intercept, 0, 'f', 1)
                      .arg(stationOffset, 0, 'f', 1)
                      .arg(hz, 0, 'f', 1)
                      .arg(corrected, 0, 'f', 1)
                      .arg(corrected - hz, 0, 'f', 2));
    }
    return corrected;
}

