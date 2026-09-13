// 1.0.234 (Sprint 3 / Phase 2 performance roadmap):
// Test unitario per StreamingListModel<T> -- vedi lib/ui/StreamingListModel.h.
//
// Test cases:
//   1. testBatchInsertion       -- 20 enqueue rapidi -> 1 solo rowsInserted
//   2. testThreadSafetyEnqueue  -- 4 thread * 50 enqueue -> rowCount() == 200
//   3. testRowCap               -- setMaxRows(10), 25 enqueue -> rowCount() == 10
//   4. testForceFlush           -- enqueue + forceFlush sincrono -> rowCount() == 5
//
// Status: file scritto, NON ancora hookato in tests/CMakeLists.txt
// (vedi report commit). L'hookup richiede aggiungere ~4 righe al
// CMakeLists e validare che il link non porti in dipendenze pesanti
// (wsjt_qt / wsjt_cxx) inutili per un template header-only.

#include <QtTest>

#include <QCoreApplication>
#include <QHash>
#include <QSignalSpy>
#include <QString>
#include <QVariant>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureSynchronizer>

#include "lib/ui/StreamingListModel.h"

namespace
{
  struct TestEntry
  {
    int id {0};
    QString name;
  };

  // Subclass concreta minimale.
  class TestModel final
    : public StreamingListModel<TestEntry>
  {
  public:
    enum Role
    {
      IdRole = Qt::UserRole + 1,
      NameRole
    };

    using StreamingListModel<TestEntry>::StreamingListModel;

    QVariant data (const QModelIndex &index, int role) const override
    {
      if (!index.isValid () || index.row () < 0 || index.row () >= rowCount ())
        return {};
      const TestEntry &e = entryAt (index.row ());
      switch (role)
        {
          case IdRole:   return e.id;
          case NameRole: return e.name;
          default:       return {};
        }
    }

    QHash<int, QByteArray> roleNames () const override
    {
      return {
        {IdRole,   QByteArrayLiteral ("id")},
        {NameRole, QByteArrayLiteral ("name")}
      };
    }
  };
}

class TestStreamingListModel
  : public QObject
{
  Q_OBJECT

private:
  // Helper: spin la event loop finche' la spy raggiunge il count atteso
  // o scade il timeout. QSignalSpy::wait fa gia' questo, ma vogliamo
  // tollerare arrivi sparsi -- usiamo QTRY_VERIFY_WITH_TIMEOUT.

  Q_SLOT void testBatchInsertion ()
  {
    TestModel model;
    model.setFlushIntervalMs (50);

    QSignalSpy inserted_spy (&model, &QAbstractItemModel::rowsInserted);

    for (int i = 0; i < 20; ++i)
      {
        model.enqueue (TestEntry {i, QStringLiteral ("entry-%1").arg (i)});
      }

    // Prima del flush la coda e' piena e il modello ha 0 righe.
    QCOMPARE (model.pendingCount (), 20);
    QCOMPARE (model.rowCount (), 0);

    // Aspetta che il timer scatti.
    QTRY_COMPARE_WITH_TIMEOUT (model.rowCount (), 20, 2000);

    // Un solo rowsInserted con range [0, 19] (batch coalescing).
    QCOMPARE (inserted_spy.count (), 1);
    const QList<QVariant> args = inserted_spy.takeFirst ();
    QCOMPARE (args.at (1).toInt (), 0);   // first
    QCOMPARE (args.at (2).toInt (), 19);  // last
    QCOMPARE (model.pendingCount (), 0);
  }

  Q_SLOT void testThreadSafetyEnqueue ()
  {
    TestModel model;
    model.setFlushIntervalMs (50);

    constexpr int kThreads        = 4;
    constexpr int kPerThread      = 50;
    constexpr int kExpectedTotal  = kThreads * kPerThread;

    QFutureSynchronizer<void> sync;
    for (int t = 0; t < kThreads; ++t)
      {
        sync.addFuture (QtConcurrent::run ([&model, t] () {
          for (int i = 0; i < kPerThread; ++i)
            {
              model.enqueue (TestEntry {t * 1000 + i,
                                        QStringLiteral ("t%1-%2").arg (t).arg (i)});
            }
        }));
      }
    sync.waitForFinished ();

    // forceFlush sincrono per non dover aspettare piu' tick del timer.
    model.forceFlush ();

    QCOMPARE (model.rowCount (), kExpectedTotal);
    QCOMPARE (model.pendingCount (), 0);
  }

  Q_SLOT void testRowCap ()
  {
    TestModel model;
    model.setFlushIntervalMs (50);
    model.setMaxRows (10);

    QSignalSpy inserted_spy (&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removed_spy  (&model, &QAbstractItemModel::rowsRemoved);

    for (int i = 0; i < 25; ++i)
      {
        model.enqueue (TestEntry {i, QStringLiteral ("entry-%1").arg (i)});
      }

    QTRY_COMPARE_WITH_TIMEOUT (model.rowCount (), 10, 2000);

    // 1 inserimento batch (25) e 1 rimozione batch (15) per riportare a 10.
    QCOMPARE (inserted_spy.count (), 1);
    QCOMPARE (removed_spy.count (),  1);

    const QList<QVariant> ins = inserted_spy.takeFirst ();
    QCOMPARE (ins.at (1).toInt (), 0);
    QCOMPARE (ins.at (2).toInt (), 24);

    const QList<QVariant> rem = removed_spy.takeFirst ();
    QCOMPARE (rem.at (1).toInt (), 0);
    QCOMPARE (rem.at (2).toInt (), 14);

    // Le entries sopravvissute sono le ultime 10 (id 15..24): verifichiamo
    // via data() sul primo elemento.
    const QModelIndex first = model.index (0, 0);
    QCOMPARE (model.data (first, TestModel::IdRole).toInt (), 15);
  }

  Q_SLOT void testForceFlush ()
  {
    TestModel model;
    // Intervallo lungo: se forceFlush non funzionasse, il timer non
    // scatterebbe entro il check.
    model.setFlushIntervalMs (60'000);

    QSignalSpy inserted_spy (&model, &QAbstractItemModel::rowsInserted);

    for (int i = 0; i < 5; ++i)
      {
        model.enqueue (TestEntry {i, QStringLiteral ("entry-%1").arg (i)});
      }

    QCOMPARE (model.rowCount (), 0);
    model.forceFlush ();
    // forceFlush e' sincrono: il count deve essere aggiornato subito.
    QCOMPARE (model.rowCount (), 5);
    QCOMPARE (inserted_spy.count (), 1);

    const QList<QVariant> args = inserted_spy.takeFirst ();
    QCOMPARE (args.at (1).toInt (), 0);
    QCOMPARE (args.at (2).toInt (), 4);

    // forceFlush con coda vuota e' no-op (nessun signal aggiuntivo).
    model.forceFlush ();
    QCOMPARE (inserted_spy.count (), 0);
    QCOMPARE (model.rowCount (), 5);
  }
};

QTEST_MAIN (TestStreamingListModel)

#include "test_streaming_list_model.moc"
