# Decodium 4 FT2 1.0.1015

## English (UK)

Release 1.0.1015 is the iu8lmc fork build of 1.0.1014 plus a faster LDPC
decoder for older PCs.

- **Older CPUs (without AVX2):** the LDPC decoder now chooses, in this order,
  superldpc with AVX2/FMA, then **superldpc with SSE2**, then the original
  generic decoder. Until now every x86-64 CPU without AVX2/FMA, or whose
  operating system does not save the AVX state (Sandy Bridge, Ivy Bridge,
  older AMD processors, some virtual machines), went straight to the generic
  decoder. The SSE2 version runs the same algorithm and gives the same results
  bit for bit as the AVX2 version.
- **Measured speed** (on a Ryzen 9 5950X, forcing each level): superldpc SSE2
  takes 34–50 µs per codeword against 32–48 µs for AVX2 and 425–6,200 µs for
  the generic decoder. The FT8 weak-signal test decodes in 1.2 s with SSE2, as
  with AVX2, against 15–30 s with the generic decoder. FT2 on an emulated Ivy
  Bridge CPU now uses the SSE2 decoder: 430 ms instead of 870 ms.
- FT2 on a CPU without AVX2 keeps its 2-second safety limit per decode.
- An AVX-only (no AVX2) variant was measured and not included: it was no
  faster than SSE2.
- The diagnostic log shows the decoder in use, for example
  `decoder=superldpc-sse2 reason="CPU has no AVX2"`.
- Not yet verified on a real CPU without AVX2: the older-CPU paths were tested
  on a modern CPU by limiting the decoder level and by emulating the CPU
  features. Reports from users with older PCs are welcome.
- Everything in 1.0.1014 is included unchanged. The previously reported
  Hound/SuperFox RR73 issue remains open.

## Italiano

La release 1.0.1015 è la build del fork iu8lmc della 1.0.1014 con un decoder
LDPC più veloce per i PC meno recenti.

- **CPU meno recenti (senza AVX2):** il decoder LDPC ora sceglie, in
  quest'ordine, superldpc con AVX2/FMA, poi **superldpc con SSE2**, poi il
  decoder generico originale. Finora ogni CPU x86-64 senza AVX2/FMA, o il cui
  sistema operativo non salva lo stato AVX (Sandy Bridge, Ivy Bridge,
  processori AMD meno recenti, alcune macchine virtuali), passava direttamente
  al decoder generico. La versione SSE2 esegue lo stesso algoritmo e dà gli
  stessi risultati, bit per bit, della versione AVX2.
- **Velocità misurata** (su un Ryzen 9 5950X, forzando ogni livello):
  superldpc SSE2 impiega 34–50 µs per parola di codice contro 32–48 µs
  dell'AVX2 e 425–6.200 µs del decoder generico. Il test FT8 a segnale debole
  decodifica in 1,2 s con SSE2, come con AVX2, contro 15–30 s del decoder
  generico. FT2 su una CPU Ivy Bridge emulata ora usa il decoder SSE2: 430 ms
  invece di 870 ms.
- FT2 su una CPU senza AVX2 mantiene il suo limite di sicurezza di 2 secondi
  per decodifica.
- Una variante solo AVX (senza AVX2) è stata misurata e non inclusa: non era
  più veloce della SSE2.
- Il log diagnostico indica il decoder in uso, per esempio
  `decoder=superldpc-sse2 reason="CPU has no AVX2"`.
- Non ancora verificato su una CPU reale senza AVX2: i percorsi per le CPU
  meno recenti sono stati provati su una CPU moderna limitando il livello del
  decoder ed emulando le caratteristiche della CPU. Le segnalazioni di chi ha
  PC meno recenti sono benvenute.
- Tutto ciò che c'è nella 1.0.1014 è incluso senza modifiche. Il problema
  Hound/SuperFox RR73 precedentemente segnalato rimane aperto.
