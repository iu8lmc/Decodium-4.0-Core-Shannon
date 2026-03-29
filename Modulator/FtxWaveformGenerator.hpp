#ifndef FTX_WAVEFORM_GENERATOR_HPP
#define FTX_WAVEFORM_GENERATOR_HPP

#include <QVector>

namespace decodium
{
namespace txwave
{

QVector<float> generateFt2Wave (int const* itone, int nsym, int nsps, float fsample, float f0);
QVector<float> generateFt4Wave (int const* itone, int nsym, int nsps, float fsample, float f0);
QVector<float> generateFt8Wave (int const* itone, int nsym, int nsps, float bt, float fsample, float f0);

}
}

#endif // FTX_WAVEFORM_GENERATOR_HPP
