# Decodium 4 v1.0.683

Hotfix for a crash in remote-radio mode.

## Fixed

- Decodium could crash (access violation inside Qt6Multimedia) right after switching the receive audio to the remote radio, when the local speaker used for listening could not be opened. The audio output now releases its failed stream after the Qt signal returns, instead of while Qt is still using it, and reports a plain "audio output could not start" message.

## Italiano

Hotfix per un crash in modalità radio remota.

### Corretto

- Decodium poteva chiudersi (violazione di accesso in Qt6Multimedia) subito dopo il passaggio dell'audio ricevuto alla radio remota, quando l'altoparlante usato per l'ascolto non poteva essere aperto. Ora l'uscita audio rilascia il flusso fallito dopo che il segnale di Qt è tornato, e non mentre Qt lo sta ancora usando, e mostra un messaggio semplice "uscita audio non avviata".

## Downloads

The Windows x64 installer EXE is attached to this release.
