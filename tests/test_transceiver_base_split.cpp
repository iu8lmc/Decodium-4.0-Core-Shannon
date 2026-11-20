#include <QtTest>

#include "Transceiver/EmulateSplitTransceiver.hpp"
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
    bool state_commands_available {true};

  protected:
    int do_start () override {return 0;}
    void do_stop () override {}
    bool do_state_commands_available () const override {return state_commands_available;}

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

  // A deliberately non-Hamlib implementation with every optional CAT
  // capability enabled.  Fake It must expose this contract unchanged rather
  // than requiring callers to know the concrete wrapped type.
  class CapabilityRig final : public Transceiver
  {
  public:
    explicit CapabilityRig (logger_type * logger)
      : Transceiver {logger, nullptr}
    {
    }

    QString morse_text;
    int morse_wpm {0};
    int stop_morse_calls {0};
    int atu_tune_calls {0};
    int atu_off_calls {0};
    int power_on_calls {0};
    int power_off_calls {0};
    bool atu_enabled {true};

    void set (TransceiverState const& state, unsigned) noexcept override
    {
      last_set = state;
    }
    void start (unsigned) noexcept override {}
    void stop () noexcept override {}

    bool supports_morse () const noexcept override {return true;}
    int morse_text_limit () const noexcept override {return 49;}
    QString send_morse_checked (QString const& text, int wpm) override
    {
      morse_text = text;
      morse_wpm = wpm;
      return {};
    }
    void stop_morse () noexcept override {++stop_morse_calls;}

    bool supports_atu_control () const noexcept override {return true;}
    bool request_atu_tune (QString *) override {++atu_tune_calls; atu_enabled = true; return true;}
    bool request_atu_off (QString *) override {++atu_off_calls; atu_enabled = false; return true;}
    bool read_atu_state (bool * enabled, QString *) override
    {
      if (enabled) *enabled = atu_enabled;
      return true;
    }

    bool supports_power_control () const noexcept override {return true;}
    bool request_power_on (QString *) override {++power_on_calls; return true;}
    bool request_power_off (QString *) override {++power_off_calls; return true;}

    QVariantMap voice_controls (QString const& key, double value, bool write) override
    {
      return {{QStringLiteral ("key"), key},
              {QStringLiteral ("value"), value},
              {QStringLiteral ("write"), write}};
    }

    void report (TransceiverState const& state)
    {
      Q_EMIT update (state, 77);
    }

    TransceiverState last_set;
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

  void commandsAreDeferredUntilTheRigIsReady ()
  {
    rig_->state_commands_available = false;
    send (request (kRx + 1000, 0, false));
    QVERIFY2 (rig_->commands.isEmpty (), qPrintable (rig_->commands.join (',')));

    rig_->state_commands_available = true;
    send (request (kRx + 1000, 0, false));
    QCOMPARE (rig_->commands, QStringList {QStringLiteral ("rx:%1").arg (kRx + 1000)});
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

  void fakeItForwardsOptionalCatCapabilities ()
  {
    auto * wrapped = new CapabilityRig {&logger_};
    std::unique_ptr<Transceiver> ownership {wrapped};
    EmulateSplitTransceiver fake {&logger_, std::move (ownership)};

    QVERIFY (fake.supports_morse ());
    QCOMPARE (fake.morse_text_limit (), 49);
    QCOMPARE (fake.send_morse_checked (QStringLiteral ("CQ TEST"), 23), QString {});
    QCOMPARE (wrapped->morse_text, QStringLiteral ("CQ TEST"));
    QCOMPARE (wrapped->morse_wpm, 23);
    fake.stop_morse ();
    QCOMPARE (wrapped->stop_morse_calls, 1);

    QVERIFY (fake.supports_atu_control ());
    QString error;
    QVERIFY (fake.request_atu_tune (&error));
    QCOMPARE (wrapped->atu_tune_calls, 1);
    bool atuEnabled = false;
    QVERIFY (fake.read_atu_state (&atuEnabled, &error));
    QVERIFY (atuEnabled);
    QVERIFY (fake.request_atu_off (&error));
    QCOMPARE (wrapped->atu_off_calls, 1);
    QVERIFY (fake.read_atu_state (&atuEnabled, &error));
    QVERIFY (!atuEnabled);

    QVERIFY (fake.supports_power_control ());
    QVERIFY (fake.request_power_on (&error));
    QVERIFY (fake.request_power_off (&error));
    QCOMPARE (wrapped->power_on_calls, 1);
    QCOMPARE (wrapped->power_off_calls, 1);

    QVariantMap const voice = fake.voice_controls (QStringLiteral ("af"), 42.0, true);
    QCOMPARE (voice.value (QStringLiteral ("key")).toString (), QStringLiteral ("af"));
    QCOMPARE (voice.value (QStringLiteral ("value")).toDouble (), 42.0);
    QVERIFY (voice.value (QStringLiteral ("write")).toBool ());
  }

  void fakeItKeepsRxFrequencyForUnkeyedTxEcho ()
  {
    auto * wrapped = new CapabilityRig {&logger_};
    std::unique_ptr<Transceiver> ownership {wrapped};
    EmulateSplitTransceiver fake {&logger_, std::move (ownership)};
    QList<State> updates;
    QObject::connect (&fake, &Transceiver::update, [&updates] (State const& state, unsigned) {
      updates << state;
    });

    State desired = request (kRx, kTx1, false);
    desired.split (true);
    fake.set (desired, 1);
    State echo = request (kTx1, 0, false);
    wrapped->report (echo);

    QCOMPARE (updates.size (), 1);
    QCOMPARE (updates.constLast ().frequency (), kRx);
    QCOMPARE (updates.constLast ().tx_frequency (), kTx1);
    QVERIFY (updates.constLast ().split ());
  }

  void fakeItStillFollowsManualRxQsy ()
  {
    auto * wrapped = new CapabilityRig {&logger_};
    std::unique_ptr<Transceiver> ownership {wrapped};
    EmulateSplitTransceiver fake {&logger_, std::move (ownership)};
    QList<State> updates;
    QObject::connect (&fake, &Transceiver::update, [&updates] (State const& state, unsigned) {
      updates << state;
    });

    State desired = request (kRx, kTx1, false);
    desired.split (true);
    fake.set (desired, 1);
    State qsy = request (kRx + 1500, 0, false);
    wrapped->report (qsy);

    QCOMPARE (updates.size (), 1);
    QCOMPARE (updates.constLast ().frequency (), kRx + 1500);
    QCOMPARE (updates.constLast ().tx_frequency (), kTx1);
    QVERIFY (updates.constLast ().split ());
  }

  void fakeItStillFollowsManualRxQsyDuringTransmit ()
  {
    auto * wrapped = new CapabilityRig {&logger_};
    std::unique_ptr<Transceiver> ownership {wrapped};
    EmulateSplitTransceiver fake {&logger_, std::move (ownership)};
    QList<State> updates;
    QObject::connect (&fake, &Transceiver::update, [&updates] (State const& state, unsigned) {
      updates << state;
    });

    State desired = request (kRx, kTx1, false);
    desired.split (true);
    fake.set (desired, 1);
    State qsy = request (kRx + 1500, 0, true);
    wrapped->report (qsy);

    QCOMPARE (updates.size (), 1);
    QCOMPARE (updates.constLast ().frequency (), kRx + 1500);
    QCOMPARE (updates.constLast ().tx_frequency (), kTx1);
    QVERIFY (updates.constLast ().ptt ());
    QVERIFY (updates.constLast ().split ());
  }

};

QTEST_GUILESS_MAIN (TestTransceiverBaseSplit)
#include "test_transceiver_base_split.moc"
