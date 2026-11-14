#include "Detector/WSPRDecodeWorker.hpp"
#include "Detector/FortranRuntimeGuard.hpp"
#include "lib/wsprd/wsprd_embedded.h"
#include <QSemaphore>
#include <QSignalSpy>
#include <QThread>
#include <QtTest>
#include <atomic>

namespace {
wsprd_output_callback_t outputCallback = nullptr;
void* outputContext = nullptr;
QSemaphore nativeEntered, releaseNative;
std::atomic_bool blockNative {false};
std::atomic_int nativeCalls {0};
}

extern "C" void wsprd_set_output_callback(wsprd_output_callback_t callback, void* context)
{
    outputCallback = callback;
    outputContext = context;
}

extern "C" int wsprd_run(int argc, char** argv)
{
    ++nativeCalls;
    if (blockNative.exchange(false)) {
        nativeEntered.release();
        releaseNative.acquire();
    }
    if (argc != 2 || QByteArray(argv[1]) != "fixture.wav") return 7;
    outputCallback(outputContext, "1200 -10 0.1 14.097100 0 K1ABC FN42 37\n", 0);
    return 0;
}

class TestWsprWorkerQueue : public QObject {
    Q_OBJECT
signals:
    void completed(quint64 serial, QStringList rows, QStringList diagnostics, int exitCode);
private slots:
    void retiredWhileWaitingForRuntime()
    {
        nativeCalls = 0;
        blockNative = false;
        QThread thread;
        auto* worker = new decodium::wspr::WSPRDecodeWorker;
        worker->moveToThread(&thread);
        connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(worker, &decodium::wspr::WSPRDecodeWorker::decodeReady,
                this, &TestWsprWorkerQueue::completed, Qt::QueuedConnection);
        QSignalSpy completions(this, &TestWsprWorkerQueue::completed);
        QSemaphore entering;
        auto& mutex = decodium::fortran::runtime_mutex();
        mutex.lock();
        thread.start();
        QMetaObject::invokeMethod(worker, [worker, &entering] {
            entering.release();
            worker->decode({1, {QStringLiteral("fixture.wav")}});
        }, Qt::QueuedConnection);
        bool const entered = entering.tryAcquire(1, 5000);
        QTest::qWait(50); // allow the worker to reach the held runtime mutex
        worker->retireThrough(1);
        mutex.unlock();
        QElapsedTimer wait;
        wait.start();
        while (completions.isEmpty() && wait.elapsed() < 5000) QTest::qWait(10);
        thread.quit();
        bool const stopped = thread.wait(5000);
        QVERIFY(stopped);
        QVERIFY(entered);
        QCOMPARE(completions.size(), 1);
        QCOMPARE(nativeCalls.load(), 0);
        QVERIFY(completions.at(0).at(1).toStringList().isEmpty());
    }
    void queuedSessionRetirement()
    {
        nativeCalls = 0;
        blockNative = true;
        QThread thread;
        auto* worker = new decodium::wspr::WSPRDecodeWorker;
        worker->moveToThread(&thread);
        connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(worker, &decodium::wspr::WSPRDecodeWorker::decodeReady,
                this, &TestWsprWorkerQueue::completed, Qt::QueuedConnection);
        QSignalSpy completions(this, &TestWsprWorkerQueue::completed);
        thread.start();
        auto enqueue = [worker](quint64 serial) {
            QMetaObject::invokeMethod(worker, [worker, serial] {
                worker->decode({serial, {QStringLiteral("fixture.wav")}});
            }, Qt::QueuedConnection);
        };
        enqueue(1);
        bool const active = nativeEntered.tryAcquire(1, 5000);
        enqueue(2); // queued behind an actually active native decode
        worker->retireThrough(2);
        enqueue(3); // same reusable worker, fresh session
        releaseNative.release();
        // Never leave a live QThread behind if an assertion fails.
        QElapsedTimer wait;
        wait.start();
        while (completions.size() < 3 && wait.elapsed() < 5000)
            QTest::qWait(10);
        thread.quit();
        bool const stopped = thread.wait(5000);
        QVERIFY(stopped);
        QVERIFY(active);
        QCOMPARE(completions.size(), 3);
        QCOMPARE(nativeCalls.load(), 2); // retired queued request never calls wsprd
        for (int i = 0; i < 3; ++i) {
            QCOMPARE(completions.at(i).at(0).toULongLong(), quint64(i + 1));
            QCOMPARE(completions.at(i).at(3).toInt(), 0);
        }
        QVERIFY(!completions.at(0).at(1).toStringList().isEmpty());
        QVERIFY(completions.at(1).at(1).toStringList().isEmpty());
        QVERIFY(completions.at(1).at(2).toStringList().isEmpty());
        QCOMPARE(completions.at(2).at(1).toStringList(), completions.at(0).at(1).toStringList());
    }
};
QTEST_GUILESS_MAIN(TestWsprWorkerQueue)
#include "test_wspr_worker_queue.moc"
