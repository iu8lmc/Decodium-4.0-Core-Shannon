## English

This maintenance release fixes the Windows installer failing to start because `libcrypto-3-x64.dll` was excluded even though Decodium requires it. Install version 1.0.734 to replace the affected installer.

- Restores the OpenSSL crypto runtime and the libraries required by the TIFF and WebP image plugins, including their dependent libraries.
- Includes the Qt test libraries required by the Qt test QML modules already shipped in the installer.
- Adds a Windows release check that silently installs the generated installer, checks executable, plugin and QML-module DLL imports against the installed files and Windows system libraries, and starts the installed executable with a restricted PATH.
- Includes the build and packaging fixes made on main since v1.0.733 for Windows OpenSSL, macOS OpenSSL architecture verification, Linux CI headers and the SSTV-disabled diagnostics build.

The source archives are provided by GitHub; platform binaries are attached as their release builds complete. Radio hardware and third-party interoperability are not implied by these packaging checks.

## Italiano

Questa versione di manutenzione corregge il mancato avvio dell'installer Windows: `libcrypto-3-x64.dll` era esclusa nonostante Decodium la richiedesse. Installare la versione 1.0.734 per sostituire l'installer interessato.

- Ripristina la libreria OpenSSL crypto e le librerie richieste dai plugin per le immagini TIFF e WebP, incluse le loro dipendenze.
- Include le librerie Qt di test necessarie ai moduli QML di test già presenti nell'installer.
- Aggiunge alla release Windows una verifica che installa silenziosamente l'installer generato, controlla gli import DLL di eseguibili, plugin e moduli QML rispetto ai file installati e alle librerie di sistema Windows, e avvia l'eseguibile installato con un PATH limitato.
- Include le correzioni di build e pacchettizzazione apportate a main dopo la v1.0.733 per OpenSSL su Windows, la verifica dell'architettura OpenSSL su macOS, gli header della CI Linux e la build diagnostica con SSTV disabilitato.

Gli archivi del codice sono forniti da GitHub; i binari delle piattaforme vengono allegati al completamento delle rispettive build. Questi controlli di pacchettizzazione non dimostrano il funzionamento con apparati radio o l'interoperabilità con programmi di terze parti.
