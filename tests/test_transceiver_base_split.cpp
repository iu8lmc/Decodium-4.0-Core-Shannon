#include <QtTest>

#include "Transceiver/TransceiverBase.hpp"

// Drives the real TransceiverBase::set() with a recording rig.  The recorded
// commands are what a CAT back end (Hamlib, HRD, ...) would be asked to run,
// so the order and the presence of "tx:" before "ptt:on" is what matters.
namespace
{
  using Frequency = Transceiver::Frequency;
  using State = Transceiver::TransceiverState;

  constexpr Frequency kRx = 14074000;
  constexpr Frequency kTx1 = 14076000;
  constexpr Frequency kTx2 = 14077000;

  class RecordingRig final : public TransceiverBase
  {
  public:
    explicit RecordingRig (logger_type * logger)
      : TransceiverBase {logger, nullptr}
    {
    }

    QStringList commands;
    bool reject_rx {false};   // a rig that refuses the RX frequency change

  protected:
    int do_start () override {return 0;}
    void do_stop () override {}

    void do_frequency (Frequency f, MODE m, bool) override
    {
      commands << QStringLiteral ("rx:%1").arg (f);
      if (!reject_rx)
        {
          update_rx_frequency (f);
          if (m != UNK) update_mode (m);
        }
    }

    void do_tx_frequency (Frequency f, MODE, bool) override
    {
      commands << QStringLiteral ("tx:%1").arg (f);
      update_other_frequency (f);
      update_split (f != 0);
    }

    void do_mode (MODE m) override
    {
      commands << QStringLiteral ("mode:%1").arg (static_cast<int> (m));
      update_mode (m);
    }

    void do_ptt (bool on) override
    {
      commands << (on ? QStringLiteral ("ptt:on") : QStringLiteral ("ptt:off"));
      update_PTT (on);
    }
  };

  State request (Frequency rx, Frequency tx, bool ptt)
  {
    State s;
    s.online (true);
    s.frequency (rx);
    s.tx_frequency (tx);
    s.mode (Transceiver::DIG_U);
    s.ptt (ptt);
    return s;
  }
}

class TestTransceiverBaseSplit : public QObject
{
  Q_OBJECT

private:
  Transceiver::logger_type logger_;
  RecordingRig * rig_ {nullptr};
  unsigned seq_ {1};

  void send (State const& s) {rig_->set (s, seq_++);}

private slots:
  void init ()
  {
    rig_ = new RecordingRig {&logger_};
    seq_ = 1;
    // Bring the rig online and tune it; the RX frequency is now "unchanged"
    // for every request below, which is exactly the situation of H04.
    send (request (kRx, 0, false));
    QCOMPARE (rig_->commands, QStringList {QStringLiteral ("rx:%1").arg (kRx)});
    rig_->commands.clear ();
  }

  void cleanup ()
  {
    delete rig_;
    rig_ = nullptr;
  }

  // The reported defect: with the RX frequency unchanged the TX/split command
  // was skipped while PTT still went on.
  void splitIsSentBeforePttWhenRxIsUnchanged ()
  {
    send (request (kRx, kTx1, true));
    QCOMPARE (rig_->commands, (QStringList {QStringLiteral ("tx:%1").arg (kTx1),
                                            QStringLiteral ("ptt:on")}));
    QVERIFY (rig_->state ().split ());
    QCOMPARE (rig_->state ().tx_frequency (), kTx1);
  }

  void txQsyIsAppliedWithRxUnchanged ()
  {
    send (request (kRx, kTx1, false));
    rig_->commands.clear ();
    send (request (kRx, kTx2, false));
    QCOMPARE (rig_->commands, QStringList {QStringLiteral ("tx:%1").arg (kTx2)});
  }

  void splitIsSwitchedOffWhenTxBecomesZero ()
  {
    send (request (kRx, kTx1, false));
    rig_->commands.clear ();
    send (request (kRx, 0, false));
    QCOMPARE (rig_->commands, QStringList {QStringLiteral ("tx:0")});
    QVERIFY (!rig_->state ().split ());
  }

  void repeatedRequestIsNotResent ()
  {
    send (request (kRx, kTx1, false));
    rig_->commands.clear ();
    send (request (kRx, kTx1, false));
    QVERIFY2 (rig_->commands.isEmpty (), qPrintable (rig_->commands.join (',')));
  }

  void pttOffAfterSplitDoesNotTouchTx ()
  {
    send (request (kRx, kTx1, true));
    rig_->commands.clear ();
    send (request (kRx, kTx1, false));
    QCOMPARE (rig_->commands, QStringList {QStringLiteral ("ptt:off")});
  }

  void unchangedRequestSendsNothing ()
  {
    send (request (kRx, 0, false));
    QVERIFY2 (rig_->commands.isEmpty (), qPrintable (rig_->commands.join (',')));
  }

  // The branch that was moved must keep doing its own job.
  void rxQsyIsStillApplied ()
  {
    send (request (kRx + 1000, 0, false));
    QCOMPARE (rig_->commands, QStringList {QStringLiteral ("rx:%1").arg (kRx + 1000)});
  }

  void rejectedRxIsRetriedThenNotRepeated ()
  {
    rig_->reject_rx = true;
    send (request (kRx + 2000, 0, false));
    send (request (kRx + 2000, 0, false));
    QCOMPARE (rig_->commands.size (), 2);   // the refusal does not stick
    rig_->commands.clear ();

    rig_->reject_rx = false;
    send (request (kRx + 2000, 0, false));
    send (request (kRx + 2000, 0, false));
    QCOMPARE (rig_->commands, QStringList {QStringLiteral ("rx:%1").arg (kRx + 2000)});
  }
};

QTEST_GUILESS_MAIN (TestTransceiverBaseSplit)
#include "test_transceiver_base_split.moc"
