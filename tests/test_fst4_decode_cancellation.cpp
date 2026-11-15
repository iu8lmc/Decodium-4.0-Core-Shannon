#include "Detector/FST4DecodeCancellation.hpp"
#include "Detector/FST4DecodeWorker.hpp"
#include "Detector/FortranRuntimeGuard.hpp"
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <future>
#include <thread>
#include <chrono>
#include <complex>
#include <cstring>
#include <vector>

extern "C" int ftx_fst4_sync_search_c(std::complex<float> const*, int, int, int, int,
                                      float, float*, float*, int*);
extern "C" void ftx_fst4_reset_runtime_state_c();
extern "C" void __fst4_decode_MOD_fst4_sync_search(std::complex<float>*, int*, int*, float*, int*, int*,
                                                    int*, float*, float*, float*, int*);

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

    void shutdownCancelsFst4wWithoutChangingRetirement()
    {
        using namespace decodium::fst4;
        std::atomic<std::uint64_t> retired {4};
        std::atomic_bool shutdown {false};
        ScopedDecodeCancellation fst4w(retired, 4, false, &shutdown);
        QVERIFY(!decodeCancelled());
        shutdown.store(true);
        QVERIFY(decodeCancelled());
        {
            ScopedDecodeCancellation nested(retired, 5, true);
            QVERIFY(!decodeCancelled());
        }
        QVERIFY(decodeCancelled());
        bool otherCancelled = true;
        std::thread other([&] { otherCancelled = decodeCancelled(); });
        other.join();
        QVERIFY(!otherCancelled);
    }

    void shutdownInterruptsFst4wWaitingForNativeMutex()
    {
        using namespace decodium::fst4;
        FST4DecodeWorker worker;
        worker.retireThrough(4);
        DecodeRequest request;
        request.serial = 4;
        request.iwspr = 1;
        request.ntrperiod = 120;
        auto& mutex = decodium::fortran::runtime_mutex();
        mutex.lock();
        std::promise<void> started, finished;
        auto startedFuture = started.get_future();
        auto finishedFuture = finished.get_future();
        std::thread thread([&] { started.set_value(); worker.decode(request); finished.set_value(); });
        startedFuture.wait();
        bool keptRunning = finishedFuture.wait_for(std::chrono::milliseconds(150)) == std::future_status::timeout;
        worker.beginShutdown();
        bool stoppedWhileLocked = finishedFuture.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        mutex.unlock();
        thread.join();
        QVERIFY(keptRunning);
        QVERIFY(stoppedWhileLocked);
    }

    void shutdownInterruptsRunningFst4w()
    {
        using namespace decodium::fst4;
        QTemporaryDir dataDir;
        QVERIFY(dataDir.isValid());
        FST4DecodeWorker worker;
        DecodeRequest request;
        request.serial = 5;
        request.iwspr = 1;
        request.ntrperiod = 120;
        request.ndepth = 3;
        request.nfqso = 1500;
        request.ntol = 250;
        request.dataDir = dataDir.path().toLocal8Bit();
        request.audio.resize(120 * 12000);
        std::uint32_t random = 1;
        for (auto& sample : request.audio) {
            random = random * 1664525u + 1013904223u;
            sample = static_cast<short>((random >> 16) & 0x3fff);
        }
        auto& mutex = decodium::fortran::runtime_mutex();
        std::promise<void> finished;
        auto future = finished.get_future();
        std::thread thread([&] { worker.decode(request); finished.set_value(); });
        bool observedRunning = false;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline &&
               future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
            if (!mutex.tryLock()) { observedRunning = true; break; }
            mutex.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        worker.beginShutdown();
        bool stopped = future.wait_for(std::chrono::seconds(12)) == std::future_status::ready;
        thread.join();
        QVERIFY(observedRunning);
        QVERIFY(stopped);
    }

    void shutdownInterruptsTwoWorkersWaitingForNativeMutex()
    {
        using namespace decodium::fst4;
        FST4DecodeWorker first, second;
        DecodeRequest request;
        request.serial = 7;
        request.iwspr = 1;
        request.ntrperiod = 120;
        auto& mutex = decodium::fortran::runtime_mutex();
        mutex.lock();
        std::promise<void> startedFirst, startedSecond, finishedFirst, finishedSecond;
        auto s1 = startedFirst.get_future(); auto s2 = startedSecond.get_future();
        auto f1 = finishedFirst.get_future(); auto f2 = finishedSecond.get_future();
        std::thread t1([&] { startedFirst.set_value(); first.decode(request); finishedFirst.set_value(); });
        s1.wait();
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        std::thread t2([&] { startedSecond.set_value(); second.decode(request); finishedSecond.set_value(); });
        s2.wait();
        bool const bothWaiting = f1.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout
            && f2.wait_for(std::chrono::milliseconds(100)) == std::future_status::timeout;
        second.beginShutdown();
        bool const secondStopped = f2.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        first.beginShutdown();
        bool const firstStopped = f1.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        mutex.unlock();
        t1.join(); t2.join();
        QVERIFY(bothWaiting);
        QVERIFY(secondStopped);
        QVERIFY(firstStopped);
    }

    void labConcurrentAuxiliarySyncSearch()
    {
        if (qEnvironmentVariableIntValue("DECODIUM_LAB_FST4W_AUX_CHECK") == 0)
            QSKIP("Concurrent auxiliary calls require the isolated-state laboratory build");
        struct Result { float strength = 0; float frequency = 0; int index = 0; int ok = 0; };
        std::vector<std::complex<float>> a(10000), b(7024);
        auto fill = [](auto& values, int factor) {
            for (size_t i = 0; i < values.size(); ++i)
                values[i] = {float((i * factor) % 101) / 101.0f, float((i * 13) % 89) / 89.0f};
        };
        fill(a, 7); fill(b, 11);
        auto search = [](auto const& audio, int nsps, int down, int period) {
            Result result;
            result.ok = ftx_fst4_sync_search_c(audio.data(), int(audio.size()), nsps, down,
                                               period, 0.0f, &result.strength,
                                               &result.frequency, &result.index);
            return result;
        };
        Result const expectedA = search(a, 720, 18, 15);
        Result const expectedB = search(b, 8200, 205, 120);
        QVERIFY(expectedA.ok && expectedB.ok);
        for (int round = 0; round < 3; ++round) {
            Result actualA, actualB;
            std::promise<void> start;
            auto ready = start.get_future().share();
            std::thread t1([&] { ready.wait(); actualA = search(a, 720, 18, 15); });
            std::thread t2([&] { ready.wait(); actualB = search(b, 8200, 205, 120); });
            start.set_value(); t1.join(); t2.join();
            QCOMPARE(actualA.ok, expectedA.ok); QCOMPARE(actualB.ok, expectedB.ok);
            QCOMPARE(actualA.index, expectedA.index); QCOMPARE(actualB.index, expectedB.index);
            QVERIFY(std::memcmp(&actualA.strength, &expectedA.strength, sizeof(float)) == 0);
            QVERIFY(std::memcmp(&actualB.strength, &expectedB.strength, sizeof(float)) == 0);
            QVERIFY(std::memcmp(&actualA.frequency, &expectedA.frequency, sizeof(float)) == 0);
            QVERIFY(std::memcmp(&actualB.frequency, &expectedB.frequency, sizeof(float)) == 0);
        }
    }

    void labCrossThreadResetInvalidatesSyncCache()
    {
        if (qEnvironmentVariableIntValue("DECODIUM_LAB_FST4W_AUX_CHECK") == 0)
            QSKIP("Cross-thread reset requires the isolated-state laboratory build");
        struct Result { float strength = 0; float frequency = 0; int index = 0; };
        std::vector<std::complex<float>> audio(10000);
        for (size_t i = 0; i < audio.size(); ++i)
            audio[i] = {float((i * 7) % 101) / 101.0f, float((i * 13) % 89) / 89.0f};
        auto search = [&audio](int modulation) {
            int size = int(audio.size()), nss = 40, period = 15, coherent = 8;
            float sampleRate = 12000.0f / 18.0f, delay = 0.0f;
            Result result;
            __fst4_decode_MOD_fst4_sync_search(audio.data(), &size, &modulation, &sampleRate,
                                               &nss, &period, &coherent, &delay,
                                               &result.strength, &result.frequency, &result.index);
            return result;
        };
        search(1); // Populate the cache on this thread.
        std::thread reset([] { ftx_fst4_reset_runtime_state_c(); });
        reset.join();
        Result const actual = search(2);
        Result expected;
        std::thread fresh([&] { expected = search(2); });
        fresh.join();
        QCOMPARE(actual.index, expected.index);
        QVERIFY(std::memcmp(&actual.strength, &expected.strength, sizeof(float)) == 0);
        QVERIFY(std::memcmp(&actual.frequency, &expected.frequency, sizeof(float)) == 0);
    }
};
QTEST_GUILESS_MAIN(Fst4CancellationTest)
#include "test_fst4_decode_cancellation.moc"
