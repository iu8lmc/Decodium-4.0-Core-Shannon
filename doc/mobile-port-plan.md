# Decodium 4 Mobile — Piano di sviluppo (Android + iOS, nativa completa)

*Bozza 2026-07-21 — architettura scelta: app nativa con decoder on-device, TX e CAT.*

## Obiettivo

Decodium 4 su Android (prima) e iPhone (poi, build sul Mac di Salvatore): decodifica
FT8/FT4/FT2 sul telefono, TX reale e CAT su tre canali — rete (rigctld/TCI),
USB-OTG (Android), Bluetooth.

## Perché è fattibile

- **Il core di decodifica è C++ puro**: il runtime Fortran non è più linkato
  (CMakeLists ~531: "resolves the migrated Fortran functionality through native
  C++"); i 197 `.f90` in-tree sono archivio dormiente. Compila con NDK/clang.
- **Dipendenze tutte portabili**: FFTW3 (single+threads), OpenMP (libomp NDK),
  Hamlib (supporto Android consolidato), libusb (Android via fd dal USB host
  Java). Unica frizione: **Boost.log** (compilabile per NDK, o da sostituire con
  un sink leggero su mobile).
- **Windows-only ben confinato**: 33 `#ifdef Q_OS_WIN/_WIN32` in DecodiumBridge
  (WASAPI park, priorità processo, ecc.); OmniRig e HRD sono COM → esclusi su
  mobile, il CAT passa da Hamlib (NET/seriale) e TCI (già nel codebase).
- **Kit Qt 6.6.3 android_arm64_v8a già installato** in C:\Qt (per la PoC; il
  target finale è Qt 6.11 for Android, stessa major del desktop).
- Prova d'esistenza sul mercato: FT8CN (Android, open source) fa FT8+CAT+audio
  USB con IC-705; SDR-Control (iOS) fa remote head. Decodium nativo può fare di più.

## Architettura mobile

```
┌─ App mobile (Android/iOS) ──────────────────────────────┐
│ UI QML mobile NUOVA (touch-first, portrait+landscape)   │
│   waterfall GPU · lista decode · pulsanti QSO · log     │
├─────────────────────────────────────────────────────────┤
│ DecodiumBridge-core (subset portabile del bridge)       │
│ decoder C++ FT8/FT4/FT2 · sequencer · logbook SQLite    │
├──────────────┬───────────────┬──────────────────────────┤
│ Audio        │ CAT           │ Rete                     │
│ mic/speaker  │ Hamlib NET    │ FT2-Link · hf-gateway    │
│ USB audio    │ TCI (nativo)  │ Cloudlog · community     │
│ (IC-705 &c.) │ USB-OTG CI-V  │                          │
│              │ BT SPP/BLE    │                          │
└──────────────┴───────────────┴──────────────────────────┘
```

**Refactor chiave**: estrarre da `DecodiumBridge.cpp` (~45k righe, desktop-centrico)
un **core portabile** (decode, sequencer, stato QSO, logbook, impostazioni) da cui
dipendono sia il desktop sia il mobile. Il desktop resta invariato; il mobile monta
una UI QML nuova sullo stesso core. Mai fork del core: una sola sorgente.

## Fasi

### P0 — Toolchain + proof-of-decode (1-2 settimane)
- [x] Kit Qt Android presente (6.6.3 arm64) · [ ] JDK 17 · [ ] Android SDK
  (platform-tools, android-34) · [ ] NDK r26+
- [ ] Cross-compilare per arm64: FFTW3, poi `ft2link_core` e la pipeline di
  decode FT8/FT2 (senza UI, senza Boost.log — stub del logger)
- [ ] **Gate di uscita**: eseguibile CLI Android che decodifica un WAV FT8 noto
  su un telefono reale (adb) con gli stessi risultati del desktop.

### P1 — Estrazione del core portabile (3-4 settimane)
- [ ] Separare DecodiumBridge in `decodium-core` (portabile) + `decodium-desktop`
  (WASAPI, priorità, OmniRig/HRD, popup) con CMake per target
- [ ] Astrazione logging (Boost.log su desktop, sink Android/os_log iOS su mobile)
- [ ] **Gate**: il desktop Windows compila e funziona IDENTICO (release normale
  dal core rifattorizzato — nessuna regressione per gli utenti attuali).

### P2 — App Android RX (4-6 settimane)
- [ ] UI QML mobile: waterfall (ripresa dal panadapter GPU), lista decode
  touch, selezione banda/modo, impostazioni essenziali
- [ ] Audio RX: microfono (accoppiamento acustico) + **USB audio class**
  (IC-705, Xiegu G90/X6100, digirig) via Qt Multimedia/Oboe
- [ ] Foreground service per RX continuo (restrizioni background Android)
- [ ] **Gate**: decodifica FT8 live su banda reale da IC-705 USB su telefono.

### P3 — TX + CAT Android (4-6 settimane)
- [ ] CAT rete: Hamlib NET (rigctld sul PC/raspberry) + **TCI** nativo
- [ ] CAT USB-OTG: CI-V/CAT seriale via USB host API (fd → libusb/Hamlib)
- [ ] CAT Bluetooth: SPP (adattatori seriali BT)
- [ ] TX audio: USB audio out + sequencer completo (riuso core: AutoCQ, cap
  retry/signoff, watchdog — già tutto nel core P1)
- [ ] **Gate**: QSO FT8 completo dal telefono con IC-705 (CAT+PTT+TX audio USB).

### P4 — iOS (3-4 settimane, col Mac di Salvatore)
- [ ] Build iOS del core + app QML (toolchain CMake iOS; niente USB/SPP:
  CAT solo rete/TCI/BLE)
- [ ] Audio: accoppiamento acustico + interfacce audio MFi/USB-C (iPhone 15+)
- [ ] Account Apple Developer (99$/anno) + TestFlight per i tester
- [ ] **Gate**: QSO FT8 da iPhone via rete (rigctld/TCI verso la stazione).

### P5 — FT2-Link mobile + rifiniture (2-3 settimane)
- [ ] FT2-Link (chat/BBS/file) su mobile — il core c'è già da P1
- [ ] hf-gateway come trasporto: QSO via internet dal telefono
- [ ] Notifiche (DX-watch, messaggi FT2-Link), tema, 14 lingue (i .ts sono già lì)
- [ ] Play Store (APK/AAB) + App Store review.

## Rischi principali

| Rischio | Mitigazione |
|---|---|
| Refactor del bridge rompe il desktop | P1 ha gate "desktop identico"; release desktop continuano dal main |
| Boost.log su NDK | stub/astrazione logging in P0-P1, non bloccante |
| Timing FT8 con audio Android (latenza) | Oboe/AAudio low-latency; il timing critico è lato TX, misurare in P3 |
| Restrizioni background (Android/iOS) | foreground service / audio session attiva; RX continuo solo a schermo acceso su iOS |
| USB audio+seriale simultanei su un solo OTG | radio con audio+CAT su un solo USB (IC-705, X6100) come target primario |
| App Store review (app "radio") | precedenti esistono (SDR-Control, FT8CN su Play) |

## Fuori scope (per ora)

MSK144/Q65/JT65 su mobile (arrivano gratis dal core, ma non validati), OmniRig/HRD
(Windows-only), stampa, DX cluster desktop-window, multi-monitor.
