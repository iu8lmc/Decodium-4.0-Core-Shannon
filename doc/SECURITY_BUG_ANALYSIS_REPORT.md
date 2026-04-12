# Security and Bug Analysis Report (Decodium3)

Questo documento consolida l'analisi statica del codice su sicurezza, stabilita' e quality risks.
Include anche i punti aggiuntivi richiesti su SSL, Fortran/C++, UI blocking, shared memory macOS e gestione memoria.

## Sintesi Esecutiva

Aggiornamento remediation (2026-02-27):
- Fase 0 TLS implementata in codice (`FoxVerifier`, `NetworkAccessManager`).
- Fase 1 avviata: hardening bounds `dataSink/refspectrum`, parsing OTP robusto, lifecycle `FoxVerifier` migliorato, `processEvents` meno re-entrante.
- CAT resilience: aggiunto reconnect automatico non bloccante in `MainWindow` e apertura CAT "silent" per i path periodici, per evitare instabilita' quando il backend CAT diventa disponibile dopo l'avvio.

- `Mitigato`: bypass della validazione TLS in `FoxVerifier` (MITM sul flusso di verifica).
- `Mitigato`: gestione permissiva degli errori TLS nel `NetworkAccessManager` (MITM esteso alle richieste di rete).
- `Alto (parzialmente mitigato)`: rischio overflow/corruzione memoria nel bridge C++/Fortran (array globali + dimensioni pre-calcolate).
- `Medio-Alto (parzialmente mitigato)`: logica sincrona pesante sul thread UI (`mainwindow.cpp`), possibili freeze/beachball.
- `Medio`: dipendenza da `QSharedMemory`/System V su macOS con tuning `sysctl` obbligatorio.
- `Medio`: gestione ownership con puntatori grezzi in piu' punti, rischio leak su path non lineari.
- `Mitigato`: uso `qrand()` legacy/deprecato rimosso dal codice runtime.

## 1) Vulnerabilita' Sicurezza

### 1.1 Critico (ora mitigato) - TLS Certificate Validation Bypass (CWE-295)

- File (origine): `Network/FoxVerifier.cpp` (pre-fix)
- Evidenza originale: nel callback `sslErrors(...)` veniva invocato `reply_->ignoreSslErrors();`.
- Impatto: un attacker in rete puo' intercettare e alterare risposte di verifica (MITM), invalidando il meccanismo di trust.
- Raccomandazione:
  - rimuovere `ignoreSslErrors()` dal percorso standard;
  - lasciare la validazione al trust store OS;
  - se serve un'eccezione (dev/test), limitarla a un certificato/errore specifico con pinning esplicito.
- Stato remediation:
  - implementato: callback TLS ora rifiuta certificati insicuri e abortisce la reply.
  - riferimento: `Network/FoxVerifier.cpp:94-110`.

### 1.2 Critico (ora mitigato) - Ignore SSL Errors in NetworkAccessManager (CWE-295)

- File (origine): `Network/NetworkAccessManager.cpp` (pre-fix)
- Evidenza originale:
  - `filter_SSL_errors(...)` accumulava errori in `allowed_ssl_errors_` e li ignorava con `reply->ignoreSslErrors(...)`;
  - `createRequest(...)` applicava preventivamente `ignoreSslErrors(allowed_ssl_errors_)` a ogni request.
- Impatto: eccezioni TLS possono propagarsi globalmente e ridurre la sicurezza di altri endpoint.
- Raccomandazione:
  - eliminare ignore globale in `createRequest`;
  - scoping per `host:port` + fingerprint certificato;
  - mai persistere/riusare eccezioni in modo trasversale tra domini differenti.
- Stato remediation:
  - implementato: rimosso pre-ignore globale da `createRequest(...)`;
  - implementato: eccezioni TLS solo scoped (`host|port|error|certDigest`) e applicate solo alle reply compatibili;
  - riferimenti: `Network/NetworkAccessManager.cpp:10-22`, `:37-85`, `:87-90`; `Network/NetworkAccessManager.hpp:32`.

## 2) Robustezza e Stabilita'

### 2.1 Alto - Buffer Overflows Potenziali nel Bridge C++/Fortran (CWE-120)

- File: `widgets/mainwindow.h:72`, `:85-86`; `widgets/mainwindow.cpp:122-193`, `:238-241`
- Evidenza:
  - array globali C-style (`itone`, `icw`, `dec_data`) passati a routine Fortran `extern "C"` (`symspec_`, `genft8_`, ecc.);
  - dimensioni gestite con costanti/parametri runtime e senza guardie centralizzate pre-call.
- Impatto: su indici/size errati si puo' arrivare a memoria corrotta o segfault.
- Raccomandazione:
  - introdurre check bounds obbligatori prima di ogni call Fortran critica;
  - usare helper unificato per validare `nsym/nsps/nwave/kin` e dimensioni buffer;
  - aggiungere `Q_ASSERT`/fail-fast in debug e clamp/log in release.
- Stato remediation:
  - implementato (parziale): clamp dei frame in `dataSink(...)` sul limite reale di `dec_data.d2`;
  - implementato (parziale): indice `refspectrum` clamped (`k - m_nsps/2`) per evitare puntatori out-of-range;
  - implementato (parziale): `fastSink(...)` ora riceve `k` validato.
  - riferimenti: `widgets/mainwindow.cpp:2548-2582`.

### 2.2 Medio-Alto - CPU/UI Blocking sul Main Thread

- File: `widgets/mainwindow.cpp` (`guiUpdate()`, `decodeDone()`, loop con `qApp->processEvents()` a `:5853` e `:6355`)
- Evidenza: grandi blocchi di logica decode/UI/I-O nel thread dell'interfaccia.
- Impatto: UI lag, beachball su macOS, variabilita' della latenza in periodi di decode intensi.
- Raccomandazione:
  - migrare calcoli pesanti e I/O massivo su worker (`QtConcurrent`/thread dedicati);
  - mantenere in UI thread solo repaint e state update minimale;
  - introdurre metriche (tempo medio/max di `guiUpdate`/`decodeDone`).
- Stato remediation:
  - implementato (parziale): sostituito `qApp->processEvents()` con `QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents)` in due loop critici waterfall/MSK, riducendo la re-entrancy da input utente.
  - riferimenti: `widgets/mainwindow.cpp:350-353`, `:5877`, `:6379`.

### 2.3 Medio - QSharedMemory su macOS (System V fragility)

- File: `main.cpp:427-455`, `:431-446`; `Darwin/ft2-pkg-postinstall.sh:32-33`; `Darwin/com.ft2.jtdx.sysctl.plist:12-13`
- Evidenza: l'app dipende da limiti `kern.sysv.shmmax/shmall`; se non corretti l'avvio puo' fallire.
- Impatto: instabilita' operativa e setup fragile tra versioni macOS/macchine diverse.
- Raccomandazione:
  - progettare fallback a file memory-mapped (`QFile::map`/`mmap`);
  - mantenere `QSharedMemory` solo come path compatibilita';
  - diagnosticare e fallback automatico, evitando hard-stop in bootstrap.

### 2.4 Medio - Gestione Memoria con Puntatori Grezzi

- File esempio: `widgets/mainwindow.cpp:7317-7325` (`new FoxVerifier(...)` accumulato in lista)
- Evidenza: uso diffuso di `new` con ownership non sempre esplicita/centralizzata.
- Impatto: rischio leak su rami d'errore, early-return o lifecycle complessi.
- Nota: parte del codice usa ownership Qt parent-child, ma non copre tutti i casi.
- Raccomandazione:
  - migrare ownership a `std::unique_ptr`/`QScopedPointer` dove possibile;
  - per oggetti `QObject`, assegnare parent esplicito o usare `deleteLater` in cleanup certo;
  - aggiungere audit mirato su contenitori di puntatori grezzi.
- Stato remediation:
  - implementato (parziale): `FoxVerifier` ora parented a `MainWindow`, cleanup dei verifier completati con `deleteLater()` + remove dalla lista;
  - implementato (parziale): hardening parsing OTP per evitare accessi out-of-range su payload malformati.
  - riferimenti: `widgets/mainwindow.cpp:7294-7374`, `:1687-1689`, `:7225-7262`.

### 2.5 Basso/Medio - Random Legacy (`qrand`) (CWE-338)

- File: `widgets/mainwindow.cpp`, `getfile.cpp`, `Modulator/Modulator.cpp`, `Transceiver/TCITransceiver.cpp`, `Network/PSKReporter.cpp`, `main.cpp`
- Evidenza (pre-fix): fallback `qrand()/qsrand()` presente in branch legacy.
- Impatto: random prevedibile e API deprecata.
- Raccomandazione: uniformare a `QRandomGenerator` (o `<random>`) su tutto il codice supportato.
- Stato remediation:
  - implementato: rimosse tutte le chiamate `qrand()/qsrand()` dal codice C++;
  - implementato: fallback legacy migrati a `std::rand()/std::srand()`, path Qt recenti invariati su `QRandomGenerator`.

## 3) Piano Intervento Raccomandato

### Fase 0 (Subito - blocco rischio)

- Rimuovere bypass TLS in `FoxVerifier`.
- Disabilitare ignore SSL globale in `NetworkAccessManager::createRequest`.
- Introdurre policy TLS strict-by-default con eventuali eccezioni scoped e tracciate.

### Fase 1 (1-2 sprint)

- Inserire guardie bounds pre-Fortran su path FT8/FT4/FT2/Q65 principali.
- Ridurre carico main-thread nelle routine `guiUpdate/decodeDone`.
- Aggiungere test regressione su networking TLS (cert valido/non valido, host mismatch).

### Fase 2 (hardening strutturale)

- Refactor progressivo ownership memory (raw pointer -> smart pointer/parent ownership esplicita).
- Prototipo backend IPC `mmap` su macOS con feature flag e benchmark.

## 4) Stato

Questo report e' basato su review statica e conferma che i rischi citati sono reali o plausibili con priorita' alta.
Per ridurre rischio operativo immediato, il fix TLS e' la prima azione consigliata.
