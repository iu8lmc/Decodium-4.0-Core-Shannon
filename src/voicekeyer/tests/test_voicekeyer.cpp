#include "AssistantChat.h"
#include "PackManager.h"
#include "PiperTts.h"
#include "SpeechToText.h"
#include "VoiceKeyer.h"

#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

#include <cmath>

using namespace decodium::voicekeyer;

class TestVoiceKeyer : public QObject {
    Q_OBJECT
private slots:
    void wavRoundTrip()
    {
        QTemporaryDir dir;
        QVector<short> in(12000);
        for (int i = 0; i < in.size(); ++i) in[i] = short(std::sin(i * 0.1) * 12000);
        const QString path = dir.filePath(QStringLiteral("a.wav"));
        QString error;
        QVERIFY(writeWav(path, in, kRadioRate, &error));
        const auto back = readWav(path, &error);
        QVERIFY(back.has_value());
        QCOMPARE(back->size(), in.size());
        QCOMPARE(back->at(5000), in.at(5000));
    }

    void resamplesFortyEightToTwelve()
    {
        QVector<short> in(48000, 1000);
        const QVector<short> out = resampleLinear(in, kCaptureRate, kRadioRate);
        QCOMPARE(out.size(), 12000);
        QCOMPARE(out.at(100), short(1000));
    }

    void rejectsNonWav()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("bad.wav"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not a wave file at all, really not");
        f.close();
        QString error;
        QVERIFY(!readWav(path, &error).has_value());
        QVERIFY(!error.isEmpty());
    }

    void sendsPacedPacketsThenEnds()
    {
        QTemporaryDir dir;
        PackManager packs(dir.path());
        VoiceKeyer keyer(dir.path(), &packs);
        QVector<short> msg(kRadioRate / 2, 3000);
        QVERIFY(writeWav(QDir(dir.path()).filePath(QStringLiteral("slot1.wav")), msg, kRadioRate));
        int begun = 0, ended = 0, packets = 0;
        VoiceKeyer::Hooks h;
        h.begin = [&] { ++begun; return true; };
        h.end = [&] { ++ended; };
        h.valid = [] { return true; };
        h.send = [&](const QVector<short>& p) { QCOMPARE(p.size(), kPacket); ++packets; };
        h.leadMs = [] { return 0; };
        keyer.setHooks(h);
        keyer.play(0);
        QTRY_COMPARE_WITH_TIMEOUT(ended, 1, 5000);
        QCOMPARE(begun, 1);
        QVERIFY(packets >= msg.size() / kPacket);
        QVERIFY(!keyer.playing());
    }

    void refusesPlayWithoutRecording()
    {
        QTemporaryDir dir;
        PackManager packs(dir.path());
        VoiceKeyer keyer(dir.path(), &packs);
        int begun = 0;
        VoiceKeyer::Hooks h;
        h.begin = [&] { ++begun; return true; };
        keyer.setHooks(h);
        keyer.play(2);
        QCOMPARE(begun, 0);
        QVERIFY(!keyer.playing());
    }

    void packsNotInstalledInEmptyDir()
    {
        QTemporaryDir dir;
        PackManager packs(dir.path());
        QVERIFY(!packs.installed(Pack::Voice));
        QVERIFY(!packs.installed(Pack::Speech));
        QVERIFY(!packs.installed(Pack::Assistant));
        QVERIFY(!packs.busy());
    }

    void inactivePackIsNotActive()
    {
        // Senza file un pacchetto non e' mai attivo, qualunque sia la selezione dell'utente.
        // (Non si scrive QSettings nei test: non tocca il profilo dell'utente.)
        QTemporaryDir dir;
        PackManager packs(dir.path());
        QVERIFY(!packs.active(Pack::Speech));
        QVERIFY(!packs.active(Pack::Assistant));
    }

    void transcriptIsCleaned()
    {
        QCOMPARE(cleanTranscript(QStringLiteral("  [BLANK_AUDIO]  CQ  CQ de IU8LMC [MUSIC] ")),
                 QStringLiteral("CQ CQ de IU8LMC"));
        QVERIFY(cleanTranscript(QStringLiteral("[BLANK_AUDIO]")).isEmpty());
    }

    void chatPromptKeepsRecentTurns()
    {
        QVector<ChatTurn> history;
        for (int i = 0; i < 10; ++i) {
            history.append({QStringLiteral("user"), QStringLiteral("domanda %1").arg(i)});
            history.append({QStringLiteral("assistant"), QStringLiteral("risposta %1").arg(i)});
        }
        const QString p = buildChatPrompt(history, QStringLiteral("ultima"));
        QVERIFY(p.startsWith(QStringLiteral("<|im_start|>system")));
        QVERIFY(!p.contains(QStringLiteral("domanda 0")));       // le prime storie sono cadute
        QVERIFY(p.contains(QStringLiteral("domanda 9")));
        QVERIFY(p.endsWith(QStringLiteral("<|im_start|>assistant\n")));
    }

    void replyIsCleaned()
    {
        QCOMPARE(cleanReply(QStringLiteral("\x1b[33mCiao\x1b[0m, sono pronto.[end of text]\n")),
                 QStringLiteral("Ciao, sono pronto."));
    }

    void assistantAnswersWhenEngineIsPresent()
    {
        // Facoltativo: serve llama-completion e il modello Qwen gia' installati.
        const QString exe = qEnvironmentVariable("DECODIUM_TEST_LLAMA_EXE");
        const QString model = qEnvironmentVariable("DECODIUM_TEST_LLAMA_MODEL");
        if (exe.isEmpty() || model.isEmpty()) QSKIP("set DECODIUM_TEST_LLAMA_EXE and DECODIUM_TEST_LLAMA_MODEL");
        QProcess p;
        p.setWorkingDirectory(QFileInfo(exe).absolutePath());
        p.start(exe, {QStringLiteral("-m"), model, QStringLiteral("-p"),
                      buildChatPrompt({}, QStringLiteral("Ciao, chi sei?")),
                      QStringLiteral("-n"), QStringLiteral("40"), QStringLiteral("-no-cnv"),
                      QStringLiteral("--no-display-prompt"), QStringLiteral("--simple-io")});
        p.closeWriteChannel();
        QVERIFY2(p.waitForStarted(10000), qPrintable(p.errorString()));
        QVERIFY(p.waitForFinished(240000));
        QCOMPARE(p.exitCode(), 0);
        const QByteArray out = p.readAllStandardOutput();
        QVERIFY2(!cleanReply(QString::fromUtf8(out)).isEmpty(),
                 qPrintable(QStringLiteral("exit=%1 status=%2 bytes=%3 err=%4").arg(p.exitCode()).arg(int(p.exitStatus()))
                                .arg(out.size()).arg(QString::fromLocal8Bit(p.readAllStandardError().right(300)))));
    }

    void piperSynthesisWhenEngineIsPresent()
    {
        const QString exe = qEnvironmentVariable("DECODIUM_TEST_PIPER_EXE");
        const QString model = qEnvironmentVariable("DECODIUM_TEST_PIPER_MODEL");
        if (exe.isEmpty() || model.isEmpty()) QSKIP("set DECODIUM_TEST_PIPER_EXE and DECODIUM_TEST_PIPER_MODEL");
        QString error;
        const auto pcm = PiperTts::synthesize(exe, QFileInfo(exe).absolutePath(), model,
                                              QStringLiteral("Prova di sintesi vocale."), &error);
        QVERIFY2(pcm.has_value(), qPrintable(error));
        QVERIFY(pcm->size() > kRadioRate / 2);
    }

    void whisperRunsOnARecording()
    {
        const QString exe = qEnvironmentVariable("DECODIUM_TEST_WHISPER_EXE");
        const QString model = qEnvironmentVariable("DECODIUM_TEST_WHISPER_MODEL");
        if (exe.isEmpty() || model.isEmpty()) QSKIP("set DECODIUM_TEST_WHISPER_EXE and DECODIUM_TEST_WHISPER_MODEL");
        QTemporaryDir dir;
        QVector<short> tone(16000 * 2);
        for (int i = 0; i < tone.size(); ++i) tone[i] = short(std::sin(i * 0.2) * 3000);
        const QString wav = dir.filePath(QStringLiteral("tone.wav"));
        QVERIFY(writeWav(wav, tone, 16000));
        QProcess p;
        p.start(exe, {QStringLiteral("-m"), model, QStringLiteral("-f"), wav, QStringLiteral("-nt"), QStringLiteral("-np")});
        QVERIFY(p.waitForFinished(120000));
        QCOMPARE(p.exitCode(), 0);
        QVERIFY(cleanTranscript(QString::fromUtf8(p.readAllStandardOutput())).size() < 200);
    }

    void voicePackInstallsFromScratch()
    {
        // Lento (circa 150 MB): gira solo con DECODIUM_TEST_NET=1.
        if (qEnvironmentVariable("DECODIUM_TEST_NET") != QStringLiteral("1"))
            QSKIP("set DECODIUM_TEST_NET=1 to download the voice pack");
        QTemporaryDir dir;
        PackManager packs(dir.path());
        packs.install(static_cast<int>(Pack::Voice));
        QTRY_VERIFY_WITH_TIMEOUT(!packs.busy(), 900000);
        QVERIFY2(packs.installed(Pack::Voice), qPrintable(packs.status()));
        packs.remove(static_cast<int>(Pack::Voice));
        QVERIFY(!packs.installed(Pack::Voice));
    }
};

QTEST_GUILESS_MAIN(TestVoiceKeyer)
#include "test_voicekeyer.moc"
