// Prova dell'analizzatore della stringa di stato SPE.
//
// E' la parte che merita una verifica automatica: una trama malformata letta
// come buona darebbe numeri assurdi proprio mentre si trasmette a piena
// potenza, che e' il momento peggiore per fidarsi di una misura.
//
// Protocollo: doc/protocollo-spe-expert.md

#include <QtTest>

#include "src/radio/DecodiumAmplifier.h"

namespace {

// Costruisce una trama valida come la manda l'amplificatore:
// 0xAA 0xAA 0xAA <lunghezza> <corpo> <chk0> <chk1> CR LF
QByteArray frame(const QByteArray& body)
{
    int sum = 0;
    for (unsigned char c : body)
        sum += c;
    QByteArray f;
    f.append(char(0xAA)).append(char(0xAA)).append(char(0xAA));
    f.append(char(body.size()));
    f.append(body);
    f.append(char(sum % 256));
    f.append(char((sum / 256) % 256));
    f.append("\r\n");
    return f;
}

// Esempio della guida del costruttore, in trasmissione.
QByteArray const kTx =
    "20K,O,T,x,1,10,1a,0r,H,0407, 1.15, 1.20, 48.0, 13.1, 37,  0,  0,N,N";
QByteArray const kRx =
    "20K,O,R,x,1,10,1a,0r,H,0000, 0.00, 0.00,  0.0,  0.0, 33,  0,  0,N,N";

} // namespace


class TestAmplifierTelemetry final : public QObject
{
    Q_OBJECT

private slots:
    void leggeLaTrasmissione()
    {
        auto const r = DecodiumAmplifier::parseSpeStatus(frame(kTx));
        QVERIFY(r.valid);
        QVERIFY(r.transmitting);
        QCOMPARE(r.watts, 407.0);
        QCOMPARE(r.swr, 1.20);
        QCOMPARE(r.swrAtu, 1.15);
        QCOMPARE(r.voltage, 48.0);
        QCOMPARE(r.current, 13.1);
        QCOMPARE(r.temperature, 37);
        QCOMPARE(r.model, QStringLiteral("20K"));
        QCOMPARE(r.alarm, QStringLiteral("N"));
    }

    void inRicezioneLaPotenzaEZero()
    {
        auto const r = DecodiumAmplifier::parseSpeStatus(frame(kRx));
        QVERIFY(r.valid);
        QVERIFY(!r.transmitting);
        QCOMPARE(r.watts, 0.0);
        // Con ROS a zero in ricezione si riporta 1.0, non un rapporto
        // impossibile: uno strumento non deve mai mostrare un ROS < 1.
        QCOMPARE(r.swr, 1.0);
    }

    void rifiutaSommaDiControlloErrata()
    {
        QByteArray f = frame(kTx);
        f[f.size() - 4] = char(quint8(f.at(f.size() - 4)) ^ 0xFF);
        QVERIFY(!DecodiumAmplifier::parseSpeStatus(f).valid);
    }

    void rifiutaTrameMutileESpazzatura()
    {
        QByteArray const f = frame(kTx);
        QVERIFY(!DecodiumAmplifier::parseSpeStatus(f.left(10)).valid);
        QVERIFY(!DecodiumAmplifier::parseSpeStatus(QByteArray()).valid);
        QVERIFY(!DecodiumAmplifier::parseSpeStatus(QByteArrayLiteral("\x01\x02rumore")).valid);
        // sincronismo presente ma corpo troncato
        QVERIFY(!DecodiumAmplifier::parseSpeStatus(f.left(20)).valid);
    }

    void rifiutaUnCorpoConTroppiPochiCampi()
    {
        QVERIFY(!DecodiumAmplifier::parseSpeStatus(frame("20K,O,T,x,1")).valid);
    }

    void estraeLeTrameDaUnFlussoDisturbato()
    {
        // Come si presenta la linea davvero in ascolto passivo: le richieste
        // del software del costruttore, le risposte, e qualche byte di
        // disturbo in mezzo.
        QByteArray flusso;
        flusso.append(DecodiumAmplifier::statusRequest());
        flusso.append(frame(kTx));
        flusso.append(DecodiumAmplifier::statusRequest());
        flusso.append(frame("20K,O,T,x,1,10,1a,0r,H,0512, 1.15, 1.35, 47.8, 15.4, 39,  0,  0,N,N"));
        flusso.append("\x00\xFF", 2);
        flusso.append(frame(kRx));

        auto const letture = DecodiumAmplifier::harvest(flusso);
        QCOMPARE(letture.size(), 3);
        QCOMPARE(letture.at(0).watts, 407.0);
        QCOMPARE(letture.at(1).watts, 512.0);
        QCOMPARE(letture.at(1).swr, 1.35);
        QVERIFY(!letture.at(2).transmitting);
    }

    void unaTramaSpezzataSiCompletaAlPezzoSuccessivo()
    {
        // La seriale consegna a pezzi: meta' trama ora, meta' fra un istante.
        // Il resto non deve andare perso.
        QByteArray const f = frame(kTx);
        QByteArray buf = f.left(12);
        QVERIFY(DecodiumAmplifier::harvest(buf).isEmpty());
        buf.append(f.mid(12));
        auto const letture = DecodiumAmplifier::harvest(buf);
        QCOMPARE(letture.size(), 1);
        QCOMPARE(letture.at(0).watts, 407.0);
        QVERIFY(buf.isEmpty());
    }

    void laRichiestaDiStatoEQuellaDellaGuida()
    {
        QByteArray const atteso("\x55\x55\x55\x01\x90\x90", 6);
        QCOMPARE(DecodiumAmplifier::statusRequest(), atteso);
    }
};

QTEST_MAIN(TestAmplifierTelemetry)
#include "test_amplifier_telemetry.moc"
