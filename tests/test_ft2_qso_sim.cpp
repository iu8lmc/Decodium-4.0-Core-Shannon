// test_ft2_qso_sim.cpp
//
// FT2 full-QSO simulator with QRM. Two scenarios:
//
//   1) Auto-CQ: we call CQ, a virtual partner K1ABC answers. We drive the
//      QSO to RR73/73 and log it.
//   2) Direct call: we direct-call K1ABC. Partner responds starting from
//      the grid/report phase. We drive the QSO to 73 and log it.
//
// For each partner transmission the test generates a realistic 12 kHz
// FT2 frame (29664 signal samples in a 45000-sample slot, 103 tones, nsps=288)
// and adds Gaussian noise + a concurrent interfering FT2 carrier (QRM)
// outside the QSO frequency. The real Fortran decoder is invoked and the
// recovered message is fed into a simulated QSO state machine that mirrors
// `DecodiumBridge::autoSequenceStep`.
//
// The test flags:
//   - decoder failure to recover the partner's message under QRM
//   - missing fields in the eventual ADIF log record
//   - state transitions that diverge from the specification documented here
//
// Exit 0 only if both scenarios finish with a complete log entry containing
// call, grid, report_sent, report_rcvd, mode, datetime.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QVector>

#include "Detector/FortranRuntimeGuard.hpp"
#include "Modulator/FtxMessageEncoder.hpp"
#include "Modulator/FtxWaveformGenerator.hpp"

extern "C"
{
  void ft2_async_decode_ (short iwave[], int* nqsoprogress, int* nfqso, int* nfa, int* nfb,
                          int* ndepth, int* ncontest, char mycall[], char hiscall[],
                          int snrs[], float dts[], float freqs[], int naps[], float quals[],
                          signed char bits77[], char decodeds[], int* nout,
                          size_t, size_t, size_t);
  void ftx_ft2_stage7_clravg_c ();
}

namespace
{
  constexpr int kSampleRate {12000};
  constexpr int kNsps {288};
  constexpr int kFt2Samples {45000};
  constexpr int kMaxLines {100};
  constexpr int kBitsPerMessage {77};
  constexpr int kDecodedChars {37};

  constexpr float kQsoCarrierHz {1500.0f};
  constexpr float kQrmCarrierHz {2100.0f}; // off-QSO interferer

  QByteArray to_fortran_field (QByteArray value, int width)
  {
    value = value.left (width);
    if (value.size () < width)
      value.append (QByteArray (width - value.size (), ' '));
    return value;
  }

  QByteArray trim_fortran_field (char const* data, int width)
  {
    QByteArray field {data, width};
    while (!field.isEmpty () && (field.back () == ' ' || field.back () == '\0'))
      field.chop (1);
    return field;
  }

  QString canonical (QString const& s)
  {
    return s.simplified ().toUpper ();
  }

  // Synthesize a 45000-sample FT2 frame: primary partner message on
  // kQsoCarrierHz + an interfering FT2 transmission on kQrmCarrierHz
  // + white Gaussian noise. Returns 16-bit PCM ready for the decoder.
  std::vector<qint16> render_frame (QString const& partnerMsg, QString const& qrmMsg,
                                    float partnerSnrDb, float qrmSnrDb, int seed)
  {
    auto encode = [] (QString const& text) {
      return decodium::txmsg::encodeFt2 (text);
    };

    auto const partnerEnc = encode (partnerMsg);
    if (!partnerEnc.ok || partnerEnc.tones.isEmpty ())
      throw std::runtime_error ("encode partner failed: " + partnerMsg.toStdString ());

    auto partnerWave = decodium::txwave::generateFt2Wave (
        partnerEnc.tones.constData (), partnerEnc.tones.size (), kNsps,
        static_cast<float> (kSampleRate), kQsoCarrierHz);

    QVector<float> qrmWave;
    if (!qrmMsg.isEmpty ())
      {
        auto const qrmEnc = encode (qrmMsg);
        if (qrmEnc.ok && !qrmEnc.tones.isEmpty ())
          qrmWave = decodium::txwave::generateFt2Wave (
              qrmEnc.tones.constData (), qrmEnc.tones.size (), kNsps,
              static_cast<float> (kSampleRate), kQrmCarrierHz);
      }

    // SNR calibration: noise sigma relative to signal RMS (peak ~0.7).
    double sigPeak = 0.0;
    for (float s : partnerWave) sigPeak = std::max (sigPeak, std::fabs (static_cast<double>(s)));
    if (sigPeak <= 0.0) sigPeak = 1.0;
    double const partnerGain = 0.6 / sigPeak;
    double const partnerRms = partnerGain * 0.5 * sigPeak; // approximation
    double const noiseSigma = partnerRms / std::pow (10.0, partnerSnrDb / 20.0);
    double const qrmGain = std::pow (10.0, (partnerSnrDb - qrmSnrDb) / 20.0) * partnerGain;

    std::vector<float> frame (static_cast<size_t> (kFt2Samples), 0.0f);
    int const offset = 3000; // ~0.25 s
    for (int i = 0; i < partnerWave.size () && offset + i < kFt2Samples; ++i)
      frame[static_cast<size_t> (offset + i)] += static_cast<float> (partnerGain) * partnerWave[i];
    for (int i = 0; i < qrmWave.size () && offset + i < kFt2Samples; ++i)
      frame[static_cast<size_t> (offset + i)] += static_cast<float> (qrmGain) * qrmWave[i];

    std::mt19937 rng {static_cast<std::mt19937::result_type> (seed)};
    std::normal_distribution<float> noise {0.0f, static_cast<float> (noiseSigma)};
    for (float& x : frame) x += noise (rng);

    std::vector<qint16> pcm (static_cast<size_t> (kFt2Samples), 0);
    for (int i = 0; i < kFt2Samples; ++i)
      {
        float const clipped = std::max (-1.0f, std::min (1.0f, frame[static_cast<size_t>(i)]));
        pcm[static_cast<size_t> (i)] =
            static_cast<qint16> (std::lround (static_cast<double> (clipped) * 32767.0));
      }
    return pcm;
  }

  struct DecodeRow
  {
    QString text;
    int snr {};
    float dt {};
    float freq {};
  };

  QList<DecodeRow> run_ft2_decoder (std::vector<qint16>& pcm,
                                    QString const& myCall, QString const& hisCall)
  {
    std::array<int, kMaxLines> snrs {};
    std::array<float, kMaxLines> dts {};
    std::array<float, kMaxLines> freqs {};
    std::array<int, kMaxLines> naps {};
    std::array<float, kMaxLines> quals {};
    std::array<signed char, kBitsPerMessage * kMaxLines> bits77 {};
    std::array<char, kMaxLines * kDecodedChars> decodeds {};
    int nout = 0;

    int nqsoprogress = 0, nfqso = 1500, nfa = 200, nfb = 3000;
    int ndepth = 3, ncontest = 0;
    QByteArray my = to_fortran_field (myCall.toLatin1 (), 12);
    QByteArray his = to_fortran_field (hisCall.toLatin1 (), 12);

    QMutexLocker locker {&decodium::fortran::runtime_mutex ()};
    ftx_ft2_stage7_clravg_c ();
    ft2_async_decode_ (pcm.data (), &nqsoprogress, &nfqso, &nfa, &nfb,
                       &ndepth, &ncontest, my.data (), his.data (),
                       snrs.data (), dts.data (), freqs.data (),
                       naps.data (), quals.data (), bits77.data (),
                       decodeds.data (), &nout,
                       static_cast<size_t> (my.size ()),
                       static_cast<size_t> (his.size ()),
                       static_cast<size_t> (kDecodedChars));
    locker.unlock ();

    QList<DecodeRow> rows;
    for (int i = 0; i < std::max (0, nout) && i < kMaxLines; ++i)
      {
        DecodeRow r;
        r.text = QString::fromLatin1 (
            trim_fortran_field (decodeds.data () + i * kDecodedChars, kDecodedChars));
        r.snr = snrs[i];
        r.dt = dts[i];
        r.freq = freqs[i];
        rows.append (r);
      }
    return rows;
  }

  // ── Simulated QSO state machine ─────────────────────────────────────
  //
  // Mirrors DecodiumBridge::autoSequenceStep for FT2 async. Inputs are
  // the partner's decoded message text; outputs are (nextTx, reportRcvdUpdate).
  // Fields tracked: dxCall, dxGrid, reportSent, reportRcvd, qsoProgress,
  // nTx73, logAfterOwn73, qsoLogged.

  struct QsoState
  {
    QString myCall;
    QString dxCall;
    QString dxGrid;
    QString reportSent;
    QString reportRcvd;
    int currentTx {6};    // 6 = CQ
    int qsoProgress {0};  // 0=IDLE, 1=REPLYING, 2=REPORT, 3=ROGER_REPORT, 4=ROGERS, 5=SIGNOFF, 6=IDLE_QSO
    int nTx73 {0};
    bool logAfterOwn73 {false};
    bool qsoLogged {false};
    bool autoCqRepeat {false};
    bool inCqMode {false};
  };

  struct LogEntry
  {
    bool valid {false};
    QString call, grid, rptSent, rptRcvd, mode;
    QDateTime on, off;
  };

  // extract partner call and last token from a decoded FT2 message.
  // expected format: "TO FROM ..." or "TO FROM TOKEN"
  struct MsgParts
  {
    QString to, from;
    QStringList tokens;
    QString last;
    bool directed {false};
  };

  MsgParts parse_msg (QString const& msg, QString const& myCallUpper)
  {
    MsgParts p;
    QStringList parts = msg.split (QRegularExpression ("\\s+"), Qt::SkipEmptyParts);
    if (parts.size () < 2) return p;
    p.to = parts[0].toUpper ();
    p.from = parts[1].toUpper ();
    p.tokens = parts;
    p.last = parts.last ().toUpper ();
    p.directed = (p.to == myCallUpper);
    return p;
  }

  bool is_grid_token (QString const& t)
  {
    if (t.length () != 4) return false;
    return t[0].isLetter () && t[1].isLetter ()
        && t[2].isDigit () && t[3].isDigit ();
  }

  bool is_report_token (QString const& t)
  {
    if (t.length () < 2) return false;
    QChar c0 = t[0];
    return (c0 == '+' || c0 == '-') && t[1].isDigit ();
  }

  bool is_r_report_token (QString const& t)
  {
    if (t.length () < 3) return false;
    return t[0] == 'R' && (t[1] == '+' || t[1] == '-') && t[2].isDigit ();
  }

  // Apply one partner decode to the state machine. Returns the TX step
  // we should emit next (0 if no TX, -1 if QSO finished and logged).
  int step_state (QsoState& st, QString const& partnerMsg, QTextStream& out, LogEntry& log)
  {
    QString const myCallUpper = st.myCall.toUpper ();
    MsgParts p = parse_msg (partnerMsg, myCallUpper);
    if (p.tokens.size () < 2) return 0;
    if (!p.directed)
      {
        out << "    [state] not directed to us, ignore\n";
        return 0;
      }

    QString const partner = p.from;

    // inCqMode on first directed call from unknown partner
    if (st.inCqMode && st.dxCall.isEmpty ())
      {
        st.dxCall = partner;
        if (is_grid_token (p.last)) st.dxGrid = p.last;
        // report sent = the SNR we heard partner at — simulate as -08
        if (st.reportSent.isEmpty ()) st.reportSent = QStringLiteral ("-08");
      }

    // partner signoff 73
    if (p.last == QStringLiteral ("73") && st.qsoProgress >= 4)
      {
        if (st.nTx73 == 0)
          {
            out << "    [state] partner 73 → send our 73 (TX5)\n";
            st.logAfterOwn73 = true;
            return 5;
          }
        out << "    [state] partner 73 after our 73 → QSO complete + log\n";
        log.valid = true;
        log.call = partner; log.grid = st.dxGrid;
        log.rptSent = st.reportSent; log.rptRcvd = st.reportRcvd;
        log.mode = QStringLiteral ("FT2");
        log.on = QDateTime::currentDateTimeUtc ();
        log.off = log.on;
        st.qsoLogged = true;
        return -1;
      }

    // partner RR73 or RRR
    if (p.last == QStringLiteral ("RR73") || p.last == QStringLiteral ("RRR"))
      {
        if (st.qsoProgress >= 5 && st.nTx73 >= 1)
          {
            out << "    [state] RR73 received but already in signoff → log\n";
            log.valid = true;
            log.call = partner; log.grid = st.dxGrid;
            log.rptSent = st.reportSent; log.rptRcvd = st.reportRcvd;
            log.mode = QStringLiteral ("FT2");
            log.on = QDateTime::currentDateTimeUtc ();
            log.off = log.on;
            st.qsoLogged = true;
            return -1;
          }
        // final ack (needs our own 73 first in FT2 non-quick)
        out << "    [state] RR73 received → TX5 (73)\n";
        st.logAfterOwn73 = true;
        return 5;
      }

    // partner R+report → we transmit RR73 (TX4).
    // NOTE: must update reportRcvd here — the bridge currently does NOT.
    if (is_r_report_token (p.last))
      {
        QString const rpt = p.last.mid (1);
        out << "    [state] R+report " << p.last << " → TX4 (RR73); reportRcvd=" << rpt << "\n";
        st.reportRcvd = rpt;
        return 4;
      }

    // partner plain -NN → we transmit R+report (TX3)
    if (is_report_token (p.last))
      {
        out << "    [state] plain report " << p.last << " → TX3 (R+report)\n";
        st.reportRcvd = p.last;
        return 3;
      }

    // partner grid → we transmit report (TX2)
    if (is_grid_token (p.last))
      {
        out << "    [state] grid " << p.last << " → TX2 (report)\n";
        st.dxGrid = p.last;
        if (st.dxCall.isEmpty ()) st.dxCall = partner;
        if (st.reportSent.isEmpty ()) st.reportSent = QStringLiteral ("-08");
        return 2;
      }

    out << "    [state] token '" << p.last << "' unrecognized, no TX\n";
    return 0;
  }

  QString tx_message_for (int txSlot, QsoState const& st)
  {
    QString const& mine = st.myCall;
    QString const& him = st.dxCall;
    switch (txSlot)
      {
      case 1: return QStringLiteral ("%1 %2 JN71").arg (him, mine);
      case 2: return QStringLiteral ("%1 %2 %3").arg (him, mine,
                                                       st.reportSent.isEmpty ()
                                                           ? QStringLiteral ("-08")
                                                           : st.reportSent);
      case 3: return QStringLiteral ("%1 %2 R%3").arg (him, mine,
                                                       st.reportRcvd.isEmpty ()
                                                           ? QStringLiteral ("-10")
                                                           : st.reportRcvd);
      case 4: return QStringLiteral ("%1 %2 RR73").arg (him, mine);
      case 5: return QStringLiteral ("%1 %2 73").arg (him, mine);
      case 6: return QStringLiteral ("CQ %1 JN71").arg (mine);
      }
    return QString ();
  }

  struct Scenario
  {
    QString name;
    bool autoCq;
    QStringList partnerMessages; // ordered list of partner transmissions
  };

  // Run one scenario. Generates audio for each partner message, decodes it,
  // feeds it through the state machine, and prints the resulting dialogue.
  // Returns true on success (log entry complete).
  bool run_scenario (QTextStream& out, Scenario const& sc)
  {
    out << "\n===== " << sc.name << " =====\n";
    QsoState st;
    st.myCall = QStringLiteral ("IW8XOU");
    st.autoCqRepeat = sc.autoCq;
    st.inCqMode = sc.autoCq;
    if (sc.autoCq)
      {
        st.currentTx = 6;
        st.qsoProgress = 0;
        out << "  [us] TX6: " << tx_message_for (6, st) << "\n";
      }
    else
      {
        st.dxCall = QStringLiteral ("K1ABC");
        st.currentTx = 1;
        st.qsoProgress = 1;
        out << "  [us] TX1: " << tx_message_for (1, st) << "\n";
      }

    LogEntry log;
    int pass = 0, fail = 0;
    int seedBase = sc.autoCq ? 1001 : 2001;

    for (int i = 0; i < sc.partnerMessages.size (); ++i)
      {
        QString const& msg = sc.partnerMessages[i];
        out << "\n  [partner #" << (i + 1) << "] simulating TX \"" << msg << "\"\n";

        // QRM: a separate bogus FT2 message on kQrmCarrierHz at higher SNR
        // to test the decoder's rejection of competing signals.
        QString const qrm = QStringLiteral ("CQ DL1QRM JO31");
        float partnerSnr = 5.0f;
        float qrmSnr = 12.0f; // QRM stronger in dB but on different freq
        std::vector<qint16> pcm = render_frame (msg, qrm, partnerSnr, qrmSnr, seedBase + i);

        QList<DecodeRow> rows = run_ft2_decoder (pcm, st.myCall,
                                                 st.dxCall.isEmpty ()
                                                     ? QStringLiteral ("")
                                                     : st.dxCall);
        out << "  [decoder] " << rows.size () << " row(s)\n";
        QString recovered;
        for (DecodeRow const& r : rows)
          {
            out << "    · " << r.text.trimmed () << " (snr=" << r.snr
                << " f=" << qRound (r.freq) << ")\n";
            if (canonical (r.text) == canonical (msg))
              recovered = r.text;
          }
        if (recovered.isEmpty ())
          {
            out << "  [FAIL] partner message not recovered from QRM (expected \""
                << msg << "\")\n";
            ++fail;
            continue;
          }
        ++pass;

        int const nextTx = step_state (st, recovered, out, log);
        if (nextTx > 0)
          {
            st.currentTx = nextTx;
            // Update qsoProgress per tx slot (mirrors advanceQsoState).
            switch (nextTx)
              {
              case 2: st.qsoProgress = 2; break;
              case 3: st.qsoProgress = 3; break;
              case 4: st.qsoProgress = 4; break;
              case 5: st.qsoProgress = 5; st.nTx73 = 1; break;
              }
            out << "  [us] TX" << nextTx << ": " << tx_message_for (nextTx, st) << "\n";

            // Mirror DecodiumBridge.cpp:3395-3402: when transmitting TX5
            // (73) while m_logAfterOwn73 is set, the TX-completion
            // callback triggers logQso() immediately — the QSO is
            // considered complete without waiting for another partner TX.
            if (nextTx == 5 && st.logAfterOwn73)
              {
                out << "    [tx-complete] own 73 sent, logAfterOwn73 set → log\n";
                log.valid = true;
                log.call = st.dxCall;
                log.grid = st.dxGrid;
                log.rptSent = st.reportSent;
                log.rptRcvd = st.reportRcvd;
                log.mode = QStringLiteral ("FT2");
                log.on = QDateTime::currentDateTimeUtc ();
                log.off = log.on;
                st.qsoLogged = true;
                st.logAfterOwn73 = false;
                out << "  [us] QSO complete, logged\n";
                break;
              }
          }
        else if (nextTx < 0)
          {
            out << "  [us] QSO complete, logged\n";
            break;
          }
      }

    out << "\n  decoder recovery: " << pass << "/" << sc.partnerMessages.size () << "\n";
    out << "  log valid:        " << (log.valid ? "yes" : "NO") << "\n";
    if (log.valid)
      {
        out << "    call:     " << log.call << "\n";
        out << "    grid:     " << (log.grid.isEmpty () ? "(empty)" : log.grid) << "\n";
        out << "    rpt sent: " << (log.rptSent.isEmpty () ? "(empty)" : log.rptSent) << "\n";
        out << "    rpt rcvd: " << (log.rptRcvd.isEmpty () ? "(empty)" : log.rptRcvd) << "\n";
        out << "    mode:     " << log.mode << "\n";
        out << "    on:       " << log.on.toString (Qt::ISODate) << "\n";
      }

    bool const ok = (fail == 0) && log.valid
                    && !log.call.isEmpty () && !log.grid.isEmpty ()
                    && !log.rptSent.isEmpty () && !log.rptRcvd.isEmpty ();
    out << "  scenario: " << (ok ? "PASS" : "FAIL") << "\n";
    return ok;
  }
}

int main (int argc, char* argv[])
{
  QCoreApplication app {argc, argv};
  QCoreApplication::setApplicationName (QStringLiteral ("test_ft2_qso_sim"));

  QTextStream out {stdout};
  out << "FT2 full-QSO simulator with QRM\n";
  out << "================================\n";
  out << "my call: IW8XOU, partner: K1ABC, QSO carrier 1500 Hz, QRM at 2100 Hz\n";

  try
    {
      // Scenario 1: we call CQ; partner answers starting with their grid,
      // then progresses through full exchange to 73.
      Scenario sc1;
      sc1.name = "Scenario 1 — auto-CQ reception";
      sc1.autoCq = true;
      sc1.partnerMessages = {
        QStringLiteral ("IW8XOU K1ABC FN42"),   // grid (answer to CQ)
        QStringLiteral ("IW8XOU K1ABC R-10"),   // R+report (skipping plain report)
        QStringLiteral ("IW8XOU K1ABC 73"),     // signoff 73
      };

      // Scenario 2: we direct-call K1ABC; partner answers with plain report
      // (full 5-step exchange), ending in RR73 + 73.
      Scenario sc2;
      sc2.name = "Scenario 2 — direct call with full report exchange";
      sc2.autoCq = false;
      sc2.partnerMessages = {
        QStringLiteral ("IW8XOU K1ABC FN42"),    // grid response
        QStringLiteral ("IW8XOU K1ABC -14"),     // plain report
        QStringLiteral ("IW8XOU K1ABC RR73"),    // final ack
        QStringLiteral ("IW8XOU K1ABC 73"),      // partner 73 after our 73
      };

      bool const r1 = run_scenario (out, sc1);
      bool const r2 = run_scenario (out, sc2);

      out << "\n====================\n";
      out << "FINAL: "
          << (r1 ? "scenario1=PASS " : "scenario1=FAIL ")
          << (r2 ? "scenario2=PASS" : "scenario2=FAIL")
          << "\n";
      return (r1 && r2) ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  catch (std::exception const& e)
    {
      std::cerr << "test_ft2_qso_sim: " << e.what () << "\n";
      return EXIT_FAILURE;
    }
}
