# Note Telegram — Decodium 4 FT2 v1.0.393

Release: https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.393

---

## 🇮🇹 Italiano

📡 **Decodium 4 FT2 — v1.0.393**
⚡ Il TX FT2 cambia marcia: niente più QSO bloccati + risposta più rapida

🛠 **Cosa cambia**
• **FT2 — fine del "QSO bloccato"** — quando due stazioni finivano a trasmettere nello stesso periodo, nessuna sentiva l'altra e il QSO moriva in silenzio fino al watchdog. Ora la collisione viene rilevata e risolta da sola: una delle due si sfasa automaticamente e lo scambio riparte (validato con QSO completi in laboratorio, su 46 ore di log on-air era la prima causa di QSO persi).
• **FT2 — "narrow reply decode"** (opzionale, *Settings → FT2*) — mentre aspetti una risposta, la decodifica si concentra attorno alla tua frequenza (±150 Hz) con una passata su tutta la banda ogni 4 cicli: la risposta viene agganciata **prima, nello stesso slot** (−45% di latenza misurata) e la CPU ringrazia. La band activity continua ad aggiornarsi.
• **FT2 — partner deboli in QSB** — la cache AP (progetto −3 dB) ora lavora anche per il sequencer, non solo per il display: se il tuo corrispondente sparisce nel QSB a metà QSO, il decode "recuperato" dalla cache può far avanzare lo scambio invece di lasciarlo appeso.
• **FT2 — decodifiche lente sotto controllo** — un decode patologicamente lento (capitava fino a 9 secondi!) non blocca più la pipeline: oltre i 2.5s viene interrotto in modo pulito e si riparte con audio fresco.
• **AutoCQ — stop alle chiamate nel vuoto** — se l'audio RX risulta morto da 15+ secondi (cavo staccato, scheda in stallo), il CQ automatico si sospende da solo e riprende appena l'audio torna. Prima continuava a chiamare per minuti senza poter sentire nessuno.
• **Log diagnostici più puliti** — niente più migliaia di righe ripetute: ora coprono giornate intere e dicono *perché* il sequencer aspetta.

⬇️ Download:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.393

73! 🌍

---

## 🇬🇧 English

📡 **Decodium 4 FT2 — v1.0.393**
⚡ FT2 TX shifts gear: no more stuck QSOs + faster replies

🛠 **What changed**
• **FT2 — no more "stuck QSO"** — when two stations ended up transmitting in the same period, neither heard the other and the QSO silently died until the watchdog. The collision is now detected and resolved automatically: one station shifts phase and the exchange resumes (validated with complete lab QSOs; across 46 hours of on-air logs this was the #1 cause of lost QSOs).
• **FT2 — "narrow reply decode"** (optional, *Settings → FT2*) — while waiting for a reply, decoding focuses around your frequency (±150 Hz) with a full-band pass every 4th cycle: the reply is caught **earlier, in the same slot** (−45% measured latency) and your CPU breathes. Band activity keeps updating.
• **FT2 — weak partners in QSB** — the AP cache (the −3 dB project) now works for the sequencer, not just the display: if your partner fades into QSB mid-QSO, the cache-rescued decode can advance the exchange instead of leaving it hanging.
• **FT2 — slow decodes under control** — a pathologically slow decode (up to 9 seconds!) no longer stalls the pipeline: past 2.5s it's cleanly interrupted and decoding restarts on fresh audio.
• **AutoCQ — no more calling into the void** — if RX audio has been dead for 15+ seconds (unplugged cable, stalled soundcard), auto-CQ suspends itself and resumes as soon as audio returns. It used to keep calling for minutes while unable to hear anyone.
• **Cleaner diagnostic logs** — no more thousands of repeated lines: logs now span whole days and tell you *why* the sequencer is waiting.

⬇️ Download:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.393

73! 🌍

---

## 🇪🇸 Español

📡 **Decodium 4 FT2 — v1.0.393**
⚡ El TX FT2 cambia de marcha: no más QSO bloqueados + respuesta más rápida

🛠 **Qué cambia**
• **FT2 — fin del "QSO bloqueado"** — cuando dos estaciones acababan transmitiendo en el mismo período, ninguna oía a la otra y el QSO moría en silencio hasta el watchdog. Ahora la colisión se detecta y se resuelve sola: una de las dos se desfasa automáticamente y el intercambio continúa (validado con QSO completos en laboratorio; en 46 horas de logs en el aire era la primera causa de QSO perdidos).
• **FT2 — "narrow reply decode"** (opcional, *Ajustes → FT2*) — mientras esperas una respuesta, la decodificación se concentra alrededor de tu frecuencia (±150 Hz) con una pasada de banda completa cada 4 ciclos: la respuesta se engancha **antes, en el mismo slot** (−45% de latencia medida) y la CPU lo agradece. La actividad de banda sigue actualizándose.
• **FT2 — corresponsales débiles en QSB** — la caché AP (el proyecto −3 dB) ahora trabaja para el secuenciador, no solo para la pantalla: si tu corresponsal se desvanece en QSB a mitad del QSO, la decodificación "rescatada" por la caché puede hacer avanzar el intercambio en vez de dejarlo colgado.
• **FT2 — decodificaciones lentas bajo control** — una decodificación patológicamente lenta (¡hasta 9 segundos!) ya no bloquea la pipeline: pasados 2.5s se interrumpe limpiamente y se reinicia con audio fresco.
• **AutoCQ — no más llamadas al vacío** — si el audio RX lleva muerto 15+ segundos (cable desconectado, tarjeta colgada), el CQ automático se suspende solo y se reanuda en cuanto vuelve el audio. Antes seguía llamando durante minutos sin poder oír a nadie.
• **Logs de diagnóstico más limpios** — no más miles de líneas repetidas: ahora cubren días enteros y dicen *por qué* el secuenciador espera.

⬇️ Descarga:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.393

¡73! 🌍
