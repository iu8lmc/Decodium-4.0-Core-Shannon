#include <QtTest>
#include "src/bridge/RttyRigModeState.h"

using State = decodium::radio::RttyRigModeState;
class TestRttyRigModeState : public QObject {
    Q_OBJECT
private slots:
    void restoresExactPreRttyModeWithCatNone() {
        State s;
        s.enter("hamlib|QMX", "DATA-L");
        s.overrideRequested("hamlib|QMX", "DATA-U");
        QVERIFY(s.restoreTarget("hamlib|QMX", {}, 100).isEmpty()); // still in RTTY
        s.leave(100);
        QCOMPARE(s.restoreTarget("hamlib|QMX", {}, 150), QString("DATA-L"));
        // Do not infer physical sideband from a Hamlib label or force USB.
        QCOMPARE(s.restoreTarget("hamlib|QMX", {}, 1300), QString("DATA-L"));
    }
    void explicitNormalCatPolicyWins() {
        State s;
        s.enter("native|radio", "USB");
        s.overrideRequested("native|radio", "RTTY-U");
        s.leave(100);
        QVERIFY(s.restoreTarget("native|radio", "DATA-U", 200).isEmpty());
    }
    void noOverrideLeavesRadioAlone() {
        State s;
        s.enter("hamlib|QMX", "DATA-L");
        s.leave(100); // RttyRigMode=None: no command sent
        QVERIFY(s.restoreTarget("hamlib|QMX", {}, 200).isEmpty());
        s.overrideRequested("hamlib|QMX", "DATA-L");
        s.leave(300);
        QVERIFY(s.restoreTarget("hamlib|QMX", {}, 400).isEmpty());
    }
    void newRigAndDisconnectionCancelRestore() {
        State s;
        s.enter("hamlib|QMX", "DATA-L");
        s.overrideRequested("hamlib|QMX", "DATA-U");
        s.leave(100);
        QVERIFY(s.restoreTarget("hamlib|other", {}, 200).isEmpty());
        QVERIFY(s.restoreTarget({}, {}, 200).isEmpty());
        s.clear(); // disconnect, including reconnect to the same rig
        QVERIFY(s.restoreTarget("hamlib|QMX", {}, 200).isEmpty());
    }
    void unknownModeCannotBeRestored() {
        for (const QString& mode : {QString(), QString("Unknown"), QString("FT8")}) {
            State s;
            s.enter("hamlib|QMX", mode);
            s.overrideRequested("hamlib|QMX", "DATA-U");
            s.leave(100);
            QVERIFY(s.restoreTarget("hamlib|QMX", {}, 200).isEmpty());
        }
    }
    void expiryAndNewModeDropOldState() {
        State s;
        s.enter("hamlib|QMX", "DATA-L");
        s.overrideRequested("hamlib|QMX", "DATA-U");
        s.leave(100);
        QVERIFY(s.restoreTarget("hamlib|QMX", {}, 6100).isEmpty());
        s.enter("hamlib|QMX", "USB");
        s.overrideRequested("hamlib|QMX", "RTTY-U");
        s.leave(6200);
        QCOMPARE(s.restoreTarget("hamlib|QMX", {}, 6300), QString("USB"));
    }
};
QTEST_GUILESS_MAIN(TestRttyRigModeState)
#include "test_rtty_rig_mode_state.moc"
