# Decodium 4 FT2 v1.0.610

Version 1.0.610 combines the upstream 1.0.609 FT2 predictive-decoding work
with a cross-platform Live Map greyline correction and compatibility hardening.

## English (UK)

### Upstream FT2 predictive decoding retained

The FT2 type-8 predictive decoder introduced in 1.0.609 is included unchanged:
when a station repeats a previously heard 77-bit message, Decodium can verify
the known codeword directly instead of repeating the full LDPC search. The
feature remains off by default and can be enabled with
`DECODIUM_FT2_AP_MSG=1`. FT8 and FT2 keep separate prediction archives, and
the frequency-drift safeguards and false-acceptance checks from 1.0.609 remain
in place.

### Live Map greyline on Linux and legacy profiles

The Live Map greyline toggle now works consistently across the Qt Quick GPU
and painter paths. Older configuration profiles may store boolean settings as
strings; those values are now converted strictly, so a stored `"false"` no
longer behaves as `true`. The `ShowGreyline` and legacy `MapShowGreyline` keys
are kept in sync, including when settings are changed from another page.

On Linux/OpenGL, the greyline shader is enabled by default instead of being
silently suppressed by the conservative renderer policy. An explicit
`DECODIUM_DISABLE_OPENGL_LIVEMAP_GREYLINE=1` override is available for a
driver with a known shader problem, while
`DECODIUM_ENABLE_OPENGL_LIVEMAP_GREYLINE=1` is retained for diagnostics.
Custom builds without the Qt ShaderTools resource now expose the missing
capability and fall back to the CPU painter map instead of showing a map with
no greyline and no explanation.

### Build and runtime compatibility

The changes are confined to the existing Live Map settings and renderer
paths. No new database or external runtime is required. The release is built
by the existing Windows x64, macOS Apple Silicon, macOS Intel, Linux x86_64,
and Linux aarch64 GitHub Actions workflows.

## Italiano

### Mantenuta la decodifica predittiva FT2

La decodifica predittiva di tipo 8 per FT2 introdotta nella 1.0.609 e' inclusa
senza modifiche: quando una stazione ripete un messaggio gia' ascoltato da 77
bit, Decodium puo' verificare direttamente la parola di codice conosciuta
senza ripetere tutta la ricerca LDPC. La funzione resta disattivata per
default e si abilita con `DECODIUM_FT2_AP_MSG=1`. Gli archivi predittivi di FT8
e FT2 restano separati e sono conservate le protezioni contro la deriva di
frequenza e gli accettamenti falsi della 1.0.609.

### Greyline della Live Map su Linux e profili legacy

Il selettore greyline della Live Map ora funziona in modo coerente sia nel
percorso GPU Qt Quick sia nel percorso painter. I profili piu' vecchi possono
salvare i booleani come stringhe; ora questi valori vengono convertiti in modo
rigoroso, quindi un `"false"` salvato non viene piu' interpretato come `true`.
Le chiavi `ShowGreyline` e la precedente `MapShowGreyline` restano sincronizzate,
anche quando l'impostazione viene modificata da un'altra pagina.

Su Linux/OpenGL lo shader greyline viene abilitato per default invece di essere
soppresso in silenzio dalla politica conservativa del renderer. Per driver con
problemi noti e' disponibile l'override esplicito
`DECODIUM_DISABLE_OPENGL_LIVEMAP_GREYLINE=1`; resta disponibile per la
diagnostica anche `DECODIUM_ENABLE_OPENGL_LIVEMAP_GREYLINE=1`. Nei build
personalizzati privi della risorsa Qt ShaderTools, la capacita' mancante viene
ora rilevata e la mappa passa al painter CPU, invece di mostrare una mappa
senza greyline senza spiegazione.

### Compatibilita' di build e runtime

Le modifiche restano nei percorsi esistenti delle impostazioni e del renderer
della Live Map. Non viene introdotto alcun nuovo database o runtime esterno.
La release viene prodotta dagli attuali workflow GitHub Actions per Windows
x64, macOS Apple Silicon, macOS Intel, Linux x86_64 e Linux aarch64.
