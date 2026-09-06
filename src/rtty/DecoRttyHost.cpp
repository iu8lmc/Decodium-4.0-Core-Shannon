#include "DecoRttyHost.h"

#include <QQmlContext>
#include <QSettings>
#include <algorithm>

namespace decortty {

DecoRttyHost::DecoRttyHost (QObject* parent)
    : QObject {parent}
{
}

DecoRttyHost::~DecoRttyHost () = default;

void DecoRttyHost::accumulaCarattere (QString const& carattere, double qualita)
{
    // Un ritorno a capo chiude la riga; il resto si accumula. La qualita' della
    // riga e' la media di quelle dei suoi caratteri: un carattere dubbio in
    // mezzo a venti buoni non deve far sembrare incerta tutta la riga.
    // Confronto sui codici invece che su stringhe di escape: 0x0A e 0x0D
    // sono il ritorno a capo e il ritorno carrello del flusso Baudot.
    QChar const primo = carattere.at (0);
    if (primo == QChar (0x0A) || primo == QChar (0x0D)) {
        chiudiRiga ();
        return;
    }
    m_rigaInCorso += carattere;
    m_qualitaSomma += qualita;
    ++m_qualitaConteggio;

    // Una riga lunghissima senza ritorni a capo esiste: il traffico RTTY non e'
    // sempre formattato. Si chiude d'ufficio per non tenerla in sospeso.
    if (m_rigaInCorso.size () >= 120) {
        chiudiRiga ();
        return;
    }
    // Ogni carattere rimanda la chiusura per pausa: quando il segnale tace, la
    // riga esce da sola invece di restare appesa fino al prossimo carattere.
    m_pausaRiga->start ();
}

void DecoRttyHost::chiudiRiga ()
{
    m_pausaRiga->stop ();
    QString const testo = m_rigaInCorso.trimmed ();
    double const qualita = m_qualitaConteggio > 0
                               ? m_qualitaSomma / m_qualitaConteggio
                               : 0.0;
    m_rigaInCorso.clear ();
    m_qualitaSomma = 0.0;
    m_qualitaConteggio = 0;

    // Le righe di soli diddle o di rumore non meritano una riga in lista.
    if (testo.size () < 3)
        return;
    emit rigaDecodificata (testo, qualita, m_motore.markHz ());
}

void DecoRttyHost::collegaTestoRicevuto ()
{
    // Caratteri decodificati e trasmessi finiscono nella stessa finestra, cosi'
    // l'operatore legge il QSO come una conversazione. E' il comportamento del
    // progetto originale e va mantenuto.
    connect (&m_motore, &app::RttyEngine::characterDecoded, &m_testoRicevuto,
             [this] (QString const& testo, double qualita, bool corretto) {
                 if (testo.isEmpty ())
                     return;
                 m_testoRicevuto.appendCharacter (testo.at (0), qualita, corretto);
                 accumulaCarattere (testo, qualita);
             });
    connect (&m_motore, &app::RttyEngine::characterTransmitted, &m_testoRicevuto,
             [this] (QString const& testo) {
                 if (!testo.isEmpty ())
                     m_testoRicevuto.appendTransmitted (testo.at (0));
             });
}

void DecoRttyHost::impostaGanciRadio (link::DecodiumLink::Ganci ganci)
{
    m_ganci = std::move (ganci);
}

void DecoRttyHost::consegnaAudio (const std::vector<float>& campioni12k, int frames)
{
    if (frames <= 0 || campioni12k.empty ())
        return;

    // Due conversioni, e nessuna delle due e' facoltativa.
    //
    // La prima e' la frequenza. Decodium lavora a 12 kHz, il motore RTTY vuole
    // 24: dentro decima di 3 fino a 8 kHz, dove sta il demodulatore. Dandogli
    // 12 kHz decimerebbe fino a 4, e il tono mark a 2125 Hz finirebbe sopra
    // Nyquist — non "un po' peggio": irriconoscibile. Si raddoppia percio' con
    // interpolazione lineare, che a 2 kHz su 24 attenua in modo trascurabile e
    // le cui immagini cadono dove il filtro del decimatore le toglie.
    //
    // La seconda e' il formato: il motore legge stereo interlacciato e ne fa
    // la media. Lo stesso campione va quindi su entrambi i canali.
    int const n = std::min (frames, static_cast<int> (campioni12k.size ()));
    if (n < 2)
        return;
    int const frames24 = n * 2;
    m_audio24.resize (static_cast<size_t> (frames24) * 2);

    for (int i = 0; i < n; ++i) {
        float const a = campioni12k[i];
        // Il campione interpolato sta a meta' fra questo e il successivo;
        // sull'ultimo non c'e' un successivo e si tiene fermo il valore.
        float const b = (i + 1 < n) ? campioni12k[i + 1] : a;
        float const meta = 0.5f * (a + b);

        size_t const j = static_cast<size_t> (i) * 4;
        m_audio24[j + 0] = a;      m_audio24[j + 1] = a;
        m_audio24[j + 2] = meta;   m_audio24[j + 3] = meta;
    }
    m_radio.consegnaAudioDecodium (m_audio24, frames24);
}

void DecoRttyHost::avvia (QSettings& impostazioni)
{
    // Due secondi di silenzio chiudono la riga: a 45,45 baud sono circa nove
    // caratteri, abbastanza perche' una pausa vera si distingua dallo spazio
    // fra una parola e l'altra.
    m_pausaRiga = new QTimer (this);
    m_pausaRiga->setSingleShot (true);
    m_pausaRiga->setInterval (2000);
    connect (m_pausaRiga, &QTimer::timeout, this, &DecoRttyHost::chiudiRiga);

    m_macro.load (impostazioni);

    // Un archivio nostro, con le stesse coordinate di quello che ci hanno
    // passato: quello e' un riferimento a un oggetto che muore appena avvia()
    // ritorna, e per salvare piu' tardi ce ne vuole uno che resti.
    m_impostazioni = new QSettings (impostazioni.format (), impostazioni.scope (),
                                    impostazioni.organizationName (),
                                    impostazioni.applicationName (), this);

    m_ritardoSalvataggio = new QTimer (this);
    m_ritardoSalvataggio->setSingleShot (true);
    m_ritardoSalvataggio->setInterval (1500);
    connect (m_ritardoSalvataggio, &QTimer::timeout, this, &DecoRttyHost::salvaImpostazioni);

    // Quello che l'operatore cambia va su disco da se'. I dati di stazione
    // passano da qsoChanged; i parametri del decodificatore dai loro segnali.
    connect (&m_macro, &app::MacroModel::qsoChanged, this, &DecoRttyHost::programmaSalvataggio);
    connect (&m_macro, &app::MacroModel::modelReset, this, &DecoRttyHost::programmaSalvataggio);
    connect (&m_motore, &app::RttyEngine::paramsChanged, this, &DecoRttyHost::programmaSalvataggio);

    m_motore.attachRadio (&m_radio);

    // Ci si attacca subito alla radio dell'applicazione: non c'e' niente da
    // cercare e niente da scegliere. Chi preferisce l'audio da un cavo
    // virtuale cambia strada dalla finestra RTTY, e quella scelta sostituisce
    // questa.
    m_radio.collegaADecodium (m_ganci);

    collegaTestoRicevuto ();

    // Impostazioni del decodificatore, con gli stessi valori di partenza del
    // progetto originale: chi passa da DecoRTTY a Decodium ritrova la sua
    // configurazione, perche' le chiavi sono le stesse.
    m_motore.setMarkHz            (impostazioni.value (QStringLiteral ("rtty/markHz"), 2125.0).toDouble ());
    m_motore.setShiftHz           (impostazioni.value (QStringLiteral ("rtty/shiftHz"), 170.0).toDouble ());
    m_motore.setBaud              (impostazioni.value (QStringLiteral ("rtty/baud"), 45.45).toDouble ());
    m_motore.setReverse           (impostazioni.value (QStringLiteral ("rtty/reverse"), false).toBool ());
    m_motore.setUnshiftOnSpace    (impostazioni.value (QStringLiteral ("rtty/usos"), true).toBool ());
    m_motore.setAfcEnabled        (impostazioni.value (QStringLiteral ("rtty/afc"), true).toBool ());
    m_motore.setAutoTuneEnabled   (impostazioni.value (QStringLiteral ("rtty/autoTune"), true).toBool ());
    m_motore.setSquelchDb         (impostazioni.value (QStringLiteral ("rtty/squelchDb"), 4.0).toDouble ());
    m_motore.setCorrectionDepth   (impostazioni.value (QStringLiteral ("rtty/correctionDepth"), 4).toInt ());
    m_motore.setTransmitLevel     (impostazioni.value (QStringLiteral ("rtty/transmitLevel"), 0.35).toDouble ());
    m_motore.setStopBits          (impostazioni.value (QStringLiteral ("rtty/stopBits"), 1.5).toDouble ());
    m_motore.setDataBits          (impostazioni.value (QStringLiteral ("rtty/dataBits"), 5).toInt ());
    m_motore.setParity            (impostazioni.value (QStringLiteral ("rtty/parity"), 0).toInt ());
    m_motore.setFiguresSet        (impostazioni.value (QStringLiteral ("rtty/figuresSet"), 0).toInt ());
    m_motore.setIgnoreFramingErrors (impostazioni.value (QStringLiteral ("rtty/ignoreFraming"), false).toBool ());
    m_motore.setBandpassEnabled   (impostazioni.value (QStringLiteral ("rtty/bandpass"), false).toBool ());
    m_motore.setBandpassWidthHz   (impostazioni.value (QStringLiteral ("rtty/bandpassWidth"), 500.0).toDouble ());
    m_motore.setLmsEnabled        (impostazioni.value (QStringLiteral ("rtty/lms"), false).toBool ());
    m_motore.setDiddleMode        (impostazioni.value (QStringLiteral ("rtty/diddleMode"), 1).toInt ());
    m_motore.setCharacterWaitBits (impostazioni.value (QStringLiteral ("rtty/charWait"), 0.0).toDouble ());

    m_attivo = true;
    emit attivoChanged ();
}

void DecoRttyHost::programmaSalvataggio ()
{
    if (m_ritardoSalvataggio)
        m_ritardoSalvataggio->start ();
}

void DecoRttyHost::salvaImpostazioni ()
{
    if (!m_impostazioni)
        return;

    QSettings& s = *m_impostazioni;
    s.setValue (QStringLiteral ("rtty/markHz"),          m_motore.markHz ());
    s.setValue (QStringLiteral ("rtty/shiftHz"),         m_motore.shiftHz ());
    s.setValue (QStringLiteral ("rtty/baud"),            m_motore.baud ());
    s.setValue (QStringLiteral ("rtty/reverse"),         m_motore.reverse ());
    s.setValue (QStringLiteral ("rtty/usos"),            m_motore.unshiftOnSpace ());
    s.setValue (QStringLiteral ("rtty/afc"),             m_motore.afcEnabled ());
    s.setValue (QStringLiteral ("rtty/autoTune"),        m_motore.autoTuneEnabled ());
    s.setValue (QStringLiteral ("rtty/squelchDb"),       m_motore.squelchDb ());
    s.setValue (QStringLiteral ("rtty/correctionDepth"), m_motore.correctionDepth ());
    s.setValue (QStringLiteral ("rtty/transmitLevel"),   m_motore.configuredTransmitLevel ());
    s.setValue (QStringLiteral ("rtty/stopBits"),        m_motore.stopBits ());
    s.setValue (QStringLiteral ("rtty/dataBits"),        m_motore.dataBits ());
    s.setValue (QStringLiteral ("rtty/parity"),          m_motore.parity ());
    s.setValue (QStringLiteral ("rtty/figuresSet"),      m_motore.figuresSet ());
    s.setValue (QStringLiteral ("rtty/ignoreFraming"),   m_motore.ignoreFramingErrors ());
    s.setValue (QStringLiteral ("rtty/bandpass"),        m_motore.bandpassEnabled ());
    s.setValue (QStringLiteral ("rtty/bandpassWidth"),   m_motore.bandpassWidthHz ());
    s.setValue (QStringLiteral ("rtty/lms"),             m_motore.lmsEnabled ());
    s.setValue (QStringLiteral ("rtty/diddleMode"),      m_motore.diddleMode ());
    s.setValue (QStringLiteral ("rtty/charWait"),        m_motore.characterWaitBits ());
    m_macro.save (s);

    // Scrittura immediata, non quando a QSettings pare: se l'applicazione si
    // chiude male, la serata di regolazioni se ne andrebbe senza lasciare
    // traccia. E' successo davvero nel progetto originale, e il commento che
    // lo ricordava e' arrivato fin qui.
    s.sync ();
}

void DecoRttyHost::esponiAlQml (QQmlContext& contesto, QString const& versione)
{
    // Stessi nomi del progetto originale: i file .qml di DecoRTTY funzionano
    // senza modifiche, e le loro correzioni si possono riportare qui cosi'
    // come sono.
    contesto.setContextProperty (QStringLiteral ("radio"),       &m_radio);
    contesto.setContextProperty (QStringLiteral ("rtty"),        &m_motore);
    contesto.setContextProperty (QStringLiteral ("receiveText"), &m_testoRicevuto);
    contesto.setContextProperty (QStringLiteral ("macros"),      &m_macro);
    contesto.setContextProperty (QStringLiteral ("qsoLog"),      &m_logQso);
    contesto.setContextProperty (QStringLiteral ("language"),    &m_lingua);
    // Nel progetto originale Theme e' il singleton di un modulo QML; qui e' una
    // proprieta' di contesto come le altre, perche' Decodium carica il QML dal
    // filesystem e non registra moduli. I file .qml continuano a scrivere
    // Theme.bgDeep senza sapere da dove arriva.
    contesto.setContextProperty (QStringLiteral ("Theme"),       &m_tema);
    contesto.setContextProperty (QStringLiteral ("appVersion"),  versione);
}

} // namespace decortty
