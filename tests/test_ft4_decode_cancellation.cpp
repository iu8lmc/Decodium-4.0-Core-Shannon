#include <QtTest>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtEndian>
#include <array>
#include <atomic>
#include <QThread>
#include <vector>
#include "Detector/Ft4DecodeCancellation.hpp"
#include "Detector/FortranRuntimeGuard.hpp"

extern "C" {
void ftx_ft4_decode_c(short const*, int*, int*, int*, int*, int*, int*, int*,
                     char const*, char const*, float*, int*, float*, float*, int*, float*,
                     signed char*, char*, int*, size_t, size_t, size_t);
void ftx_ft4_cpp_dsp_rollout_stage_override_c(int);
void ftx_ft4_cpp_dsp_rollout_stage_reset_c();
}

class Ft4CancellationTest : public QObject
{
    Q_OBJECT
    std::vector<short> audio;
    QStringList decode()
    {
        std::array<float, 100> sync {}, dt {}, frequency {}, quality {};
        std::array<int, 100> snr {}, nap {};
        std::array<signed char, 7700> bits {};
        std::array<char, 3700> messages {};
        int progress=0, qso=1300, low=200, high=4000, depth=3, cq=0, contest=0, count=0;
        QByteArray call("IU8LMC      "), other(12, ' ');
        QMutexLocker lock(&decodium::fortran::runtime_mutex());
        ftx_ft4_cpp_dsp_rollout_stage_override_c(4);
        ftx_ft4_decode_c(audio.data(), &progress, &qso, &low, &high, &depth, &cq, &contest,
                         call.constData(), other.constData(), sync.data(), snr.data(), dt.data(),
                         frequency.data(), nap.data(), quality.data(), bits.data(), messages.data(),
                         &count, size_t(call.size()), size_t(other.size()), messages.size());
        ftx_ft4_cpp_dsp_rollout_stage_reset_c();
        QStringList result;
        for (int i=0; i<count && i<100; ++i)
            result << QString::fromLatin1(messages.data()+i*37, 37).trimmed();
        return result;
    }
private slots:
    void initTestCase()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString path=directory.filePath("known-ft4.wav");
        QProcess generator;
        generator.start(QString::fromLocal8Bit(FT4_WAV_GENERATOR),
                        {"--message", "CQ 9H1SR JM75", "--freq", "1300", path});
        QVERIFY(generator.waitForFinished(30000));
        QCOMPARE(generator.exitCode(), 0);
        QFile wav(path);
        QVERIFY(wav.open(QIODevice::ReadOnly));
        QByteArray pcm=wav.readAll().mid(44);
        QCOMPARE(pcm.size(), 72576*2);
        audio.resize(72576);
        for (size_t i=0; i<audio.size(); ++i)
            audio[i]=qFromLittleEndian<qint16>(reinterpret_cast<uchar const*>(pcm.constData()+i*2));
    }
    void cancelledScopeAndRecovery()
    {
        QStringList const baseline=decode();
        QVERIFY(baseline.contains("CQ 9H1SR JM75"));
        std::atomic<std::uint64_t> generation {1};
        {
            decodium::ft4::ScopedDecodeCancellation outer(generation, 0);
            QVERIFY(decode().isEmpty());
            {
                decodium::ft4::ScopedDecodeCancellation inner(generation, 1);
                QCOMPARE(decode(), baseline);
            }
            QVERIFY(decode().isEmpty());
        }
        QCOMPARE(decode(), baseline);
    }
    void independentThreadDoesNotInheritCancellation()
    {
        QStringList const baseline=decode();
        std::atomic<std::uint64_t> generation {1};
        {
            decodium::ft4::ScopedDecodeCancellation cancelled(generation, 0);
            QStringList independent;
            auto* worker = QThread::create([&] { independent=decode(); });
            // Match the native FT4 worker; macOS std::thread defaults to a
            // smaller stack than this decoder's local DSP workspaces need.
            worker->setStackSize(8 * 1024 * 1024);
            worker->start();
            bool const finished = worker->wait(30000);
            if (!finished) worker->wait();
            delete worker;
            QVERIFY(finished);
            QCOMPARE(independent, baseline);
            QVERIFY(decode().isEmpty());
        }
        QCOMPARE(decode(), baseline);
    }
};
QTEST_GUILESS_MAIN(Ft4CancellationTest)
#include "test_ft4_decode_cancellation.moc"
