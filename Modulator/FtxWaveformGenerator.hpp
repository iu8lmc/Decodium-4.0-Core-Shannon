#ifndef FTX_WAVEFORM_GENERATOR_HPP
#define FTX_WAVEFORM_GENERATOR_HPP

#include <array>

#include <QString>
#include <QVector>

namespace decodium
{
namespace txwave
{

QVector<float> generateFt2Wave (int const* itone, int nsym, int nsps, float fsample, float f0);
QVector<float> generateFt4Wave (int const* itone, int nsym, int nsps, float fsample, float f0);
QVector<float> generateFt8Wave (int const* itone, int nsym, int nsps, float bt, float fsample, float f0);
// JT4 uses a 4.375 baud symbol clock. The symbol boundaries are fractional at
// common audio sample rates, so use a dedicated generator instead of rounding
// samples-per-symbol for every symbol.
QVector<float> generateJt4Wave (int const* itone, int nsym, float fsample, float f0,
                                int submode = 0);
// JT65 uses 4096 samples per symbol at the reference 11025 Hz rate.
// Preserve fractional symbol boundaries when rendering at 48 kHz.
QVector<float> generateJt65Wave (int const* itone, int nsym, float fsample, float f0);
QVector<float> generateMsk144Wave (int const* itone, int nsym, float fsample, float centerFrequency,
                                   double trPeriodSeconds);
QVector<float> generateFst4Wave (int const* itone, int nsym, int nsps, float fsample, int hmod, float f0);
QVector<float> generateToneWave (int const* itone, int nsym, int nsps, float fsample,
                                 float toneSpacing, float f0);
QVector<float> generateCwWave (QString const& message, int ifreq);
QVector<float> generateCwWaveWpm (QString const& message, int ifreq, int wpm);
std::array<int, 250> encodeMorseBits (QString const& message, int* symbolCount = nullptr);
std::array<int, 6> encodeEchoCallTones (QString const& callsign);
bool packSuperFoxMessage (QString const& line, QString const& otpKey, bool bMoreCQs, bool bSendMsg,
                          QString const& freeTextMsg, std::array<unsigned char, 50>* xinOut);
bool generateSuperFoxTx (QString const& otpKey);

}
}

#endif // FTX_WAVEFORM_GENERATOR_HPP
