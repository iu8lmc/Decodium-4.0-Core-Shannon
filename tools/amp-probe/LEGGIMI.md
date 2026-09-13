# amp_probe — la telemetria dell'amplificatore è leggibile?

Nasce da una richiesta di **PA3GYQ**: far vedere al DECØMETER la potenza
all'uscita dell'amplificatore (SPE Expert 1.5K Taurus) invece di quella
dell'eccitatrice.

## Perché serve una prova, e non basta guardare il codice

Hamlib ha un'interfaccia dedicata agli amplificatori e definisce esattamente le
grandezze che al DECØMETER servono: potenza diretta, riflessa, di picco,
pilotante e ROS. Conosce anche gli SPE Expert, come modello **401**
(famiglia 1.3K-FA / 1.5K-FA / 2K-FA).

Ma quel backend **dichiara di non saper leggere nulla** — `has_get_level = 0x0`
— pur avendo la funzione di lettura implementata. È la stessa contraddizione
già vista con l'ALC di alcuni apparati: la maschera delle capacità dice una
cosa, l'implementazione ne dice un'altra.

Da fuori è impossibile stabilire chi abbia ragione. Solo l'apparato può dirlo.

## Uso

Su Windows: copiare `amp_probe.exe` nella cartella di installazione di
Decodium (serve `libhamlib-4.dll`, che sta lì) e lanciarlo da lì.

```
amp_probe.exe 401 COM7        SPE Expert 1.3K-FA / 1.5K-FA / 2K-FA
amp_probe.exe 201 COM7        Elecraft KPA1500
amp_probe.exe 301 COM7        Gemini DX1200 / HF-1K
amp_probe.exe 1   -           simulatore, per provare la sonda stessa
```

Su Linux: `gcc amp_probe.c -o amp_probe -lhamlib`, poi
`./amp_probe 401 /dev/ttyUSB0`.

⚠️ **Chiudere prima il software del costruttore.** Una porta seriale la apre un
solo programma alla volta: se il programma dell'amplificatore è in esecuzione,
la sonda troverà la porta occupata.

## Come si legge l'esito

La sonda interroga ogni grandezza **anche quando le capacità dichiarate dicono
di no** — è precisamente il punto della prova.

- **Almeno una risponde** → il DECØMETER può mostrare i dati
  dell'amplificatore, e il lavoro in Decodium è contenuto: c'è già tutto,
  scale, balistica e portate. Serve solo cambiare la sorgente.
- **Nessuna risponde** → la strada Hamlib non è praticabile e servirebbe
  implementare il protocollo del costruttore, che è tutt'altra impresa.

In entrambi i casi vale la pena inviare l'esito: un "no" documentato evita di
sprecare una giornata su una scommessa.
