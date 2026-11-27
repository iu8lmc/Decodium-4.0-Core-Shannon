# Decodium 4 FT2 1.0.1009

## English (UK)

Changes since 1.0.1005 include the work incorporated from Martino's 1.0.1006–1.0.1008 releases and subsequent local fixes.

- **Older x86 processors:** isolate the AVX2/FMA decoder's C++ template instantiations from baseline code, addressing an illegal-instruction crash on CPUs without AVX2. Dispatch continues to select the decoder supported by the processor. The fix was exercised on an Ivy Bridge Linux machine and checked with MinGW cross-compiled objects; native Windows on an affected processor remains to be verified.
- **Logbook ADIF dialogue:** associate the Load and Import file selectors with the actual logbook window and respect its always-on-top setting, reducing the risk of a modal selector appearing behind a detached logbook. A/B tests cover selector ownership, cancellation and selection; actual Windows window ordering remains to be confirmed.
- **macOS development builds:** offer an optional, explicitly configured development-signing identity so Keychain authorisations can persist across rebuilds. The first authorisation for each credential may still be necessary; distribution signing is unchanged. Prevent a Ninja/MOC dependency cycle in the waterfall palette build.
- **FT8/FT4/FT2 performance:** optimise bit metrics and signal subtraction; add reference comparisons. Include FT2 metric alternatives, overflow protection and an optional sync-gated flipped-bit cap. Decoder choices and measured trade-offs are documented.
- **FT8 decoder research:** incorporate gated BICM-ID feedback, optional phase-trellis demodulation, (174,77) OSD and margin gates, with benchmark tools and documented defaults. These experimental paths have not been validated over the air.
- **Windows packaging and CI:** add runtime staging for Qt, MinGW, third-party libraries, QML, data and translations; link OpenSSL Crypto for encrypted SSTV sharing; improve build compatibility with GCC 16, feature-disabled configurations and sanitiser builds. Runtime dependencies include OpenSSL, FFTW, Hamlib, RTL-SDR, OpenJPEG and Boost where required.
- **Robustness and tests:** reject truncated BWF bext chunks; harden the SSTV WAV reader; update SSTV, FT2, weak-signal, waterfall, map, CW, DecoLink, file-dialog and SIMD regression coverage, build aggregation, timeouts and benchmarks. Include project configuration changes and development documentation.
- The previously reported Hound/SuperFox RR73 issue remains open.

## Italiano

Le modifiche rispetto alla 1.0.1005 comprendono il lavoro integrato dalle versioni 1.0.1006–1.0.1008 di Martino e le successive correzioni locali.

- **Processori x86 meno recenti:** isolamento delle istanziazioni C++ del decoder AVX2/FMA dal codice base, per correggere un arresto con istruzione non consentita sulle CPU senza AVX2. La selezione continua a scegliere il decoder supportato dal processore. Correzione provata su Linux Ivy Bridge e controllata su oggetti compilati con MinGW; resta da verificare Windows nativo su una CPU interessata.
- **Selettore ADIF del logbook:** i selettori Carica e Importa sono associati alla finestra effettiva del logbook e ne rispettano la priorità in primo piano, riducendo il rischio che una finestra modale rimanga nascosta. I test A/B coprono associazione, annullamento e selezione; l'ordine effettivo delle finestre su Windows resta da confermare.
- **Build di sviluppo macOS:** firma di sviluppo opzionale e configurata esplicitamente per conservare le autorizzazioni del Portachiavi tra ricompilazioni. Può rimanere necessaria la prima autorizzazione per ogni credenziale; la firma delle distribuzioni non cambia. Corretta una dipendenza circolare Ninja/MOC nella build della palette waterfall.
- **Prestazioni FT8/FT4/FT2:** ottimizzate metriche dei bit e sottrazione del segnale, con confronti rispetto al riferimento. Incluse varianti delle metriche FT2, protezione dall'overflow e limite opzionale dei bit invertiti condizionato dalla sincronizzazione. Scelte e misure sono documentate.
- **Ricerca decoder FT8:** integrati feedback BICM-ID condizionato, demodulazione opzionale a traliccio di fase, OSD (174,77) e gate a margine, con benchmark e valori predefiniti documentati. I percorsi sperimentali non sono stati validati via radio.
- **Pacchettizzazione e CI Windows:** preparazione del runtime con Qt, MinGW, librerie esterne, QML, dati e traduzioni; collegamento di OpenSSL Crypto per la condivisione SSTV cifrata; migliorata la compatibilità con GCC 16, configurazioni con funzioni disattivate e sanitizzatori. Le dipendenze includono, dove necessarie, OpenSSL, FFTW, Hamlib, RTL-SDR, OpenJPEG e Boost.
- **Robustezza e test:** rifiuto dei blocchi BWF bext troncati; rafforzato il lettore WAV SSTV; aggiornati test e benchmark SSTV, FT2, segnali deboli, waterfall, mappa, CW, DecoLink, selettore file e SIMD, aggregazione delle build e timeout. Incluse modifiche alla configurazione del progetto e documentazione di sviluppo.
- Il problema Hound/SuperFox RR73 precedentemente segnalato rimane aperto.
