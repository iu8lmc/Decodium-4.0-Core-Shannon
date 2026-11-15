#!/usr/bin/env python3
"""Write an experimental FST4W worker source to a separate file.

The generated variant can release the runtime mutex at candidate boundaries
when a FST4W session is retired and DECODIUM_LAB_FST4W_YIELD=1. It is not a
production change: shared FST4 state and other decoder interleavings still
require an audit. This tool never overwrites the project's worker source.
"""
import argparse
from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = root/'Detector/FST4DecodeWorker.cpp'
p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True)
p.add_argument('--protect-session',action='store_true',help='serialize FST4 decode and reset in the lab variant')
p.add_argument('--protect-aux',action='store_true',help='isolate sync cache and diagnostic state by thread')
a=p.parse_args()
if a.protect_aux and not a.protect_session: p.error('--protect-aux requires --protect-session')
if a.output.resolve() == source.resolve(): raise SystemExit('refusing to overwrite production source')
s=source.read_bytes()
def replace(old,new):
    global s
    if s.count(old)!=1: raise RuntimeError('worker source no longer matches lab anchor: '+repr(old[:80]))
    s=s.replace(old,new,1)
replace(b'#include <QTextStream>\r\n',b'#include <QTextStream>\r\n#include <QThread>\n')
replace(b'#include <QThread>\n',b'#include <QThread>\n#include <QCoreApplication>\n#include <QDebug>\n#include <QJsonArray>\n#include <QJsonDocument>\n#include <QJsonObject>\n#include <QStandardPaths>\n')
replace(b'  using Complex = std::complex<float>;\r\n',b'  using Complex = std::complex<float>;\r\n  thread_local std::atomic<std::uint64_t> const* labRetiredBoundary = nullptr;\n')
replace(b'  if (decodeCancelled ()) return {};\r\n\r\n  RuntimeConfig const cfg =',b'''  if (decodeCancelled ()) return {};
  // Experimental only: yield a retired FST4W decode at candidate boundaries.
  bool const labYield = request.iwspr != 0 && request.serial != 0 && labRetiredBoundary
      && qEnvironmentVariableIntValue ("DECODIUM_LAB_FST4W_YIELD") > 0;
  auto yieldIfRetired = [&] {
    if (labYield && labRetiredBoundary->load (std::memory_order_acquire) >= request.serial)
      {
        runtime_mutex.unlock ();
        QThread::msleep (2);
        runtime_mutex.lock ();
      }
  };

  RuntimeConfig const cfg =''')
replace(b'      if (decodeCancelled ()) return {};\r\n          if (request.iwspr == 0',b'      if (decodeCancelled ()) return {};\n          yieldIfRetired ();\n          if (request.iwspr == 0')
replace(b'          if (decodeCancelled ()) return {};\r\n          bool candidateDone = false;',b'          if (decodeCancelled ()) return {};\n          yieldIfRetired ();\n          bool candidateDone = false;')
old=b'  ScopedDecodeCancellation cancellation (m_retiredSerial, request.serial, request.iwspr == 0, &m_shutdown);\n'
replace(old,old+b'  auto const* previousLabBoundary = labRetiredBoundary;\n  labRetiredBoundary = &m_retiredSerial;\n  auto const restoreLabBoundary = qScopeGuard ([previousLabBoundary] { labRetiredBoundary = previousLabBoundary; });\n')
old=b'  if (request.serial && m_retiredSerial.load (std::memory_order_acquire) >= request.serial)\n    rows.clear ();'
new=b'''  if (request.serial && m_retiredSerial.load (std::memory_order_acquire) >= request.serial) {
    if (request.iwspr && QStandardPaths::isTestModeEnabled ()
        && qEnvironmentVariableIntValue ("DECODIUM_LAB_FST4W_CAPTURE") > 0) {
      QFile calls (QDir (QString::fromLocal8Bit (request.dataDir)).filePath (QStringLiteral ("fst4w_calls.txt")));
      QByteArray saved;
      if (calls.open (QIODevice::ReadOnly)) saved = calls.readAll ();
      QJsonObject result {{QStringLiteral ("rows"), QJsonArray::fromStringList (rows)},
                          {QStringLiteral ("calls"), QString::fromUtf8 (saved)}};
      qInfo ().noquote () << "[FST4W-RETIRED-LAB]"
                          << QJsonDocument (result).toJson (QJsonDocument::Compact);
    }
    rows.clear ();
  }'''
replace(old,new)
if a.protect_session:
    old=b'  thread_local std::atomic<std::uint64_t> const* labRetiredBoundary = nullptr;\n'
    replace(old,old+b'  QMutex& labFst4SessionMutex () { static QMutex mutex; return mutex; }\n')
    old=b'  if (decodeCancelled ()) return {};\n  auto& runtime_mutex = decodium::fortran::runtime_mutex ();'
    new=b'''  if (decodeCancelled ()) return {};
  auto& sessionMutex = labFst4SessionMutex ();
  while (!sessionMutex.tryLock (50)) {
    if (decodeCancelled ()) return {};
  }
  auto const sessionUnlock = qScopeGuard ([&sessionMutex] { sessionMutex.unlock (); });
  if (decodeCancelled ()) return {};
  auto& runtime_mutex = decodium::fortran::runtime_mutex ();'''
    replace(old,new)
    old=b'  auto const runtime_unlock = qScopeGuard ([&runtime_mutex] { runtime_mutex.unlock (); });\n'
    new=b'  bool runtimeLocked = true;\n  auto const runtime_unlock = qScopeGuard ([&] { if (runtimeLocked) runtime_mutex.unlock (); });\n'
    replace(old,new)
    old=b'''        runtime_mutex.unlock ();
        QThread::msleep (2);
        runtime_mutex.lock ();
      }
  };'''
    new=b'''        runtime_mutex.unlock ();
        runtimeLocked = false;
        QThread::msleep (2);
        while (!runtime_mutex.tryLock (50)) {
          if (decodeCancelled ()) return false;
        }
        runtimeLocked = true;
        if (decodeCancelled ()) return false;
      }
    return true;
  };'''
    replace(old,new)
    old=b'          yieldIfRetired ();\n'
    if s.count(old)!=2: raise RuntimeError('unexpected yield sites')
    s=s.replace(old,b'          if (!yieldIfRetired ()) return {};\n')
    old=b'extern "C" void ftx_fst4_reset_runtime_state_c ()\r\n{\r\n  QMutexLocker runtime_lock'
    new=b'extern "C" void ftx_fst4_reset_runtime_state_c ()\r\n{\n  QMutexLocker session_lock {&labFst4SessionMutex ()};\n  QMutexLocker runtime_lock'
    replace(old,new)
if a.protect_aux:
    old=b'''  SyncCache& sync_cache ()\r
  {\r
    static SyncCache cache;\r
    return cache;\r
  }'''
    new=b'''  std::atomic<std::uint64_t>& labSyncEpoch ()
  {
    static std::atomic<std::uint64_t> epoch {0};
    return epoch;
  }
  SyncCache& sync_cache ()
  {
    static thread_local SyncCache cache;
    static thread_local std::uint64_t seen = labSyncEpoch ().load (std::memory_order_acquire);
    auto const current = labSyncEpoch ().load (std::memory_order_acquire);
    if (seen != current) { cache = SyncCache {}; seen = current; }
    return cache;
  }'''
    replace(old,new)
    old=b'  persistent_state () = PersistentState {};\r\n  sync_cache () = SyncCache {};'
    new=b'  persistent_state () = PersistentState {};\n  labSyncEpoch ().fetch_add (1, std::memory_order_release);\n  sync_cache () = SyncCache {};'
    replace(old,new)
    anchor=b'extern "C" int ftx_fst4_debug_first_candidate_c ('
    start=s.index(anchor)
    old=b'  PersistentState& state = persistent_state ();\r\n'
    pos=s.index(old,start)
    s=s[:pos]+b'  PersistentState state {};\n'+s[pos+len(old):]
a.output.write_bytes(s)
print('Experimental source written to',a.output)
