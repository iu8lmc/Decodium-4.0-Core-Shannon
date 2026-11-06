# DecoRTTY innestato in Decodium 4.0 — passo 1: le librerie.
#
# Deriva dal CMakeLists di DecoRTTY 0.7.2, ridotto alle sole librerie statiche:
# gli eseguibili del progetto originale (l'applicazione, il gateway FT-991A e
# gli strumenti di prova) non servono qui, perche' il codice viene collegato
# dentro Decodium.
#
# Il trasporto di rete del progetto originale — VITA-49 verso i FlexRadio e il
# gateway FT-991A — non c'e' piu': dentro Decodium cercava radio in rete che non
# potevano esserci, perche' la radio e' quella dell'applicazione, su una porta
# seriale sola. Chi risincronizza con DecoRTTY non le ritrovera' qui, ed e'
# voluto: non sono state dimenticate.

# AUTOMOC acceso PRIMA di definire i target: Decodium lo attiva a meta' del
# suo CMakeLists (riga ~1627) mentre questo sottoprogetto viene aggiunto prima
# (riga ~1370) e non lo eredita. Impostarlo dopo, con set_target_properties,
# non basta: le regole di generazione sono gia' state scritte e mocs_compilation
# non viene prodotto. decortty_dsp lo spegne per conto suo piu' sotto: non
# contiene oggetti Qt, come nell'originale.
set (CMAKE_AUTOMOC ON)

# ── DSP: demodulatore FSK, Baudot, Viterbi ──────────────────────────────────
# Nessun oggetto Qt: AUTOMOC spento come nell'originale.
add_library (decortty_dsp STATIC
    dsp/Fft.cpp                 dsp/Fft.h
    dsp/Baudot.cpp              dsp/Baudot.h
    dsp/RttyTypes.h
    dsp/RttyDemodulator.cpp     dsp/RttyDemodulator.h
    dsp/ViterbiBaudot.cpp       dsp/ViterbiBaudot.h
    dsp/FirDecimator.cpp        dsp/FirDecimator.h
    dsp/FskModulator.cpp        dsp/FskModulator.h
    dsp/RxFilters.cpp           dsp/RxFilters.h
    dsp/AutoTuner.cpp           dsp/AutoTuner.h
)
target_include_directories (decortty_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties (decortty_dsp PROPERTIES AUTOMOC OFF)
if (MSVC)
  target_compile_options (decortty_dsp PRIVATE /W4)
else ()
  target_compile_options (decortty_dsp PRIVATE -Wall -Wextra -ffast-math)
endif ()

# ── I collegamenti alla radio ───────────────────────────────────────────────
# Ne resta uno solo, e non passa dalla rete: la radio che Decodium governa. Il
# trasporto VITA-49 verso i FlexRadio e il gateway FT-991A sono stati tolti
# perche' dentro Decodium non trovavano mai nulla, e con loro se n'e' andata
# anche la scheda audio: chi decide da dove arrivano audio e comandi e'
# Decodium, in un posto solo, e RTTY non ha piu' niente da connettere.
add_library (decortty_link STATIC
    link/RadioLink.h
    link/DecodiumLink.cpp       link/DecodiumLink.h
    link/RadioHub.cpp           link/RadioHub.h
    # Il piano di banda e' un dato puro (solo QString e vector) ma RadioHub lo
    # usa: sta qui e non in decortty_app, che dipende da questa libreria e non
    # viceversa.
    app/BandPlan.cpp            app/BandPlan.h
)
target_link_libraries (decortty_link PUBLIC
    Qt6::Core decortty_dsp)

# ── Motore e modelli dell'applicazione ──────────────────────────────────────
# Il waterfall e' un QQuickItem: qui serve perche' RttyEngine lo referenzia.
# L'aggancio all'interfaccia di Decodium e' il passo 4 del piano.
add_library (decortty_app STATIC
    app/RttyEngine.cpp          app/RttyEngine.h
    app/ReceiveTextModel.cpp    app/ReceiveTextModel.h
    app/WaterfallItem.cpp       app/WaterfallItem.h
    app/MacroModel.cpp          app/MacroModel.h
    app/QsoLog.cpp              app/QsoLog.h
    app/Language.cpp            app/Language.h
    # Solo intestazione, ma con Q_OBJECT: sta fra i sorgenti perche' AUTOMOC lo
    # veda. E' la tavolozza, condivisa con Decodium.
    app/Theme.h
)
target_link_libraries (decortty_app PUBLIC
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Multimedia
    decortty_dsp decortty_link)

# La versione finisce nell'intestazione degli ADIF esportati: chi rilegge quel
# file deve poter dire con che cosa e' stato scritto. La definiva la libreria
# del gateway, che non c'e' piu'; il valore resta quello del progetto da cui
# questo codice viene.
target_compile_definitions (decortty_app PUBLIC
    DECORTTY_VERSION="${DECORTTY_VERSION}")

# ── Il punto di aggancio a Decodium ─────────────────────────────────────────
# Raccoglie gli oggetti che nel progetto originale vivevano in main.cpp, cosi'
# il resto di Decodium ne vede uno solo. Vedi il passo 2 del piano.
add_library (decortty_host STATIC
    DecoRttyHost.cpp            DecoRttyHost.h
)
target_link_libraries (decortty_host PUBLIC decortty_app)

# I warning restano fatali anche qui, come nel resto di Decodium.
# La tentazione era di allentarli, visto che questo sottoprogetto va
# risincronizzato con il repository DecoRTTY: il primo che si e' presentato
# pero' — 'unused variable c' in ViterbiBaudot.cpp — non era stile ma un
# difetto vero. L'indice del ciclo sui 32 codici Baudot faceva ombra alla
# certezza del frame, e step.certainty finiva per contenere il numero del
# codice invece della certezza, rendendo priva di senso la sfumatura con cui
# l'interfaccia mostra quanto un carattere sia sicuro. Corretto a monte, in
# DecoRTTY, cosi' la prossima sincronizzazione non lo riporta indietro.

# DecoRTTY e' scritto in C++20 (usa std::numbers), Decodium compila a C++17.
# Lo standard si alza sui soli target di questo sottoprogetto: il resto di
# Decodium resta a 17, e nessun altro bersaglio cambia comportamento.
foreach (bersaglio decortty_dsp decortty_link
                   decortty_app decortty_host)
  set_target_properties (${bersaglio} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON)
endforeach ()
