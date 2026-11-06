#include <QtTest>

#include <hamlib/rig.h>
#include <hamlib/riglist.h>

// This is deliberately a no-radio test.  It exercises the precise Hamlib
// shared library linked into a release build and validates the model metadata
// Decodium relies on for CAT, ATU and dashboard power controls.
class TestHamlibReleaseContract final : public QObject
{
    Q_OBJECT

private slots:
    void ic7300ModelCapabilitiesArePresent()
    {
        QCOMPARE(rig_load_all_backends(), RIG_OK);

        QCOMPARE(static_cast<rig_port_t>(rig_get_caps_int(RIG_MODEL_IC7300,
                                                           RIG_CAPS_PORT_TYPE)),
                 RIG_PORT_SERIAL);
        const char* const modelName = rig_get_caps_cptr(RIG_MODEL_IC7300,
                                                        RIG_CAPS_MODEL_NAME_CPTR);
        QVERIFY(modelName != nullptr);
        QVERIFY(QString::fromLatin1(modelName).contains(QStringLiteral("IC-7300"),
                                                        Qt::CaseInsensitive));

        RIG* const rig = rig_init(RIG_MODEL_IC7300);
        QVERIFY(rig != nullptr);
        QVERIFY(rig->caps != nullptr);
        QVERIFY(rig->caps->set_powerstat != nullptr);
        QVERIFY(rig_has_set_func(rig, RIG_FUNC_TUNER));
        QCOMPARE(rig_cleanup(rig), RIG_OK);
    }
};

QTEST_APPLESS_MAIN(TestHamlibReleaseContract)
#include "test_hamlib_release_contract.moc"
