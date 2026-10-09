#include "CwController.h"

#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <limits>

using namespace decodium::cw;

namespace {

// Un testo in CW come audio mono a 16 bit: il tono acceso per i punti e le
// linee, spento per gli spazi. Le tre lunghezze sono quelle di sempre: punto 1,
// linea 3, spazio fra i segni 1, fra le lettere 3, fra le parole 7.
QVector<short> morseAudio(const QString& text, int wpm, int toneHz, int rate, double amp = 8000.0)
{
    const double dot = 1.2 / wpm;
    QVector<short> out;
    auto tone = [&](double seconds) {
        const int n = int(seconds * rate);
        for (int i = 0; i < n; ++i) {
            // un po' di rampa agli estremi, come ogni manipolatore fatto bene
            const double edge = std::min({1.0, i / (0.004 * rate), (n - i) / (0.004 * rate)});
            out.append(short(amp * edge * std::sin(2.0 * M_PI * toneHz * i / rate)));
        }
    };
    auto silence = [&](double seconds) { out.append(QVector<short>(int(seconds * rate), 0)); };
    silence(0.5);
    for (QChar c : text.toUpper()) {
        if (c == QLatin1Char(' ')) { silence(dot * 4); continue; }   // 4 + i 3 della lettera = 7
        const QString m = CwKeyer::morseOf(c);
        for (QChar s : m) {
            tone(s == QLatin1Char('.') ? dot : dot * 3);
            silence(dot);
        }
        silence(dot * 2);                                             // 1 + 2 = 3
    }
    silence(1.5);
    return out;
}

struct KeyRig {
    bool remote {true};
    bool supported {true};
    bool failSend {false};
    QVector<bool> ptt;
    QVector<QVector<KeyEvent>> pieces;
    QVector<int> tones;
    int audioCalls {0};
    QVector<QString> order;     // "ptt1", "key", "ptt0"

    CwController::Hooks hooks()
    {
        CwController::Hooks h;
        h.canTransmit = [] { return true; };
        h.remoteRadio = [this] { return remote; };
        h.remoteKeySupported = [this] { return supported; };
        h.remoteLeadMs = [] { return 10; };
        h.remotePtt = [this](bool on) { ptt.append(on); order.append(on ? "ptt1" : "ptt0"); };
        h.sendKey = [this](const QVector<KeyEvent>& ev, int tone) {
            if (failSend) return false;
            pieces.append(ev); tones.append(tone); order.append("key");
            return true;
        };
        h.sendAudio = [this](const QString&, int) { ++audioCalls; return true; };
        return h;
    }
    QVector<KeyEvent> all() const
    {
        QVector<KeyEvent> out;
        for (const auto& p : pieces) out += p;
        return out;
    }
};


}  // namespace

class TestCwController : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString iniPath() const { return m_dir.filePath("cw.ini"); }

private slots:
    void init() { QFile::remove(iniPath()); }

    void vfoRejectsInvalidTargets()
    {
        CwController c;
        double dial = 14025000;
        bool connected = true;
        CwController::Hooks h;
        h.frequency = [&] { return dial; };
        h.tuneFrequency = [&](double hz) { if (!connected) return false; dial = hz; return true; };
        c.setHooks(h);
        QVERIFY(c.tuneBy(1)); QCOMPARE(dial, 14025001.0);
        QVERIFY(c.tuneBy(-10)); QCOMPARE(dial, 14024991.0);
        QVERIFY(!c.tuneTo(-1));
        QVERIFY(!c.tuneTo(std::numeric_limits<double>::quiet_NaN()));
        connected = false;
        QVERIFY(!c.tuneBy(100)); QCOMPARE(dial, 14024991.0);
    }
    void centerMeasuredTone_data()
    {
        QTest::addColumn<bool>("lower"); QTest::addColumn<int>("pitch");
        QTest::newRow("upper-high") << false << 800;
        QTest::newRow("lower-high") << true << 800;
        QTest::newRow("upper-low") << false << 600;
        QTest::newRow("lower-low") << true << 600;
    }
    void centerMeasuredTone()
    {
        QFETCH(bool, lower); QFETCH(int, pitch);
        CwController c; c.setTuningLowerSideband(lower);
        double dial = 14025000;
        CwController::Hooks h;
        h.frequency = [&] { return dial; };
        h.tuneFrequency = [&](double hz) { dial = hz; return true; };
        c.setHooks(h);
        QVERIFY(!c.centerSignal());
        auto audio = morseAudio("CQ CQ CQ DE IU8LMC TEST TEST", 20, pitch, 12000);
        audio.resize(audio.size() - 12000);
        c.feedRxAudio(audio, 12000);
        QVERIFY(c.decoderScope().value("reading").toBool());
        QVERIFY(c.centerSignal());
        const double expected = 14025000 + (lower ? -1 : 1) * (pitch - 700);
        QVERIFY2(std::abs(dial - expected) < 12, qPrintable(QString::number(dial)));
        const double once = dial;
        QVERIFY(!c.centerSignal()); QCOMPARE(dial, once);
    }
    void centerRejectsLockedDecoderAndStaleAudio()
    {
        CwController c; int calls = 0;
        CwController::Hooks h;
        h.frequency = [] { return 14025000.; };
        h.tuneFrequency = [&](double) { ++calls; return true; };
        c.setHooks(h);
        auto audio = morseAudio("CQ CQ CQ DE IU8LMC TEST", 20, 800, 12000);
        c.setDecoderToneLock(800); c.feedRxAudio(audio, 12000);
        QVERIFY(!c.centerSignal()); QCOMPARE(calls, 0);
        c.setDecoderToneLock(0); c.feedRxAudio(audio, 12000);
        QTest::qWait(800);
        QVERIFY(!c.centerSignal()); QCOMPARE(calls, 0);
    }

    void audioBackendSendsTheExpandedMacroThroughTheHook()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        QString sent;
        int sentWpm = 0;
        int aborts = 0;
        CwController::Hooks h;
        h.myCall = [] { return QStringLiteral("iu8lmc"); };
        h.hisCall = [] { return QStringLiteral("dl1abc"); };
        h.canTransmit = [] { return true; };
        h.sendAudio = [&](const QString& t, int w) { sent = t; sentWpm = w; return true; };
        h.abortAudio = [&] { ++aborts; };
        c.setHooks(h);
        c.start(&s);
        QVERIFY(c.canSend());
        c.setWpm(25);

        c.sendMacro(0, {});
        QCOMPARE(sent, QString("CQ TEST IU8LMC IU8LMC TEST"));
        QCOMPARE(sentWpm, 25);
        QVERIFY(c.sending());
        QCOMPARE(c.activeMacroIndex(), 0);

        c.sendMacro(1, {});                                    // {CALL} dal gancio
        QCOMPARE(sent, QString("DL1ABC"));
        c.sendMacro(2, {{"nr", "042"}});                       // {NR} dalla finestra
        QCOMPARE(sent, QString("DL1ABC 5NN 042"));

        c.stop();
        QCOMPARE(aborts, 1);
        QVERIFY(!c.sending());
        QCOMPARE(c.activeMacroIndex(), -1);
    }

    void audioSendingEndsByItself()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        CwController::Hooks h;
        h.canTransmit = [] { return true; };
        h.sendAudio = [](const QString&, int) { return true; };
        c.setHooks(h);
        c.start(&s);
        c.setWpm(60);
        c.sendText("E", {});
        QVERIFY(c.sending());
        QTRY_VERIFY_WITH_TIMEOUT(!c.sending(), 4000);
    }

    void refusesToSendWhenTheRadioCannotTransmit()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        int calls = 0;
        CwController::Hooks h;
        h.canTransmit = [] { return false; };
        h.sendAudio = [&](const QString&, int) { ++calls; return true; };
        c.setHooks(h);
        c.start(&s);
        QVERIFY(!c.canSend());
        QSignalSpy msg(&c, &CwController::message);
        c.sendMacro(0, {});
        QCOMPARE(calls, 0);
        QCOMPARE(msg.size(), 1);
        QVERIFY(!c.sending());
    }

    void aFailingHookLeavesNothingArmed()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        CwController::Hooks h;
        h.canTransmit = [] { return true; };
        h.sendAudio = [](const QString&, int) { return false; };
        c.setHooks(h);
        c.start(&s);
        QSignalSpy msg(&c, &CwController::message);
        c.sendText("TEST", {});
        QVERIFY(!c.sending());
        QCOMPARE(msg.size(), 1);
    }

    void serialAndWinKeyerNeedTheirPort()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        c.start(&s);
        c.setTxBackend("serial");
        QCOMPARE(c.txBackend(), QString("serial"));
        QVERIFY(!c.canSend());
        c.setTxBackend("winkeyer");
        QVERIFY(!c.canSend());
        c.setTxBackend("qualcosa");                            // sconosciuto: torna all'audio
        QCOMPARE(c.txBackend(), QString("audio"));
    }

    void aRemoteRadioAlwaysUsesTheAudioPath()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        bool remote = false;
        QString sent;
        CwController::Hooks h;
        h.canTransmit = [] { return true; };
        h.remoteRadio = [&] { return remote; };
        h.sendAudio = [&](const QString& t, int) { sent = t; return true; };
        c.setHooks(h);
        c.start(&s);
        c.setTxBackend("serial");                              // un keyer su questo computer...
        QVERIFY(!c.canSend());                                 // ...senza porta aperta: non puo'
        remote = true;                                         // ma con la radio lontana conta l'audio
        QCOMPARE(c.effectiveBackend(), QString("audio"));
        QVERIFY(c.remoteRadio());
        QVERIFY(c.canSend());
        c.sendText("CQ", {});
        QCOMPARE(sent, QString("CQ"));
        QCOMPARE(c.txBackend(), QString("serial"));            // la scelta dell'operatore non si perde
    }

    // ── CW a tasto verso la radio remota ───────────────────────────────────

    void remoteKeyIsPreferredOnARemoteRadio()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        KeyRig rig;
        c.setHooks(rig.hooks());
        c.start(&s);
        QCOMPARE(c.effectiveBackend(), QString("remotekey"));
        QVERIFY(c.canSend());
        c.setRemoteKey(false);                                  // l'operatore preferisce il tono audio
        QCOMPARE(c.effectiveBackend(), QString("audio"));
        c.setRemoteKey(true);
        rig.supported = false;                                  // gateway che non sa farlo
        QCOMPARE(c.effectiveBackend(), QString("audio"));
        rig.supported = true;
        rig.remote = false;                                     // radio locale: la scelta e' del tipo di keyer
        QCOMPARE(c.effectiveBackend(), QString("audio"));
    }

    void remoteKeySendsThePttThenTheWholeTraceThenReleasesIt()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        KeyRig rig;
        c.setHooks(rig.hooks());
        c.start(&s);
        c.setWpm(60);
        c.setToneHz(650);
        c.sendText("CQ CQ", {});
        QVERIFY(c.sending());
        QTRY_VERIFY_WITH_TIMEOUT(!c.sending(), 8000);
        QCOMPARE(rig.audioCalls, 0);                            // il tono audio non e' stato toccato
        QCOMPARE(rig.order.first(), QString("ptt1"));           // il PTT prima del primo evento
        QCOMPARE(rig.order.last(), QString("ptt0"));            // e abbassato alla fine
        QCOMPARE(rig.ptt, (QVector<bool>{true, false}));
        QCOMPARE(rig.tones.first(), 650);
        const QVector<KeyEvent> sent = rig.all();
        const QVector<KeyEvent> wanted = timelineFor("CQ CQ", 60);
        QCOMPARE(sent.size(), wanted.size());                   // tutta la traccia, uguale, nell'ordine
        for (int i = 0; i < wanted.size(); ++i) {
            QCOMPARE(sent[i].deltaMs, wanted[i].deltaMs);
            QCOMPARE(sent[i].down, wanted[i].down);
        }
        QVERIFY(!sent.last().down);                             // finisce a tasto su
    }

    void remoteKeyDeliversALongMessageInPiecesAheadOfPlayback()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        KeyRig rig;
        c.setHooks(rig.hooks());
        c.start(&s);
        c.setWpm(60);
        c.sendText("CQ CQ CQ DE IU8LMC IU8LMC K", {});
        QTRY_VERIFY_WITH_TIMEOUT(rig.pieces.size() >= 2, 3000);
        for (const auto& piece : rig.pieces) {
            QVERIFY(piece.first().down);
            QVERIFY(!piece.last().down);                        // mai un pezzo a tasto giu'
        }
        QTRY_VERIFY_WITH_TIMEOUT(!c.sending(), 15000);
        QCOMPARE(rig.all().size(), timelineFor("CQ CQ CQ DE IU8LMC IU8LMC K", 60).size());
    }

    void stopReleasesThePttAndTheKeyAtOnce()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        KeyRig rig;
        c.setHooks(rig.hooks());
        c.start(&s);
        c.setWpm(5);                                            // lento: il messaggio e' ancora in corso
        c.sendText("CQ CQ CQ DE IU8LMC", {});
        QTRY_VERIFY_WITH_TIMEOUT(!rig.pieces.isEmpty(), 2000);
        const int before = rig.pieces.size();
        c.stop();
        QVERIFY(!c.sending());
        QCOMPARE(rig.ptt.last(), false);                        // PTT giu' subito
        QVERIFY(rig.pieces.size() > before);                    // e un "su" mandato per spegnere la nota
        QCOMPARE(rig.pieces.last().size(), 1);
        QVERIFY(!rig.pieces.last().first().down);
        const int after = rig.pieces.size();
        QTest::qWait(1500);
        QCOMPARE(rig.pieces.size(), after);                     // e non parte nient'altro
    }

    void aFailedSendDropsThePttAndTellsWhy()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        KeyRig rig;
        rig.failSend = true;
        c.setHooks(rig.hooks());
        c.start(&s);
        QSignalSpy msg(&c, &CwController::message);
        c.sendText("TEST", {});
        QTRY_VERIFY_WITH_TIMEOUT(!c.sending(), 3000);
        QCOMPARE(rig.ptt, (QVector<bool>{true, false}));
        QCOMPARE(msg.size(), 1);
    }

    void toneIsClamped()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        c.start(&s);
        c.setToneHz(50);
        QCOMPARE(c.toneHz(), 400);
        c.setToneHz(5000);
        QCOMPARE(c.toneHz(), 1000);
    }

    void settingsPersist()
    {
        {
            QSettings s(iniPath(), QSettings::IniFormat);
            CwController c;
            c.start(&s);
            c.setWpm(31);
            c.setKeyerLine("RTS");
            c.setMacro(0, "F1 CQ", "CQ DX DE {MYCALL}");
            c.setDecoderToneLock(650);
            c.setDecoderSpeedLock(22);
            c.setDecoderOn(false);
            s.sync();
        }
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        c.start(&s);
        QCOMPARE(c.wpm(), 31);
        QCOMPARE(c.keyerLine(), QString("RTS"));
        QCOMPARE(c.macros().first().toMap().value("text").toString(), QString("CQ DX DE {MYCALL}"));
        QCOMPARE(c.decoderToneLock(), 650);
        QCOMPARE(c.decoderSpeedLock(), 22);
        QVERIFY(!c.decoderOn());
    }

    void macroListKeepsItsLimits()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        c.start(&s);
        QCOMPARE(c.macros().size(), 12);
        for (int i = 0; i < 40; ++i) c.addMacro();
        QCOMPARE(c.macros().size(), kMaxMacros);
        while (c.macros().size() > 1) c.removeMacro(0);
        c.removeMacro(0);                                      // l'ultima non si toglie
        QCOMPARE(c.macros().size(), 1);
        c.resetMacros();
        QCOMPARE(c.macros().size(), 12);
        c.setMacro(99, "x", "y");                              // fuori lista: ignorato
        QCOMPARE(c.macros().size(), 12);
    }

    void limitsOfTheLocks()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        c.start(&s);
        c.setDecoderToneLock(5);
        QCOMPARE(c.decoderToneLock(), 100);
        c.setDecoderToneLock(99999);
        QCOMPARE(c.decoderToneLock(), 3000);
        c.setDecoderToneLock(0);
        QCOMPARE(c.decoderToneLock(), 0);
        c.setDecoderSpeedLock(2);
        QCOMPARE(c.decoderSpeedLock(), 5);
        c.setDecoderSpeedLock(500);
        QCOMPARE(c.decoderSpeedLock(), 60);
        c.setWpm(1);
        QCOMPARE(c.wpm(), 5);
        c.setWpm(1000);
        QCOMPARE(c.wpm(), 60);
    }

    void decoderReadsTheRadioAudio()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        c.start(&s);
        const QVector<short> audio = morseAudio("CQ CQ DE IU8LMC", 20, 700, 12000);
        const int chunk = 240;                                 // 20 ms a 12 kHz, come arrivano dalla radio
        for (int i = 0; i < audio.size(); i += chunk)
            c.feedRxAudio(audio.mid(i, chunk), 12000);
        QVERIFY2(c.decoderText().contains("IU8LMC"), qPrintable(c.decoderText()));
        QVERIFY(c.decoderWpm() >= 15 && c.decoderWpm() <= 25);
        QVERIFY(c.decoderTone() > 600 && c.decoderTone() < 800);
    }

    void decoderOffReadsNothing()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        c.start(&s);
        c.setDecoderOn(false);
        const QVector<short> audio = morseAudio("CQ DE IU8LMC", 20, 700, 12000);
        for (int i = 0; i < audio.size(); i += 240)
            c.feedRxAudio(audio.mid(i, 240), 12000);
        QVERIFY(c.decoderText().isEmpty());
    }

    void clearDecoderStartsOver()
    {
        QSettings s(iniPath(), QSettings::IniFormat);
        CwController c;
        c.start(&s);
        const QVector<short> audio = morseAudio("TEST", 20, 700, 12000);
        for (int i = 0; i < audio.size(); i += 240)
            c.feedRxAudio(audio.mid(i, 240), 12000);
        QVERIFY(!c.decoderText().isEmpty());
        c.clearDecoder();
        QVERIFY(c.decoderText().isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestCwController)
#include "test_cw_controller.moc"
