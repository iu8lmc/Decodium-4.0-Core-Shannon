#include "SharedMemorySegment.hpp"

#include <cstdio>

// Multiple instances: KK1D, 17 Jul 2013
SharedMemorySegment shmem;

struct jt9com;

namespace
{
  void report_shmem_failure (char const * operation)
  {
    auto const message = shmem.errorString ().toLocal8Bit ();
    std::fprintf (stderr, "jt9 shmem %s failed: %s\n", operation,
                  message.isEmpty () ? "unknown error" : message.constData ());
    std::fflush (stderr);
  }
}

// C wrappers for a QSharedMemory class instance
extern "C"
{
  bool shmem_create (int nsize) {return shmem.create(nsize);}
  void shmem_setkey (char * const mykey) {shmem.setKey(QString::fromLatin1(mykey));}
  bool shmem_attach ()
  {
    auto const ok = shmem.attach ();
    if (!ok) report_shmem_failure ("attach");
    return ok;
  }
  int shmem_size () {return static_cast<int> (shmem.size());}
  struct jt9com * shmem_address () {return reinterpret_cast<struct jt9com *>(shmem.data());}
  bool shmem_lock ()
  {
    auto const ok = shmem.lock ();
    if (!ok) report_shmem_failure ("lock");
    return ok;
  }
  bool shmem_unlock ()
  {
    auto const ok = shmem.unlock ();
    if (!ok) report_shmem_failure ("unlock");
    return ok;
  }
  bool shmem_detach ()
  {
    auto const ok = shmem.detach ();
    if (!ok) report_shmem_failure ("detach");
    return ok;
  }
}
