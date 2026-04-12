#ifndef SHARED_MEMORY_SEGMENT_HPP
#define SHARED_MEMORY_SEGMENT_HPP

#include <QString>
#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#if defined(Q_OS_DARWIN)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <QSharedMemory>
#endif

class SharedMemorySegment
{
public:
  explicit SharedMemorySegment (QString const& key = QString {})
  {
    setKey (key);
  }

  ~SharedMemorySegment ()
  {
    detach ();
  }

  SharedMemorySegment (SharedMemorySegment const&) = delete;
  SharedMemorySegment& operator= (SharedMemorySegment const&) = delete;

  void setKey (QString const& key)
  {
#if defined(Q_OS_DARWIN)
    if (attached_)
      {
        last_error_ = QStringLiteral ("Cannot change key while attached");
        return;
      }
    key_ = key;
    path_ = make_path (key_);
#else
    shared_.setKey (key);
#endif
  }

  QString key () const
  {
#if defined(Q_OS_DARWIN)
    return key_;
#else
    return shared_.key ();
#endif
  }

  QString nativeKey () const
  {
#if defined(Q_OS_DARWIN)
    return path_;
#else
    return shared_.nativeKey ();
#endif
  }

  bool create (int nsize)
  {
#if defined(Q_OS_DARWIN)
    if (attached_)
      {
        last_error_ = QStringLiteral ("Already attached");
        return false;
      }
    if (key_.isEmpty ())
      {
        last_error_ = QStringLiteral ("Shared memory key is empty");
        return false;
      }
    if (nsize <= 0)
      {
        last_error_ = QStringLiteral ("Invalid shared memory size");
        return false;
      }

    auto path_bytes = QFile::encodeName (path_);
    int flags = O_RDWR | O_CREAT | O_EXCL;
    int new_fd = ::open (path_bytes.constData (), flags, 0600);
    if (new_fd < 0 && errno == EEXIST)
      {
        // Stale orphan file: try to remove once and recreate.
        if (0 == ::unlink (path_bytes.constData ()))
          {
            new_fd = ::open (path_bytes.constData (), flags, 0600);
          }
      }
    if (new_fd < 0)
      {
        last_error_ = QStringLiteral ("open failed: %1").arg (QString::fromLocal8Bit (std::strerror (errno)));
        return false;
      }

    if (0 != ::ftruncate (new_fd, nsize))
      {
        last_error_ = QStringLiteral ("ftruncate failed: %1").arg (QString::fromLocal8Bit (std::strerror (errno)));
        ::close (new_fd);
        ::unlink (path_bytes.constData ());
        return false;
      }

    void * mapped = ::mmap (nullptr, static_cast<size_t> (nsize),
                            PROT_READ | PROT_WRITE, MAP_SHARED, new_fd, 0);
    if (MAP_FAILED == mapped)
      {
        last_error_ = QStringLiteral ("mmap failed: %1").arg (QString::fromLocal8Bit (std::strerror (errno)));
        ::close (new_fd);
        ::unlink (path_bytes.constData ());
        return false;
      }

    fd_ = new_fd;
    data_ = mapped;
    size_ = nsize;
    attached_ = true;
    owner_ = true;
    lock_held_ = false;
    last_error_.clear ();
    return true;
#else
    return shared_.create (nsize);
#endif
  }

  bool attach ()
  {
#if defined(Q_OS_DARWIN)
    if (attached_)
      {
        return true;
      }
    if (key_.isEmpty ())
      {
        last_error_ = QStringLiteral ("Shared memory key is empty");
        return false;
      }

    auto path_bytes = QFile::encodeName (path_);
    int new_fd = ::open (path_bytes.constData (), O_RDWR, 0600);
    if (new_fd < 0)
      {
        last_error_ = QStringLiteral ("open failed: %1").arg (QString::fromLocal8Bit (std::strerror (errno)));
        return false;
      }

    struct stat st {};
    if (0 != ::fstat (new_fd, &st) || st.st_size <= 0)
      {
        last_error_ = QStringLiteral ("fstat failed: %1").arg (QString::fromLocal8Bit (std::strerror (errno)));
        ::close (new_fd);
        return false;
      }

    int map_size = static_cast<int> (st.st_size);
    void * mapped = ::mmap (nullptr, static_cast<size_t> (map_size),
                            PROT_READ | PROT_WRITE, MAP_SHARED, new_fd, 0);
    if (MAP_FAILED == mapped)
      {
        last_error_ = QStringLiteral ("mmap failed: %1").arg (QString::fromLocal8Bit (std::strerror (errno)));
        ::close (new_fd);
        return false;
      }

    fd_ = new_fd;
    data_ = mapped;
    size_ = map_size;
    attached_ = true;
    owner_ = false;
    lock_held_ = false;
    last_error_.clear ();
    return true;
#else
    return shared_.attach ();
#endif
  }

  bool detach ()
  {
#if defined(Q_OS_DARWIN)
    if (!attached_)
      {
        return false;
      }

    if (lock_held_)
      {
        unlock ();
      }

    bool ok = true;
    if (data_ && MAP_FAILED != data_)
      {
        ok = (0 == ::munmap (data_, static_cast<size_t> (size_)));
      }
    if (fd_ >= 0)
      {
        if (0 != ::close (fd_))
          {
            ok = false;
          }
      }
    if (owner_)
      {
        auto path_bytes = QFile::encodeName (path_);
        (void) ::unlink (path_bytes.constData ());
      }

    data_ = nullptr;
    fd_ = -1;
    size_ = 0;
    attached_ = false;
    owner_ = false;
    lock_held_ = false;
    if (!ok)
      {
        last_error_ = QStringLiteral ("detach failed");
      }
    return ok;
#else
    return shared_.detach ();
#endif
  }

  int size () const
  {
#if defined(Q_OS_DARWIN)
    return size_;
#else
    return static_cast<int> (shared_.size ());
#endif
  }

  void * data ()
  {
#if defined(Q_OS_DARWIN)
    return attached_ ? data_ : nullptr;
#else
    return shared_.data ();
#endif
  }

  bool lock (int timeout_ms = -1)
  {
#if defined(Q_OS_DARWIN)
    if (!attached_ || fd_ < 0)
      {
        last_error_ = QStringLiteral ("Not attached");
        return false;
      }
    if (lock_held_)
      {
        return true;
      }

    struct flock fl {};
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    bool const blocking = timeout_ms < 0;
    int waited_ms = 0;
    while (true)
      {
        if (0 == ::fcntl (fd_, F_SETLK, &fl))
          {
            lock_held_ = true;
            last_error_.clear ();
            return true;
          }

        auto const err = errno;
        if (EINTR == err)
          {
            continue;
          }
        if (EAGAIN == err || EACCES == err)
          {
            if (!blocking && waited_ms >= timeout_ms)
              {
                last_error_ = QStringLiteral ("lock timeout after %1 ms").arg (timeout_ms);
                return false;
              }
            ::usleep (1000);
            ++waited_ms;
            continue;
          }
        last_error_ = QStringLiteral ("lock failed: %1").arg (QString::fromLocal8Bit (std::strerror (err)));
        return false;
      }
#else
    (void) timeout_ms;
    return shared_.lock ();
#endif
  }

  bool unlock ()
  {
#if defined(Q_OS_DARWIN)
    if (!attached_ || fd_ < 0)
      {
        last_error_ = QStringLiteral ("Not attached");
        return false;
      }
    if (!lock_held_)
      {
        return true;
      }

    struct flock fl {};
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;

    if (0 != ::fcntl (fd_, F_SETLK, &fl))
      {
        last_error_ = QStringLiteral ("unlock failed: %1").arg (QString::fromLocal8Bit (std::strerror (errno)));
        return false;
      }

    lock_held_ = false;
    last_error_.clear ();
    return true;
#else
    return shared_.unlock ();
#endif
  }

  QString errorString () const
  {
#if defined(Q_OS_DARWIN)
    return last_error_;
#else
    return shared_.errorString ();
#endif
  }

private:
#if defined(Q_OS_DARWIN)
  static QString make_path (QString const& key)
  {
    auto root = QStandardPaths::writableLocation (QStandardPaths::TempLocation);
    if (root.isEmpty ())
      {
        root = QDir::tempPath ();
      }
    QDir dir {root};
    dir.mkpath (QStringLiteral ("ft2-ipc"));
    auto digest = QCryptographicHash::hash (key.toUtf8 (), QCryptographicHash::Sha256).toHex ();
    auto name = QString::fromLatin1 (digest.left (32));
    return dir.absoluteFilePath (QStringLiteral ("ft2-ipc/%1.map").arg (name));
  }

  QString key_;
  QString path_;
  QString last_error_;
  int fd_ {-1};
  void * data_ {nullptr};
  int size_ {0};
  bool attached_ {false};
  bool owner_ {false};
  bool lock_held_ {false};
#else
  QSharedMemory shared_;
#endif
};

#endif // SHARED_MEMORY_SEGMENT_HPP
