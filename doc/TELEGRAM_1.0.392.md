# Note Telegram — Decodium 4 FT2 v1.0.392

Release: https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.392

---

## 🇮🇹 Italiano

📡 **Decodium 4 FT2 — v1.0.392**
🔧 Fix da audit interno: FT8 partner deboli + controllo remoto + CAT

🛠 **Cosa cambia**
• **FT8 — partner deboli** — il decode profondo (depth 3-4 + AP) fuori trasmissione torna ad alimentare l'auto-sequencer: le risposte decodificabili solo dal pass profondo non venivano più ingaggiate e il QSO restava appeso (regressione 1.0.389). Ora il QSO col partner debole si chiude di nuovo.
• **Controllo remoto — risposta al chiamante** — rispondendo a un chiamante dalla pagina web in FT8/FT4 ora il periodo TX (pari/dispari) viene impostato dal decode, come col doppio clic: prima ~metà delle risposte trasmetteva NEL periodo del chiamante e il QSO non partiva mai.
• **Controllo remoto — sicurezza** — il comando "rispondi al chiamante" è rifiutato nei modi beacon/test (WSPR, FST4W, Echo, FreqCal) e il nominativo viene validato: niente più TX involontari da comandi malformati.
• **CAT/HRD** — al timeout di connessione il collegamento viene chiuso in modo pulito: il tentativo successivo non trova più la porta/sessione occupata.

⬇️ Download:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.392

73! 🌍

---

## 🇬🇧 English

📡 **Decodium 4 FT2 — v1.0.392**
🔧 Internal audit fixes: FT8 weak partners + remote control + CAT

🛠 **What changed**
• **FT8 — weak partners** — the deep decode pass (depth 3-4 + AP) outside transmission feeds the auto-sequencer again: replies only decodable by the deep pass were no longer engaged and the QSO hung (1.0.389 regression). QSOs with weak partners complete again.
• **Remote control — answer caller** — answering a caller from the web page in FT8/FT4 now sets the TX period (even/odd) from the decode, just like double-click: previously ~half of the replies transmitted IN the caller's period and the QSO never started.
• **Remote control — safety** — the "answer caller" command is rejected in beacon/test modes (WSPR, FST4W, Echo, FreqCal) and the callsign is validated: no more unintended TX from malformed commands.
• **CAT/HRD** — on connect timeout the link is now closed cleanly: the next attempt no longer finds the port/session busy.

⬇️ Download:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.392

73! 🌍

---

## 🇪🇸 Español

📡 **Decodium 4 FT2 — v1.0.392**
🔧 Correcciones de auditoría interna: FT8 corresponsales débiles + control remoto + CAT

🛠 **Qué cambia**
• **FT8 — corresponsales débiles** — la decodificación profunda (depth 3-4 + AP) fuera de transmisión vuelve a alimentar el auto-secuenciador: las respuestas decodificables solo por el pase profundo ya no se enganchaban y el QSO quedaba colgado (regresión 1.0.389). Los QSO con corresponsales débiles se completan de nuevo.
• **Control remoto — responder al llamador** — al responder a un llamador desde la página web en FT8/FT4 ahora el período TX (par/impar) se ajusta desde la decodificación, como con el doble clic: antes ~la mitad de las respuestas transmitía EN el período del llamador y el QSO nunca arrancaba.
• **Control remoto — seguridad** — el comando "responder al llamador" se rechaza en modos baliza/test (WSPR, FST4W, Echo, FreqCal) y el indicativo se valida: no más TX involuntarios por comandos malformados.
• **CAT/HRD** — en el timeout de conexión el enlace se cierra limpiamente: el siguiente intento ya no encuentra el puerto/sesión ocupado.

⬇️ Descarga:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.392

¡73! 🌍
