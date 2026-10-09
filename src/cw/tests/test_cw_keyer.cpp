// Il manipolatore CW su porta seriale: la tavola del codice e i tempi.
//
// Manipolare davvero vuol dire una porta e una radio attaccata, e qui non ci
// sono; quello che si prova e' tutto il resto — che il codice sia quello giusto
// e che la durata di un messaggio torni con i conti che fa ogni operatore CW.
#include "CwKeyer.h"

#include <QSignalSpy>
#include <QTest>

using decodium::cw::CwKeyer;

class TestCwKeyer : public QObject {
    Q_OBJECT

private slots:
    void theCodeIsTheCode()
    {
        QCOMPARE(CwKeyer::morseOf('E'), QString("."));
        QCOMPARE(CwKeyer::morseOf('T'), QString("-"));
        QCOMPARE(CwKeyer::morseOf('K'), QString("-.-"));
        QCOMPARE(CwKeyer::morseOf('5'), QString("....."));
        QCOMPARE(CwKeyer::morseOf('/'), QString("-..-."));
        QCOMPARE(CwKeyer::morseOf('?'), QString("..--.."));
        // Le minuscole valgono come le maiuscole.
        QCOMPARE(CwKeyer::morseOf('k'), CwKeyer::morseOf('K'));
        // Quello che non si sa mandare non si manda: niente, non un errore.
        QCOMPARE(CwKeyer::morseOf(QChar(0x00E8)), QString());   // e accentata
        QCOMPARE(CwKeyer::morseOf('#'), QString());
    }

    // PARIS a 20 parole al minuto dura tre secondi: e' la definizione stessa
    // della velocita' in CW, e se questo conto torna tornano tutti i tempi.
    void parisIsTheYardstick()
    {
        // La parola PARIS piu' lo spazio che la segue: 50 punti tondi.
        const int dot = 1200 / 20;
        QCOMPARE(CwKeyer::millisFor(QStringLiteral("PARIS "), 20), 50 * dot);
        // Dieci volte in un minuto, a 10 wpm; venti a 20.
        QCOMPARE(CwKeyer::millisFor(QStringLiteral("PARIS "), 10) * 10, 60000);
        QCOMPARE(CwKeyer::millisFor(QStringLiteral("PARIS "), 20) * 20, 60000);
    }

    void aSingleLetterLastsWhatItShould()
    {
        // La E a 24 wpm e' un punto solo: 50 millisecondi.
        QCOMPARE(CwKeyer::millisFor(QStringLiteral("E"), 24), 50);
        // La O sono tre linee e due silenzi: 3*3 + 2 = 11 punti.
        QCOMPARE(CwKeyer::millisFor(QStringLiteral("O"), 24), 11 * 50);
        // Due lettere hanno tre punti in mezzo.
        QCOMPARE(CwKeyer::millisFor(QStringLiteral("EE"), 24), (1 + 3 + 1) * 50);
        // Quello che non si manda non dura niente.
        QCOMPARE(CwKeyer::millisFor(QStringLiteral("#"), 24), 0);
        QCOMPARE(CwKeyer::millisFor(QString(), 24), 0);
    }

    // Senza porta aperta non si manipola, e soprattutto non si pianta niente:
    // chi preme F1 con il manipolatore spento deve solo non vedere succedere
    // nulla.
    void withoutAPortNothingHappens()
    {
        CwKeyer keyer;
        QVERIFY(!keyer.isOpen());
        QVERIFY(!keyer.open(QString(), QStringLiteral("DTR")));
        keyer.send(QStringLiteral("CQ CQ DE IU8LMC"), 25);
        QVERIFY(!keyer.sending());
        keyer.stop();
        keyer.close();
    }

    // Una porta che non esiste si rifiuta di aprirsi e lo dice, invece di far
    // finta di niente.
    void aPortThatIsNotThereSaysSo()
    {
        CwKeyer keyer;
        QSignalSpy failures(&keyer, &CwKeyer::failed);
        QVERIFY(!keyer.open(QStringLiteral("COM99"), QStringLiteral("DTR")));
        QVERIFY(!keyer.isOpen());
        // Il messaggio viene dal thread del manipolatore: arriva appena il
        // giro degli eventi glielo lascia passare.
        QVERIFY(failures.wait(3000));
        QVERIFY(failures.first().first().toString().contains(QStringLiteral("COM99")));
    }
};

QTEST_GUILESS_MAIN(TestCwKeyer)
#include "test_cw_keyer.moc"
