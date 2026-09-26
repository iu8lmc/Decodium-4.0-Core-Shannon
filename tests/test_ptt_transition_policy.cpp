#include <QtTest>

#include "src/bridge/PttTransitionPolicy.h"

class TestPttTransitionPolicy final : public QObject
{
    Q_OBJECT

private slots:
    void classifiesFeedbackAndNonQueryableMethods()
    {
        using decodium::tx::PttConfirmationMode;
        QVERIFY(decodium::tx::pttConfirmationMode(QStringLiteral("CAT"), true)
                == PttConfirmationMode::RigFeedback);
        QVERIFY(decodium::tx::pttConfirmationMode(QStringLiteral("DTR"), true)
                == PttConfirmationMode::CommandDispatch);
        QVERIFY(decodium::tx::pttConfirmationMode(QStringLiteral("RTS"), true)
                == PttConfirmationMode::CommandDispatch);
        QVERIFY(decodium::tx::pttConfirmationMode(QStringLiteral("VOX"), false)
                == PttConfirmationMode::AudioActivity);
        QVERIFY(decodium::tx::pttConfirmationMode(QStringLiteral("CAT"), false)
                == PttConfirmationMode::RigFeedback);
    }

    void bridgeManagedLegacyStateIsNotOnAirProof()
    {
        QVERIFY(decodium::tx::legacyReportedTxIsAuthoritative(false));
        QVERIFY(!decodium::tx::legacyReportedTxIsAuthoritative(true));
        QCOMPARE(decodium::tx::pttFeedbackTimeoutMs(), 650);
    }
};

QTEST_APPLESS_MAIN(TestPttTransitionPolicy)
#include "test_ptt_transition_policy.moc"
