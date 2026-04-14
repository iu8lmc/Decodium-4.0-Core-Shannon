#ifndef FOX_WAVE_GUARD_HPP
#define FOX_WAVE_GUARD_HPP

#include <QReadWriteLock>
#include <QString>
#include <QVector>

inline QReadWriteLock& fox_wave_lock ()
{
  static QReadWriteLock lock;
  return lock;
}

inline QVector<float>& precomputed_tx_wave_buffer ()
{
  static QVector<float> wave;
  return wave;
}

inline QString& precomputed_tx_wave_mode ()
{
  static QString mode;
  return mode;
}

#endif // FOX_WAVE_GUARD_HPP
