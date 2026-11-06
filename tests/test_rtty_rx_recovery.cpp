#include <QtTest>
#include "src/bridge/RttyRxRecovery.h"

using Recovery = decodium::audio::RttyRxRecovery;
class TestRttyRxRecovery : public QObject {
    Q_OBJECT
private slots:
    void monitorHandoffAndFreshSilentPcm() {
        Recovery::State state;
        state.monitoring = false;
        state.details = "rms=0 peak=0";
        QStringList logs;
        int rearms = 0, attempts = 0;
        Recovery recovery({[&] { return state; }, [&] {
            ++rearms;
            QTimer::singleShot(5, this, [&] { state.monitoring = true; });
        }, [&](bool retry) {
            QVERIFY(!retry);
            ++attempts;
            QTimer::singleShot(5, this, [&] { ++state.pcmStamp; });
        }, [&](const QString& s) { logs << s; }});
        recovery.start({1, 10, 20, 200});
        // Period-timer changes are intentionally absent from the controller's
        // interface: monitor hand-off may rearm UTC without invalidating this.
        QTRY_VERIFY(logs.join('\n').contains("completed, fresh PCM"));
        QCOMPARE(rearms, 1);
        QCOMPARE(attempts, 1);
        QVERIFY(logs.join('\n').contains("deferred reason=monitor hand-off"));
        QVERIFY(logs.join('\n').contains("pcm=1 attempt=1 rms=0 peak=0"));
    }
    void noPcmHasBoundedRetry() {
        QStringList logs;
        QList<bool> attempts;
        Recovery recovery({[] { return Recovery::State{}; }, [] {},
            [&](bool retry) { attempts << retry; }, [&](const QString& s) { logs << s; }});
        recovery.start({1, 5, 10, 100});
        QTRY_VERIFY(logs.join('\n').contains("exhausted, no fresh PCM"));
        QCOMPARE(attempts, QList<bool>({false, true}));
        QCOMPARE(logs.filter("RTTY exit RX check: pcm=0").size(), 2);
    }
    void busyDefersThenRecovers() {
        Recovery::State state;
        state.busy = true;
        QStringList logs;
        int attempts = 0;
        Recovery recovery({[&] { return state; }, [] {}, [&](bool) {
            QVERIFY(!state.busy);
            ++attempts;
            ++state.pcmStamp;
        }, [&](const QString& s) { logs << s; }});
        recovery.start({1, 5, 10, 200});
        QTRY_VERIFY(logs.join('\n').contains("deferred reason=TX/Tune"));
        QCOMPARE(attempts, 0);
        state.busy = false;
        QTRY_VERIFY(logs.join('\n').contains("completed, fresh PCM"));
    }
    void stoppedMonitorNeverRearmed() {
        Recovery::State state;
        state.requested = false;
        QStringList logs;
        Recovery recovery({[&] { return state; }, [] { QFAIL("must not rearm"); },
            [](bool) { QFAIL("must not recover"); }, [&](const QString& s) { logs << s; }});
        recovery.start({1, 5, 10, 100});
        QTRY_VERIFY(logs.join('\n').contains("cancelled reason=monitor stopped"));
    }
    void cancellationSupersedesPendingCallbacks() {
        int attempts = 0;
        QStringList logs;
        Recovery recovery({[] { return Recovery::State{}; }, [] {},
            [&](bool) { ++attempts; }, [&](const QString& s) { logs << s; }});
        recovery.start({20, 5, 10, 100});
        recovery.cancel("audio input changed");
        recovery.start({1, 5, 10, 100});
        QTRY_VERIFY(logs.join('\n').contains("exhausted"));
        QTest::qWait(30);
        QCOMPARE(attempts, 2);
        QVERIFY(logs.join('\n').contains("cancelled reason=audio input changed"));
    }
    void handoffTimeoutIsReported() {
        Recovery::State state;
        state.monitoring = false;
        QStringList logs;
        int rearms = 0;
        Recovery recovery({[&] { return state; }, [&] { ++rearms; },
            [](bool) { QFAIL("must wait for monitor"); }, [&](const QString& s) { logs << s; }});
        recovery.start({1, 5, 10, 20});
        QTRY_VERIFY(logs.join('\n').contains("RTTY exit RX check: unavailable"));
        QCOMPARE(rearms, 1);
        QVERIFY(logs.join('\n').contains("deadline exceeded"));
    }
    void remoteSourceIsCancelled() {
        Recovery::State state;
        state.cancelReason = "RX source is not a local sound card";
        QStringList logs;
        Recovery recovery({[&] { return state; }, [] { QFAIL("must not rearm"); },
            [](bool) { QFAIL("must not recover"); }, [&](const QString& s) { logs << s; }});
        recovery.start({1, 5, 10, 100});
        QTRY_VERIFY(logs.join('\n').contains("cancelled reason=RX source"));
    }
};
QTEST_GUILESS_MAIN(TestRttyRxRecovery)
#include "test_rtty_rx_recovery.moc"
