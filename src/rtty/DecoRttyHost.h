// Il punto in cui DecoRTTY entra in Decodium.
//
// La radio e' quella di Decodium, e l'audio pure.
//
// Il progetto originale portava con se' il proprio trasporto di rete: VITA-49
// verso un FlexRadio, o il gateway per una FT-991A su un altro PC. Dentro
// Decodium non trovava mai niente, e giustamente — la radio e' una sola, sta
// su una porta seriale sola, e il CAT dell'applicazione ce l'ha gia' in mano.
// Quel trasporto e' stato tolto: RTTY prende frequenza, modo e PTT dal CAT di
// Decodium attraverso dei ganci, e i campioni dal suo percorso audio.
//
// Resta la scheda audio come seconda strada, per chi porta qui l'audio da un
// altro programma con un cavo virtuale: la' non si comanda niente, si ascolta
// e basta, e l'interfaccia lo dice.
//
// Qui si mettono insieme gli oggetti che nel progetto originale vivevano in
// main.cpp, cosi' il resto di Decodium ne vede uno solo. La proprieta' e' di
// questa classe: nessuno di essi sopravvive all'host.

#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QVector>

#include <vector>

#include "app/Language.h"
#include "app/MacroModel.h"
#include "app/QsoLog.h"
#include "app/ReceiveTextModel.h"
#include "app/Theme.h"
#include "app/RttyEngine.h"
#include "link/DecodiumLink.h"
#include "link/RadioHub.h"

class QQmlContext;
class QSettings;

namespace decortty {

class DecoRttyHost : public QObject
{
    Q_OBJECT

    // Vero quando il motore RTTY sta ricevendo: serve a Decodium per sapere se
    // il sottosistema e' vivo senza doverne conoscere i dettagli.
    Q_PROPERTY (bool attivo READ attivo NOTIFY attivoChanged)

public:
    explicit DecoRttyHost (QObject* parent = nullptr);
    ~DecoRttyHost () override;

    // I ganci verso la radio di Decodium. Si passano prima di avvia(), e sono
    // gli stessi che DecoPort usa per la stessa ragione: qui dentro non si
    // include DecodiumBridge, cosi' il sottosistema RTTY resta compilabile per
    // conto suo e i due non si tengono per il bavero a vicenda.
    void impostaGanciRadio (link::DecodiumLink::Ganci ganci);

    // Carica le impostazioni salvate e collega gli oggetti fra loro. Va
    // chiamata una volta, dopo la costruzione.
    void avvia (QSettings& impostazioni);

    // Scrive su disco quello che l'operatore ha cambiato. Nel progetto
    // originale questo avveniva una volta sola, alla chiusura del programma:
    // qui DecoRTTY non ha una chiusura propria — la finestra si apre e si
    // richiude mentre l'applicazione continua — e il nominativo appena scritto
    // non arrivava mai su disco. Si salva percio' quando le cose cambiano,
    // con un ritardo perche' scrivere a ogni tasto non serve a nessuno.
    void salvaImpostazioni ();

    // I campioni che l'applicazione ha gia' in mano, a 12 kHz, per il
    // decodificatore RTTY. E' la stessa sorgente che alimenta FT8: RTTY la
    // legge, non se la prende.
    void consegnaAudio (const std::vector<float>& campioni12k, int frames);

    // Espone gli oggetti al QML con gli stessi nomi del progetto originale, in
    // modo che i file .qml di DecoRTTY funzionino senza modifiche.
    void esponiAlQml (QQmlContext& contesto, QString const& versione);

    bool attivo () const { return m_attivo; }

    link::RadioHub&           radio        () { return m_radio; }
    app::RttyEngine&          motore       () { return m_motore; }
    app::ReceiveTextModel&    testoRicevuto() { return m_testoRicevuto; }
    app::MacroModel&          macro        () { return m_macro; }
    app::QsoLog&              logQso       () { return m_logQso; }
    app::Language&            lingua       () { return m_lingua; }
    app::Theme&               tema         () { return m_tema; }

signals:
    void attivoChanged ();

    // Una riga di testo completa, pronta per la lista dei decodificati di
    // Decodium. RTTY e' un flusso continuo: il testo scorre nella finestra
    // dedicata carattere per carattere, mentre qui esce solo quando la riga e'
    // chiusa — da un ritorno a capo o da una pausa nel segnale. Senza questo
    // taglio la lista si riempirebbe di frammenti.
    void rigaDecodificata (QString const& testo, double qualita, double frequenzaHz);


private:
    void collegaTestoRicevuto ();
    void programmaSalvataggio ();
    void accumulaCarattere (QString const& carattere, double qualita);
    void chiudiRiga ();

    QString m_rigaInCorso;
    double  m_qualitaSomma {0.0};
    int     m_qualitaConteggio {0};
    QTimer* m_pausaRiga {nullptr};
    // Il ritardo fra l'ultima modifica e la scrittura su disco.
    QTimer* m_ritardoSalvataggio {nullptr};
    // L'archivio delle impostazioni RTTY, di proprieta' dell'host: quello
    // passato ad avvia() e' un riferimento a un oggetto che vive solo per la
    // durata della chiamata, e tenerlo sarebbe stato un puntatore penzolante.
    QSettings* m_impostazioni {nullptr};

    app::Language          m_lingua;
    link::RadioHub         m_radio;
    app::RttyEngine        m_motore;
    app::ReceiveTextModel  m_testoRicevuto;
    app::MacroModel        m_macro;
    app::QsoLog            m_logQso;
    link::DecodiumLink::Ganci m_ganci;
    // Il buffer della conversione 12->24 kHz, tenuto qui per non
    // riallocarlo a ogni blocco di campioni.
    std::vector<float>     m_audio24;
    app::Theme             m_tema;
    bool                   m_attivo {false};
};

} // namespace decortty
