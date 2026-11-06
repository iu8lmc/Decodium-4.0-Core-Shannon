# Decodium 4 — Alleggerimento per PC lenti/vecchi

Richiesta utenti: versione più leggera, modulare, adatta a hardware datato.
Analisi peso (2026-07-23): install 390 MB, ma il collo di bottiglia dei PC
vecchi è **runtime (GPU/CPU/RAM)**, non il disco. Tre leve indipendenti.

## Fase 1 — Taglio peso morto (✅ 1.0.497, per TUTTI)

~53 MB di codec immagine/video orfani rimossi dall'installer (verificato con
tabella import: nessun file li carica; imageformats = solo gif/ico/jpeg/svg):
`libx265, libaom, librsvg, libjxl, libvpx, libgdk_pixbuf`. Zero cambio
funzionale. Seconda passata futura: dipendenze secondarie di rsvg (cairo,
pango, libxml2) — attenzione a freetype/harfbuzz/fontconfig CONDIVISE con Qt.

## Fase 2 — Modalità PC lento (runtime) — la vera velocità su hw datato

Un unico interruttore `LowEndMode` che orchestra le leve esistenti:
- **Grafica**: forza `QSG_RHI_BACKEND=opengl` all'avvio (evita device-loss
  D3D12 su GPU vecchie — è il fix Danilo/Pasquale). Letto in main_qml PRIMA di
  QApplication, come UILanguage. WARP software = ultima spiaggia manuale.
- **CPU**: `lowCpuMode` ON, `ftThreads` cap basso (≤4), priorità processo
  Normale, ready-profile `cpu`.
- **UI**: Live Map e Full Spectrum chiusi di default (i maggiori consumatori
  GPU/RAM). Restano attivabili a mano.
- **Attivazione**: toggle in Impostazioni + offerta al primo avvio.

## Fase 3 — Installer a componenti (✅ 1.0.497)

`[Types]`: Completa / Leggera (PC lenti) / Personalizzata. `[Components]`:
`sounds` (suoni ~3,5 MB) e `langs` (12 lingue extra + qt_*.qm non-base, ~10 MB).
Inglese/italiano sempre installati. NB: il Setup.exe resta un superset (tutti i
file compressi dentro); la deselezione risparmia sul DISCO INSTALLATO, non sul
download (quello l'ha ridotto la Fase 1: Setup 77,6 → 67 MB). Live Map / FT2-Link
/ modi = compilati nell'exe, non componentizzabili (→ Modalità PC lento runtime).

### Note storiche

Inno Setup `[Types]` (Completa / Leggera / Personalizzata) + `[Components]`.
NB: FT2-Link e i modi sono COMPILATI nell'exe (90 MB) → non rimovibili come
file; per quelli la leva è la Fase 2 (nasconde/disattiva). I componenti
rimuovibili sono ASSET:
- Live Map + dati mondo (assets)
- Suoni di avviso (~4 MB)
- Lingue extra (tieni solo la tua; ~7 MB di .qm)
- (codec morti: già sempre esclusi in Fase 1)

Componente = mapping file→feature nella sezione `[Files]` dell'iss.
