#include <QtTest>
#include <QScopeGuard>
#include <algorithm>
#include <array>
#include <cmath>
#include <random>

#include "Detector/FT8DecodeWorker.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"

extern "C" {
void ftx_ft8_set_ap_mycall_enabled_c (int enabled);
int ftx_ft8_ap_mycall_enabled_c ();
int ftx_ft8_prepare_ap_pass_c (int ipass, int progress, int cqonly, int contest,
                             int nfqso, int nftx, float frequency, int width,
                             int const* apsym, int const* aph10,
                             float const* llra, float const* llrc,
                             float* llrz, int* mask, int* type);
int ftx_encode_ft8_candidate_c (char const* message, char* sent, int* tones,
                                signed char* codeword);
}

class TestFt8MyCallAp : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void eligibility ()
  {
    using decodium::ft8::allowMyCallAp;
    constexpr qint64 now = 1000000;
    QVERIFY (!allowMyCallAp (false, false, 0, now));
    QVERIFY (allowMyCallAp (false, true, 0, now));
    QVERIFY (allowMyCallAp (true, false, 0, now));
    QVERIFY (allowMyCallAp (false, false, now, now));
    QVERIFY (allowMyCallAp (false, false, now - 179999, now));
    QVERIFY (!allowMyCallAp (false, false, now - 180000, now));
    QVERIFY (!allowMyCallAp (false, false, now + 1, now));
  }

  void mycallPassSuppressedButCqRetained ()
  {
    int const previous = ftx_ft8_ap_mycall_enabled_c ();
    auto const restore = qScopeGuard ([previous] {
      ftx_ft8_set_ap_mycall_enabled_c (previous);
    });
    std::array<int, 58> symbols;
    std::array<int, 10> fox;
    std::array<float, 174> a, c, z;
    std::array<int, 174> mask;
    symbols.fill (1); fox.fill (-1); a.fill (0.5f); c.fill (0.25f);
    int type = -1;
    auto prepare = [&] (int cqonly) {
      return ftx_ft8_prepare_ap_pass_c (6, 3, cqonly, 0, 1500, 1600, 1505.f, 100,
          symbols.data (), fox.data (), a.data (), c.data (), z.data (), mask.data (), &type);
    };
    ftx_ft8_set_ap_mycall_enabled_c (1);
    QCOMPARE (prepare (0), 1);
    QCOMPARE (type, 3);
    ftx_ft8_set_ap_mycall_enabled_c (0);
    QCOMPARE (prepare (0), 0);
    QCOMPARE (prepare (1), 1);
    QCOMPARE (type, 1);
  }

  void workerApSnapshot_data ()
  {
    QTest::addColumn<bool> ("mycall");
    QTest::addColumn<bool> ("ap");
    QTest::addColumn<bool> ("pressure");
    QTest::newRow ("idle-listening") << false << true << false;
    QTest::newRow ("recent-tx") << true << true << false;
    QTest::newRow ("ap-disabled") << true << false << false;
    QTest::newRow ("cpu-pressure") << true << true << true;
  }

  void workerApSnapshot ()
  {
    QFETCH (bool, mycall);
    QFETCH (bool, ap);
    QFETCH (bool, pressure);
    bool const expected = mycall && ap && !pressure;
    int const previous = ftx_ft8_ap_mycall_enabled_c ();
    auto const restore = qScopeGuard ([previous] {
      ftx_ft8_set_ap_mycall_enabled_c (previous);
    });
    // Deliberately disagree with the queued request: the worker must apply its
    // snapshot during decoding, then restore the enclosing runtime's state.
    ftx_ft8_set_ap_mycall_enabled_c (expected ? 0 : 1);
    decodium::ft8::FT8DecodeWorker worker;
    decodium::ft8::DecodeRequest request;
    request.audio.fill (0, 180000);
    request.availableSamples = request.audio.size ();
    request.nfa = 1300; request.nfb = 1700;
    request.nfqso = request.nftx = 1500;
    request.lft8apon = ap;
    request.apMyCallEnabled = mycall;
    request.cpuPressureLimited = pressure;
    request.maxDecodeMs = 1000;
    request.mycall = "9H1SR";
    request.hiscall = "K1ABC";
    int observed = -1;
    int deliveries = 0;
    connect (&worker, &decodium::ft8::FT8DecodeWorker::decodeReady, this,
             [&] (quint64, QStringList) {
      observed = ftx_ft8_ap_mycall_enabled_c ();
      ++deliveries;
    }, Qt::DirectConnection);
    worker.decode (request);
    QCOMPARE (deliveries, 1);
    QCOMPARE (observed, expected ? 1 : 0);
    QCOMPARE (ftx_ft8_ap_mycall_enabled_c (), expected ? 0 : 1);
  }

  void realDirectedMessageStillDecodesWhileIdle ()
  {
    std::array<int, 79> tones {};
    std::array<signed char, 174> bits {};
    std::array<char, 37> sent {};
    QByteArray const message = QByteArray ("9H1SR K1ABC -11").leftJustified (37, ' ');
    QVERIFY (ftx_encode_ft8_candidate_c (message.constData (), sent.data (),
                                        tones.data (), bits.data ()) != 0);
    auto const wave = decodium::txwave::generateFt8Wave (
        tones.data (), 79, 1920, 2.f, 12000.f, 1500.f);
    QVERIFY (!wave.isEmpty ());
    decodium::ft8::DecodeRequest request;
    request.audio.fill (0, 180000);
    std::mt19937 random (618);
    std::normal_distribution<float> noise (0.f, 200.f);
    for (auto& sample : request.audio) sample = short (noise (random));
    for (int i = 0; i < wave.size () && i + 6000 < request.audio.size (); ++i)
      request.audio[i + 6000] += short (2000.f * wave[i]);
    request.availableSamples = request.audio.size ();
    request.nfa = 1300; request.nfb = 1700;
    request.nfqso = request.nftx = 1500;
    request.lft8apon = 1;
    request.apMyCallEnabled = false;
    request.mycall = "9H1SR";
    request.hiscall = "K1ABC";
    decodium::ft8::FT8DecodeWorker worker;
    QStringList decoded;
    connect (&worker, &decodium::ft8::FT8DecodeWorker::decodeReady, this,
             [&] (quint64, QStringList rows) { decoded = rows; }, Qt::DirectConnection);
    worker.decode (request);
    QVERIFY2 (decoded.join ('\n').contains (QStringLiteral ("9H1SR K1ABC -11")),
              qPrintable (decoded.join ('\n')));
  }

  void cleanupTestCase () { decodium::ft8::shutdownHashSeedWorker (); }
};

QTEST_GUILESS_MAIN (TestFt8MyCallAp)
#include "test_ft8_mycall_ap.moc"
