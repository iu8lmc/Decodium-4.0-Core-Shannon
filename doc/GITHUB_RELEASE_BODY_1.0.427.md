## Decodium 4 FT2 1.0.427

Questa release contiene un fix mirato rispetto alla 1.0.426 per rendere piu puntuale la consegna dei decode FT4 sui computer piu lenti.

### FT4 latency guard

- Aggiunto un profilo automatico `FT4 latency guard` attivato quando il runtime misura callback FT4 lente.
- Se un decode FT4 supera `2500 ms`, Decodium alleggerisce temporaneamente il profilo FT4 per 2 minuti.
- Durante il guard, il pre-decode FT4 viene anticipato da circa `6250 ms` a circa `5200 ms` nello slot.
- Durante il guard, FT4 viene limitato a massimo `depth 2` e massimo `4` thread, riducendo il carico sui PC che non riescono a consegnare in tempo.
- Se un pre-decode FT4 e gia pendente, il pass finale viene saltato per evitare doppio lavoro e ritardi a catena.

### Protezioni runtime

- I PC veloci continuano a usare il profilo normale: il guard si attiva solo dopo una lentezza misurata dal runtime.
- Le protezioni Low CPU, CPU pressure e adaptive CPU restano attive.
- I single-core e i computer molto limitati non ricevono carico aggiuntivo: il fix riduce il lavoro quando la macchina dimostra di essere in ritardo.
- Le callback FT4 realmente troppo tardive continuano a essere gestite dal limite adattivo gia presente.

### Diagnostica

- Aggiunto log `FT4 latency guard active` con durata residua e motivo di attivazione.
- Aggiunto log quando il pass finale FT4 viene saltato perche il guard ha gia un pre-decode pendente.
- La metrica `[DECODEMETRIC]` resta utilizzabile per verificare `decode_ms`, `threads_active`, `depth` e `audio`.

### Versione e packaging

- Versione locale aggiornata a `1.0.427` tramite `fork_release_version.txt`.
- Aggiornati i riferimenti installer Windows Inno Setup e NSIS a `1.0.427`.

### Asset previsti

- Codice sorgente GitHub per `v1.0.427`.
- Installer Windows x64.
- Pacchetti macOS Apple Silicon.
- Pacchetti macOS Intel.
- AppImage Linux x86_64.
- AppImage Linux aarch64.
