#include "Detector/FST4DecodeCancellation.hpp"
#include "Detector/FST4DecodeWorker.hpp"
#include "Detector/FortranRuntimeGuard.hpp"
#include <QtTest/QtTest>
#include <future>
#include <thread>
#include <chrono>

class Fst4CancellationTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void scopesAndThreads()
    {
        using namespace decodium::fst4;
        std::atomic<std::uint64_t> retired {4};
        QVERIFY(!decodeCancelled());
        {
            ScopedDecodeCancellation outer(retired, 4, true);
            QVERIFY(decodeCancelled());
            {
                ScopedDecodeCancellation fresh(retired, 5, true);
                QVERIFY(!decodeCancelled());
            }
            QVERIFY(decodeCancelled());
            {
                ScopedDecodeCancellation fst4w(retired, 4, false);
                QVERIFY(!decodeCancelled());
            }
            bool otherCancelled = true;
            std::thread other([&] { otherCancelled = decodeCancelled(); });
            other.join();
            QVERIFY(!otherCancelled);
            QVERIFY(decodeCancelled());
        }
        QVERIFY(!decodeCancelled());
        ScopedDecodeCancellation legacySerial(retired, 0, true);
        QVERIFY(!decodeCancelled());
    }

    void retiredQueueDoesNotWaitForNativeMutex()
    {
        using namespace decodium::fst4;
        FST4DecodeWorker worker;
        worker.retireThrough(4);
        worker.retireThrough(2); // Retirement must remain monotonic.
        DecodeRequest request;
        request.serial = 4;
        request.mode = QStringLiteral("FST4");
        request.ntrperiod = 15;
        request.nfa = 1400;
        request.nfb = 1600;
        request.nfqso = 1500;
        int completions = 0;
        quint64 serial = 0;
        QStringList rows;
        QObject::connect(&worker, &FST4DecodeWorker::decodeReady, &worker,
                         [&](quint64 s, QStringList r) { ++completions; serial=s; rows=r; },
                         Qt::DirectConnection);
        auto& mutex = decodium::fortran::runtime_mutex();
        mutex.lock();
        std::promise<void> finished;
        auto future = finished.get_future();
        std::thread thread([&] { worker.decode(request); finished.set_value(); });
        bool completedWhileNativeBusy = future.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        // Always release/join before assertions, including the negative-control path.
        mutex.unlock();
        thread.join();
        QVERIFY(completedWhileNativeBusy);
        QCOMPARE(completions, 1);
        QCOMPARE(serial, quint64(4));
        QVERIFY(rows.isEmpty());
    }
};
QTEST_GUILESS_MAIN(Fst4CancellationTest)
#include "test_fst4_decode_cancellation.moc"
