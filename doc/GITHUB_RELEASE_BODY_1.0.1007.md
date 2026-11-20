# Decodium 4 FT2 1.0.1007

## English (UK)

Final release on the iu8lmc fork: elisir80 1.0.1005 plus the work from the
Claude Code web sessions, plus the OpenSSL link fix for the MinGW build.

- FT8/FT4/FT2 decode speed-up: faster bit metrics and signal subtraction,
  with a comparison harness (`tests/tools/ftx_bitmetrics_compare.cpp`) that
  checks the output against the reference. The fork PR #114 reports about
  1.9× faster FT8 decoding; the -fno-trapping-math flag is applied to
  FtxBitmetrics.cpp on x86-64 only.
- Project permissions in `.claude/settings.json` (fork PR #88).
- Build fix: libcrypto is linked into decodium_qml for the SSTV encrypted
  sharing code.
- The installer includes the MinGW and third-party runtime DLLs
  (libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll, libgomp-1.dll,
  libcrypto-3-x64.dll, libfftw3f-3.dll, libhamlib-4.dll, librtlsdr.dll,
  libopenjp2-7.dll and the Boost runtime).
- The Hound / SuperFox RR73 issue is still open and is not fixed by this
  release.

## Italiano

Release finale sul fork iu8lmc: 1.0.1005 di elisir80, più il lavoro delle
sessioni Claude Code web, più la correzione di link OpenSSL per la build MinGW.

- Accelerazione della decodifica FT8/FT4/FT2: metriche dei bit e sottrazione del
  segnale più veloci, con un banco di confronto (`tests/tools/ftx_bitmetrics_compare.cpp`)
  che verifica l'output rispetto al riferimento. La PR #114 del fork riporta
  decodifica FT8 circa 1,9× più veloce; il flag -fno-trapping-math è applicato
  a FtxBitmetrics.cpp solo su x86-64.
- Permessi di progetto in `.claude/settings.json` (PR #88 del fork).
- Correzione di build: libcrypto è collegata a decodium_qml per la condivisione
  SSTV cifrata.
- L'installer include le DLL di runtime MinGW e delle librerie esterne
  (libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll, libgomp-1.dll,
  libcrypto-3-x64.dll, libfftw3f-3.dll, libhamlib-4.dll, librtlsdr.dll,
  libopenjp2-7.dll e il runtime Boost).
- Il problema Hound / SuperFox (RR73 mancante) resta aperto e questa release non
  lo risolve.
