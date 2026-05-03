# Decodium 4 FT2 1.0.71

Release 1.0.71 raccoglie i fix successivi alla 1.0.70 su waterfall, GPU/panadapter, PSK Reporter, CAT/Hamlib, modalita Hound e persistenza impostazioni.

## Cambiamenti principali

- Stabilizzato il waterfall della webapp: canvas a dimensione fissa, righe ricampionate e nessun reset quando cambia la sorgente o la larghezza del frame.
- Rafforzato il path GPU del panadapter/waterfall su Metal, Direct3D11 e OpenGL, con fallback CPU/texture robusto e diagnostica `[GPUDBG]`.
- Migliorata la FFT visuale del panadapter con workspace thread-local, finestra precomputata e scheduling asincrono.
- Corrette persistenza e comportamento di opzioni Settings/side menu, PSK Reporter TCP/IP, Decode Depth, Single Decode e Wait & Pounce.
- Sistemati PSK Reporter e invio spot RX, riducendo il rumore diagnostico dopo il debug.
- Rafforzata la gestione CAT/Hamlib multipiattaforma: DTR/RTS, VOX/no PTT, Fake-It Hamlib, log tentativi falliti e compatibilita HRD.
- Sistemati Hound mode, badge HOUND, generazione messaggi e prevenzione dei TX brevi con bad message.
- Correzioni UI su log, QSO panel, PSK Reporter input, HOLD, scorrimento bande e layout dei controlli.

## Asset previsti

- `Decodium_1.0.71_Setup_x64.exe`
- `decodium4-ft2-1.0.71-macos-tahoe-arm64.dmg`
- `decodium4-ft2-1.0.71-macos-tahoe-arm64.zip`
- `decodium4-ft2-1.0.71-macos-tahoe-arm64-sha256.txt`
- `decodium4-ft2-1.0.71-macos-sequoia-arm64.dmg`
- `decodium4-ft2-1.0.71-macos-sequoia-arm64.zip`
- `decodium4-ft2-1.0.71-macos-sequoia-arm64-sha256.txt`
