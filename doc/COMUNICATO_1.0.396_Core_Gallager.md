# Comunicato — Decodium 4 diventa "Core Gallager"

**Release v1.0.396 · 14 giugno 2026**
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.396

---

## 🇮🇹 Italiano

### Decodium 4 presenta il **Core Gallager**: il decoder che scava i segnali deboli

Il cuore di **Decodium 4** ha un nuovo nome: **Core Gallager**, in onore di **Robert G. Gallager**, lo scienziato che nel 1960 inventò i **codici LDPC** — la stessa matematica su cui oggi si reggono FT8 e FT4. Un omaggio doveroto a chi ha reso possibile la decodifica dei segnali al limite del rumore.

Ma non è solo un cambio di nome. La 1.0.396 introduce il **decoder profondo weak-signal di Gallager**: un secondo passaggio di decodifica che, **parallelizzato sui core della CPU**, ripesca le stazioni più deboli — quelle vicine al fondo di rumore che la decodifica normale lascia indietro.

**Il risultato, misurato sul campo.** In test on-air a banda piena, su una sessione di circa 40 minuti, Decodium con il decoder Gallager attivo si è collocato **alla pari o oltre i migliori decoder di riferimento** sui segnali deboli, restando **dentro i tempi del ciclo** (il rilascio dei decodi non slitta) e **senza disturbare l'audio in ricezione**. Lo scavo profondo, prima troppo lento per stare nel tempo reale, oggi entra nel budget grazie alla parallelizzazione.

**Un pulsante, e basta.** Tutto è racchiuso nel nuovo pulsante **"GAL"**, accanto a Monitor. Un click e si accende; un altro e si spegne — **a caldo, senza riavviare**. È **disattivato di default**: chi vuole lo accende quando cerca i deboli. Un avviso nel tooltip ricorda che richiede una CPU multi-core: su PC datati conviene lasciarlo spento, e nulla cambia rispetto a prima.

**Libero e aperto, per la comunità.** Decodium 4 Core Gallager è software libero (licenza GPL v3, fork di WSJT-X di Joe Taylor K1JT), pensato per i radioamatori di tutto il mondo che lavorano in digitale sulle bande HF/VHF/UHF.

**Disponibile ora:** versione 1.0.396, installer per Windows x64.
⬇️ https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.396

*73 e buoni DX!*

---

## 🇬🇧 English

### Decodium 4 introduces **Core Gallager**: the decoder that digs out weak signals

The heart of **Decodium 4** has a new name: **Core Gallager**, honoring **Robert G. Gallager**, the scientist who invented **LDPC codes** in 1960 — the very mathematics behind today's FT8 and FT4. A fitting tribute to the man who made decoding signals at the edge of the noise possible.

It's more than a rename. Version 1.0.396 introduces the **Gallager deep weak-signal decoder**: a second decoding pass that, **parallelized across the CPU cores**, recovers the faintest stations — those near the noise floor that normal decoding leaves behind.

**The result, measured on air.** In full-band on-air tests over a ~40-minute session, Decodium with the Gallager decoder enabled placed **on par with or beyond the best reference decoders** on weak signals, while staying **within the cycle timing** (decode release doesn't slip) and **without disturbing receive audio**. The deep dig — once too slow for real time — now fits the budget thanks to parallelization.

**One button, that's it.** It's all wrapped in the new **"GAL"** button, next to Monitor. One click turns it on, another turns it off — **live, no restart**. It's **off by default**: switch it on when you're hunting the weak ones. A tooltip warns it needs a multi-core CPU: on older PCs it's best left off, and nothing changes from before.

**Free and open, for the community.** Decodium 4 Core Gallager is free software (GPL v3, a fork of Joe Taylor K1JT's WSJT-X), built for radio amateurs worldwide working digital modes on HF/VHF/UHF.

**Available now:** version 1.0.396, Windows x64 installer.
⬇️ https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.396

*73 and good DX!*
