#ifndef DECODIUM_FORTRANRUNTIMEGUARD_HPP
#define DECODIUM_FORTRANRUNTIMEGUARD_HPP

#include <QMutex>

namespace decodium
{
namespace fortran
{

inline QMutex& runtime_mutex ()
{
  static QMutex mutex;
  return mutex;
}

}
}

#endif
