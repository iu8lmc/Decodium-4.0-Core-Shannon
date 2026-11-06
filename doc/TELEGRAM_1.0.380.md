# Note Telegram — Decodium 4 FT2 v1.0.380

Release: https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.380

---

## 🇮🇹 Italiano

📡 **Decodium 4 FT2 — v1.0.380**
🔁 Fix FT8/FT4: ritrasmette se il partner non risponde

🛠 **Cosa cambia**
• Se chiami una stazione e il corrispondente **non risponde**, ora Decodium **ritrasmette** la chiamata al periodo successivo (fino al limite di tentativi), invece di fare una sola trasmissione e fermarsi.
• Quando il partner risponde, il QSO procede regolarmente come prima.
• Vale per FT8 e FT4 (in FT2 il problema non c'era).

Era una regressione introdotta dall'irrobustimento del sequencer nella 1.0.375: in un QSO attivo il programma aspettava la risposta del corrispondente ma, se non arrivava, saltava la ritrasmissione. Ora aspetta comunque (per non sovrascrivere una risposta in arrivo) ma, se il partner tace, riprova.

⬇️ Download:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.380

73! 🌍

---

## 🇬🇧 English

📡 **Decodium 4 FT2 — v1.0.380**
🔁 FT8/FT4 fix: re-transmits when the partner doesn't reply

🛠 **What changed**
• If you call a station and the other operator **doesn't reply**, Decodium now **re-transmits** the call on the next period (up to the retry limit), instead of sending once and stopping.
• When the partner replies, the QSO proceeds normally as before.
• Applies to FT8 and FT4 (FT2 was not affected).

It was a regression from the sequencer hardening in 1.0.375: in an active QSO the app waited for the partner's reply but, if none came, it skipped the re-transmission. Now it still waits (so it won't overwrite an incoming reply) but, if the partner stays silent, it retries.

⬇️ Download:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.380

73! 🌍

---

## 🇪🇸 Español

📡 **Decodium 4 FT2 — v1.0.380**
🔁 Corrección FT8/FT4: retransmite si el corresponsal no responde

🛠 **Qué cambia**
• Si llamas a una estación y el otro operador **no responde**, ahora Decodium **retransmite** la llamada en el periodo siguiente (hasta el límite de reintentos), en vez de transmitir una sola vez y detenerse.
• Cuando el corresponsal responde, el QSO continúa con normalidad como antes.
• Vale para FT8 y FT4 (en FT2 no ocurría).

Era una regresión del endurecimiento del secuenciador en la 1.0.375: en un QSO activo el programa esperaba la respuesta del corresponsal pero, si no llegaba, se saltaba la retransmisión. Ahora sigue esperando (para no sobrescribir una respuesta entrante) pero, si el corresponsal calla, reintenta.

⬇️ Descarga:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.380

¡73! 🌍
