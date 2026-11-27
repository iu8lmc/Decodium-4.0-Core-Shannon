# Decodium 4 FT2 1.0.1002

## English (UK)

This release improves the handling and presentation of transmission-audio errors reported after 1.0.1001.

- Fixed the generic warning dialogue layout: its OK button now has a dedicated footer, and the header contributes to the dialogue's calculated height. The button remains within the dialogue when a warning is displayed, including with longer messages.
- Transmission-audio warnings now show the selected output device, the specific message reported by the audio output component, and guidance to check the device and audio settings. For example, the component may report an output-open or output-write failure when that information is available.
- Prevented the generic `sound-output-error` playback-completion message from replacing the more specific audio warning. Other playback failures continue to produce their existing warning.
- Added Italian translations for the new warning title, introductory text and default-device label. Existing audio-component translations continue to apply.
- Updated the application version to 1.0.1002.

The reported device failure itself can have several causes; the message presents the detail provided by the audio backend and does not claim to identify a cause that it did not report. The popup was compared before and after in an isolated QML test, including a longer message, and the macOS application was built locally. An actual USB-device failure was not reproduced end-to-end locally.

## Italiano

Questa versione migliora la gestione e la presentazione degli errori audio in trasmissione segnalati dopo la 1.0.1001.

- Corretta la disposizione del popup generico di avviso: il pulsante OK dispone ora di un'area dedicata e l'intestazione contribuisce all'altezza calcolata della finestra. Il pulsante rimane all'interno del popup anche con messaggi più lunghi.
- Gli avvisi audio TX mostrano il dispositivo di uscita selezionato, il messaggio specifico segnalato dal componente audio e un'indicazione per controllare dispositivo e impostazioni audio. Per esempio, il componente può segnalare un errore di apertura o di scrittura quando tale dettaglio è disponibile.
- Evitato che il messaggio generico di fine riproduzione `sound-output-error` sostituisca l'avviso audio più specifico. Gli altri errori di riproduzione continuano a produrre l'avviso previsto.
- Aggiunte le traduzioni italiane del titolo, del testo introduttivo e dell'etichetta per il dispositivo predefinito. Restano valide le traduzioni esistenti del componente audio.
- Aggiornata la versione dell'applicazione alla 1.0.1002.

Il malfunzionamento del dispositivo può avere cause diverse: il messaggio presenta i dettagli forniti dal backend audio senza attribuire una causa non segnalata. Il popup è stato confrontato prima e dopo in un test QML isolato, anche con un messaggio lungo, e l'applicazione è stata compilata localmente su macOS. Un errore reale del dispositivo USB non è stato riprodotto end-to-end in locale.
