#include <QtTest>
#include <QFile>
#include <QBuffer>
#include <QSettings>
#include <QTemporaryDir>
#include "SsbController.h"
#include "SsbRxAgc.h"
#include <limits>
using namespace decodium::ssb;
namespace decodium::ssb {
// Feed the real capture/DSP/PTT path without opening a microphone or a radio.
struct SsbControllerTestAccess {
    static void attach(SsbController& c, QIODevice& input, const QAudioFormat& format) {
        c.input = &input;
        c.format = format;
        c.active = true;
        c.lead = 0;
        c.clock.start();
        c.lastAudio.start();
    }
    static void capture(SsbController& c) { c.capture(); }
    static qint64 audioAge(const SsbController& c) { return c.lastAudio.elapsed(); }
    static QByteArray remainder(const SsbController& c) { return c.pending; }
    static void recover(SsbController& c) {
        // No device selected: a failed reopen must end the simulated session.
        c.deviceId.clear();
        c.recoverMicrophone(QStringLiteral("Simulated disconnected microphone"));
    }
};
}
class TestSsb: public QObject {
    Q_OBJECT
    QTemporaryDir settingsDir;
private slots:
    void initTestCase() {
        QVERIFY(settingsDir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
    }
    void delayedMicrophoneBlockDoesNotReleasePtt_data() {
        QTest::addColumn<int>("sampleFormat");
        QTest::addColumn<int>("channels");
        QTest::addColumn<int>("delayMs");
        for (auto sf : {QAudioFormat::Int16, QAudioFormat::Float})
            for (int ch : {1, 2})
                for (int ms : {260, 500, 2000})
                    QTest::newRow(qPrintable(QString("format-%1-ch-%2-delay-%3").arg(int(sf)).arg(ch).arg(ms)))
                        << int(sf) << ch << ms;
    }
    void delayedMicrophoneBlockDoesNotReleasePtt() {
        QFETCH(int, sampleFormat);
        QFETCH(int, channels);
        QFETCH(int, delayMs);
        QAudioFormat format;
        format.setSampleRate(48000);
        format.setSampleFormat(QAudioFormat::SampleFormat(sampleFormat));
        format.setChannelCount(channels);
        QBuffer input;
        input.setData(QByteArray(format.bytesForDuration(delayMs * 1000), '\0'));
        QVERIFY(input.open(QIODevice::ReadOnly));
        SsbController c;
        int ended = 0, samplesSent = 0;
        SsbController::Hooks h;
        h.valid = [] { return true; };
        h.end = [&] { ++ended; };
        h.send = [&](const QVector<short>& block) {
            QCOMPARE(block.size(), 480);
            samplesSent += block.size();
            for (short sample : block) QCOMPARE(sample, short(0));
        };
        c.setHooks(h);
        SsbControllerTestAccess::attach(c, input, format);
        SsbControllerTestAccess::capture(c);
        QVERIFY2(c.transmitting(), qPrintable(c.status()));
        QCOMPARE(ended, 0);
        // Catch up to live audio, never replay a half-second-old voice burst.
        QCOMPARE(samplesSent, 480);
        // Subsequent healthy 40 ms blocks keep flowing, followed by release.
        for (int i = 0; i < 10; ++i) {
            input.close();
            input.setData(QByteArray(format.bytesForDuration(40000), '\0'));
            QVERIFY(input.open(QIODevice::ReadOnly));
            SsbControllerTestAccess::capture(c);
            QVERIFY(c.transmitting());
        }
        QCOMPARE(samplesSent, 11 * 480);
        c.stop();
        QCOMPARE(ended, 1);
        QVERIFY(!c.transmitting());
        c.stop();
        QCOMPARE(ended, 1);
    }
    void backlogDropsOldPacketAndPreservesFrameAlignment() {
        QAudioFormat format;
        format.setSampleRate(48000);
        format.setSampleFormat(QAudioFormat::Int16);
        format.setChannelCount(2);
        QBuffer input;
        input.setData(QByteArray(format.bytesForDuration(20000), '\x20'));
        QVERIFY(input.open(QIODevice::ReadOnly));
        SsbController c;
        QVector<short> sent;
        SsbController::Hooks h;
        h.valid = [] { return true; };
        h.send = [&](const QVector<short>& block) { sent += block; };
        c.setHooks(h);
        SsbControllerTestAccess::attach(c, input, format);
        SsbControllerTestAccess::capture(c); // a half-filled TX packet and filter history
        QVERIFY(sent.isEmpty());
        input.close();
        input.setData(QByteArray(format.bytesForDuration(460000), '\x20')
            + QByteArray(format.bytesForDuration(40000), '\0') + QByteArray(1, '\x7f'));
        QVERIFY(input.open(QIODevice::ReadOnly));
        SsbControllerTestAccess::capture(c);
        QVERIFY(c.transmitting());
        QCOMPARE(sent, QVector<short>(480, 0)); // no old speech/filter history transmitted
        QCOMPARE(SsbControllerTestAccess::remainder(c), QByteArray(1, '\x7f'));
        c.stop();
    }
    void emptyReadDoesNotHideMicrophoneStall() {
        QAudioFormat format;
        format.setSampleRate(48000);
        format.setSampleFormat(QAudioFormat::Int16);
        format.setChannelCount(1);
        QBuffer input;
        QVERIFY(input.open(QIODevice::ReadOnly));
        SsbController c;
        SsbController::Hooks h;
        h.valid = [] { return true; };
        c.setHooks(h);
        SsbControllerTestAccess::attach(c, input, format);
        QTest::qWait(30);
        const auto age = SsbControllerTestAccess::audioAge(c);
        SsbControllerTestAccess::capture(c);
        QVERIFY(SsbControllerTestAccess::audioAge(c) >= age);
        c.stop();
    }
    void lostRadioReleasesPttBeforeSendingBacklog() {
        QAudioFormat format;
        format.setSampleRate(48000);
        format.setSampleFormat(QAudioFormat::Int16);
        format.setChannelCount(1);
        QBuffer input;
        input.setData(QByteArray(format.bytesForDuration(500000), '\0'));
        QVERIFY(input.open(QIODevice::ReadOnly));
        SsbController c;
        int ended = 0, sent = 0;
        SsbController::Hooks h;
        h.valid = [] { return false; };
        h.end = [&] { ++ended; };
        h.send = [&](const QVector<short>&) { ++sent; };
        c.setHooks(h);
        SsbControllerTestAccess::attach(c, input, format);
        SsbControllerTestAccess::capture(c);
        QVERIFY(!c.transmitting());
        QCOMPARE(ended, 1);
        QCOMPARE(sent, 0);
    }
    void failedMicrophoneRecoveryStillReleasesPtt() {
        SsbController c;
        QBuffer input;
        QAudioFormat format;
        int ended = 0;
        SsbController::Hooks h;
        h.end = [&] { ++ended; };
        c.setHooks(h);
        SsbControllerTestAccess::attach(c, input, format);
        SsbControllerTestAccess::recover(c);
        QVERIFY(c.transmitting());
        QTRY_VERIFY_WITH_TIMEOUT(!c.transmitting(), 1000);
        QCOMPARE(ended, 1);
        QVERIFY(c.status().contains(QStringLiteral("Cannot reopen microphone")));
    }
    void releaseDuringRecoveryCannotRestartPtt() {
        SsbController c;
        QBuffer input;
        QAudioFormat format;
        int ended = 0;
        SsbController::Hooks h;
        h.end = [&] { ++ended; };
        c.setHooks(h);
        SsbControllerTestAccess::attach(c, input, format);
        SsbControllerTestAccess::recover(c);
        c.stop();
        QTest::qWait(350);
        QCOMPARE(ended, 1);
        QVERIFY(!c.transmitting());
        QCOMPARE(c.status(), QStringLiteral("Receiving"));
    }
    void limiterAndFinite(){SpeechDsp dsp;dsp.gainDb=30;for(int i=0;i<48000;++i){const float y=dsp.process(std::sin(i*.4)*100);QVERIFY(std::isfinite(y));QVERIFY(std::abs(y)<=.951);}QVERIFY(std::isfinite(dsp.process(std::numeric_limits<float>::quiet_NaN())));}
    void rejectsDc(){SpeechDsp dsp;dsp.automatic=false;float y=0;for(int i=0;i<48000;++i)y=dsp.process(.5);QVERIFY(std::abs(y)<.00001);}
    void voiceBand(){auto rms=[](double hz){SpeechDsp dsp;dsp.automatic=false;double sum=0;for(int i=0;i<48000;++i){double y=dsp.process(.2*std::sin(2*3.141592653589793*hz*i/48000));if(i>24000)sum+=y*y;}return std::sqrt(sum/24000);};QVERIFY(rms(1000)>rms(8000)*20);QVERIFY(rms(1000)>rms(30)*3);}
    void noImplicitMicrophone(){SsbController c;bool keyed=false;SsbController::Hooks h;h.begin=[&]{keyed=true;return true;};c.setHooks(h);c.setMicrophone("not-an-audio-device");QVERIFY(!c.start());QVERIFY(!keyed);QVERIFY(!c.transmitting());}
    void stopIsSafeBeforeAndAfterFailedCapture(){SsbController c;c.stop();c.setMicrophone("not-an-audio-device");QVERIFY(!c.start());c.stop();QVERIFY(!c.transmitting());}
    void transientDeviceRefreshUsesBoundedRecovery(){
        QFile f(QStringLiteral(DECODIUM_SOURCE_DIR "/src/ssb/SsbController.cpp"));
        QVERIFY(f.open(QIODevice::ReadOnly|QIODevice::Text));
        const QString source=QString::fromUtf8(f.readAll());
        QVERIFY(source.contains(QStringLiteral("void SsbController::handleAudioInputsChanged()")));
        QVERIFY(source.contains(QStringLiteral("if(!selected.isNull() && source && source->state()!=QAudio::StoppedState)return;")));
        QVERIFY(source.contains(QStringLiteral("if(!active || microphoneRecoveryPending)return;")));
        QVERIFY(source.contains(QStringLiteral("microphoneRestartAttempts++ >= 1")));
        QVERIFY(source.contains(QStringLiteral("!microphoneRecoveryPending && lastAudio.elapsed()>1500")));
        QVERIFY(source.contains(QStringLiteral("Microphone did not remain active after startup")));
        QVERIFY(!source.contains(QStringLiteral("if(active) fail(tr(\"Audio devices changed; PTT released\"))")));
    }
    void rxAgcLevelsSignals(){
        RxAgc agc;agc.configure(12000);agc.setMode(RxAgc::Medium);agc.setThresholdDb(-90);
        auto settle=[&](double amp){float peak=0;for(int b=0;b<60;++b){float buf[1200];for(int i=0;i<1200;++i)buf[i]=float(amp*std::sin(2*3.14159265*700*i/12000.));agc.process(buf,1200);if(b>=50)for(float v:buf)peak=std::max(peak,std::abs(v));}return peak;};
        const float weak=settle(.002),strong=settle(.5);
        QVERIFY(weak>.15f);                 // il segnale debole viene portato su
        QVERIFY(strong<.6f && strong>.15f); // quello forte non sfonda
        QVERIFY(std::abs(20*std::log10(weak/strong))<12.f); // dinamica compressa
    }
    void rxAgcOffIsManualGain(){
        RxAgc agc;agc.configure(12000);agc.setMode(RxAgc::Off);agc.setManualGainDb(6);
        float buf[4]={.1f,-.1f,.2f,0.f};agc.process(buf,4);
        QVERIFY(std::abs(buf[0]-.1f*1.9953f)<.001f);
    }
    void rxAgcAutoDoesNotPumpNoise(){
        RxAgc agc;agc.configure(12000);agc.setMode(RxAgc::Auto);
        unsigned seed=12345;float maxOut=0;
        for(int b=0;b<200;++b){float buf[1200];for(int i=0;i<1200;++i){seed=seed*1664525u+1013904223u;buf[i]=((seed>>9)/float(1<<23)-.5f)*.01f;}agc.process(buf,1200);if(b>150)for(float v:buf)maxOut=std::max(maxOut,std::abs(v));}
        QVERIFY(std::isfinite(maxOut));
        QVERIFY(maxOut<.9f);                // il rumore non diventa un muro
    }
};
QTEST_GUILESS_MAIN(TestSsb)
#include "test_ssb.moc"
