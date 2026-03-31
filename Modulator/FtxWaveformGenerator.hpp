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
QVector<float> generateFst4Wave (int const* itone, int nsym, int nsps, float fsample, int hmod, float f0);
bool packSuperFoxMessage (QString const& line, QString const& otpKey, bool bMoreCQs, bool bSendMsg,
                          QString const& freeTextMsg, std::array<unsigned char, 50>* xinOut);
bool generateSuperFoxTx (QString const& otpKey);

}
}

#endif // FTX_WAVEFORM_GENERATOR_HPP
