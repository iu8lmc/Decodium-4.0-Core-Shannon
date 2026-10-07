#include "CwController.h"

#include <QFile>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <cmath>

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

}  // namespace

class TestCwController : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString iniPath() const { return m_dir.filePath("cw.ini"); }

private slots:
    void init() { QFile::remove(iniPath()); }

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
