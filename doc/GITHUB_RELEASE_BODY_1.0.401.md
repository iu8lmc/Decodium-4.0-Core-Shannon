# Decodium 4 FT2 1.0.401 — Core Gallager

Changes from `1.0.400` to `1.0.401`.

## FORK-ONLY: Fix GAL (Core Gallager harvest subpass) — iu8lmc fork

This release fixes a regression introduced by Salvatore's serial-gate commit (1.0.399) that silently broke the **GAL (Core Gallager scavo deboli)** subpass in iu8lmc builds 1.0.399 and 1.0.400. The fix is fork-only and is NOT sent to elisir80 upstream.

## Italiano

### Fix critico: GAL (scavo Gallager) ora pienamente operativo

Il serial gate di Salvatore (`1.0.399`) scartava il decode subpass quando `serial != latest`, senza distinguere se si trattava del passaggio harvest Gallager.
Il risultato era che **GAL non produceva decode** nelle release 1.0.399 e 1.0.400 del fork iu8lmc.

**Fix (commit `2056924`):** aggiunto `&& !request.subpass` ai tre gate stale-drop in `FT8DecodeWorker.cpp`.
Il subpass harvest (subpass=1) e' ora esente dal serial-gate: completa sempre, indipendentemente dal contatore serial.

**Validato live sul commit 2056924:**
- subpass=1 fp=4, nout 2-6 per slot
- harvest Gallager: 0.72 decode/slot
- A/B vs JTDX: ~95% e in salita

### Allineamento a elisir80 1.0.400

Questa release mantiene tutto il contenuto di `1.0.400` di Salvatore:
- **FT8 weak repeated report recovery** (fast A7 replay anticipato, retry stretto su report diretti ripetuti)
- **FT8 trace** e **decoder serial gate** (modo-switch reset)
- Allineamento UI Linux/Win

Le feature fork-only iu8lmc sono interamente preservate:
- Core Gallager + pulsante GAL in toolbar (subpass harvest esente da serial-gate)
- DX-Pedition Mode completa
- ALC automatico (lettura + auto-cal gain TX)
- Tutti i toggle FT2 opt-in (ft2ConservativeTiming, ft8FastSequence, ftxImmediateClickTx, ecc.)
- Shortcut tastiera, scala UI, selezione banda, colori personalizzabili
- Profili Pronti FT2 (balanced/weak/contest/cpu)

### Versione e release

- `fork_release_version.txt` aggiornato a `1.0.401`
- Tag operativo: `v1.0.401`
- Asset Windows: `Decodium_1.0.401_Setup_x64.exe` (build MSYS2 MINGW64 locale, iu8lmc)

### Verifica runtime

- Build: MSYS2 MINGW64, mingw32-make decodium_app
- Versione exe: `Decodium 1.0.401` confermata
- App: avvio stabile, splash "Core Gallager" presente, pulsante GAL in toolbar
- Installer: `Decodium_1.0.401_Setup_x64.exe` 91.32 MB
- SHA256: `FAB7A001CAF197E72C582D3B4505789B6D2A9DCF547BEC71E87FE108D0ED9D74`

---

## English

### Critical fix: GAL (Gallager harvest subpass) now fully operational

Salvatore's serial-gate introduced in `1.0.399` discarded the harvest subpass whenever `serial != latest`, without distinguishing the Gallager harvest pass.
As a result, **GAL produced no decodes** in iu8lmc builds 1.0.399 and 1.0.400.

**Fix (commit `2056924`):** added `&& !request.subpass` to the three stale-drop gates in `FT8DecodeWorker.cpp`.
The harvest subpass (subpass=1) is now exempt from the serial-gate and always completes, regardless of the serial counter.

**Validated live on commit 2056924:**
- subpass=1 fp=4, nout 2-6 per slot
- Gallager harvest rate: 0.72 decodes/slot
- A/B vs JTDX: ~95% and rising

### Alignment with elisir80 1.0.400

This release retains all content from Salvatore's `1.0.400`:
- FT8 weak repeated report recovery (early fast A7 replay, narrow retry on repeated directed reports)
- FT8 trace and decoder serial gate (mode-switch reset)
- Linux/Win UI alignment

All iu8lmc fork-only features are fully preserved:
- Core Gallager + GAL toolbar button (harvest subpass exempt from serial-gate)
- DX-Pedition Mode (full)
- ALC auto-calibration (read + TX gain loop)
- All FT2 opt-in toggles
- Keyboard shortcuts, UI scaling, band selector, custom colors
- FT2 Ready Profiles (balanced/weak/contest/cpu)

### Version and release

- `fork_release_version.txt` updated to `1.0.401`
- Operational tag: `v1.0.401`
- Windows asset: `Decodium_1.0.401_Setup_x64.exe` (MSYS2 MINGW64 local build, iu8lmc)

### Runtime verification

- Build: MSYS2 MINGW64, mingw32-make decodium_app
- Exe version: `Decodium 1.0.401` confirmed
- App: stable launch, "Core Gallager" splash present, GAL button in toolbar
- Installer: `Decodium_1.0.401_Setup_x64.exe` 91.32 MB
- SHA256: `FAB7A001CAF197E72C582D3B4505789B6D2A9DCF547BEC71E87FE108D0ED9D74`
