# Decodium 4 FT2 1.0.322

Release incrementale basata su 1.0.321, con correzioni mirate alla pipeline AutoCQ/AutoSeq e ai falsi positivi FT2 diretti verso la propria stazione.

## Correzioni principali

- Aggiunta una pulizia esplicita dello stato QSO completato quando il sequencer torna a CQ/TX6.
- Evitato che latch residui di signoff (`73`/`RR73`), retry TX5 o TX pendenti del QSO precedente possano influenzare il primo decode di un nuovo chiamante.
- Corretto il caso in cui, dopo un QSO chiuso con `73`, AutoCQ riparte in CQ, riceve un nuovo rapporto (`+NN`/`-NN`) ma poteva saltare direttamente al `73` senza aver prima inviato il rapporto/R-report.
- Il reset viene applicato nei percorsi centrali di selezione TX6, avanzamento stato QSO verso CQ e riattivazione AutoCQ quando lo stato e' idle/completato.

## Filtro decode spuri diretti

- Aggiunto un filtro centralizzato per decode "to me" sospetti, applicato prima di farli entrare in Signal RX, AutoSeq, resume-on-reply e code Multi Answer/AutoCQ.
- I decode diretti con prefisso/DXCC non valido, struttura call sospetta o locator incoerente con la DXCC vengono declassati/soppressi dalla pipeline operativa.
- I TX manuali e i QSO gia' attivi restano preservati: il filtro non blocca un nominativo che e' gia' target manuale, partner attivo, lock AutoCQ, messaggio TX corrente o TX appena inviato.

## Packaging

- Versione locale aggiornata a 1.0.322.
- Aggiornati gli installer Windows Inno Setup e NSIS alla numerazione 1.0.322.
- Asset previsti: sorgenti GitHub, installer Windows x64, AppImage Linux x86_64 Qt 6.11, DMG macOS Apple Silicon e DMG macOS Intel.
