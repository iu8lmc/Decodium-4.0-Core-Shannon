# Note Telegram — Decodium 4 v1.0.431

Release: https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.431

---

## 🇮🇹 Italiano

📡 **Decodium 4 — v1.0.431**
🛠 Fix: "Caller retries" non interrompe più i QSO in chiusura (FT8/FT4)

🛠 **Cosa cambia**
• **FT8/FT4 — niente più QSO troncati a fine collegamento** — con un valore di **"Caller retries" basso** (es. 1) poteva capitare che l'**RR73 del partner venisse ignorato** e il QSO non si chiudesse né venisse loggato. Ora il limite "Caller retries" vale **solo per la fase di chiamata** (quando chiami qualcuno che non risponde): un QSO **già avviato viene lasciato completare e loggare**. Solo logica di sequenza TX: **il decoder è invariato**.

⬇️ Download:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.431

73! 🌍

---

## 🇬🇧 English

📡 **Decodium 4 — v1.0.431**
🛠 Fix: "Caller retries" no longer interrupts a QSO that's closing (FT8/FT4)

🛠 **What changed**
• **FT8/FT4 — no more QSOs cut off at the end** — with a **low "Caller retries"** value (e.g. 1), an incoming **RR73 could be ignored** and the QSO wouldn't close or get logged. Now the "Caller retries" limit only applies to the **calling phase** (when you call someone who doesn't answer): a QSO **already in progress is allowed to complete and log**. TX sequencing logic only: **the decoder is unchanged**.

⬇️ Download:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.431

73! 🌍

---

## 🇫🇷 Français

📡 **Decodium 4 — v1.0.431**
🛠 Correctif : « Caller retries » n'interrompt plus un QSO en cours de clôture (FT8/FT4)

🛠 **Ce qui change**
• **FT8/FT4 — fini les QSO coupés à la fin** — avec une valeur **« Caller retries » faible** (ex. 1), un **RR73 reçu pouvait être ignoré** et le QSO ne se terminait pas et n'était pas journalisé. Désormais la limite « Caller retries » ne s'applique qu'à la **phase d'appel** (quand vous appelez quelqu'un qui ne répond pas) : un QSO **déjà engagé est laissé se terminer et se journaliser**. Logique de séquence TX uniquement : **le décodeur n'est pas touché**.

⬇️ Téléchargement :
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.431

73 ! 🌍

---

## 🇪🇸 Español

📡 **Decodium 4 — v1.0.431**
🛠 Corrección: "Caller retries" ya no interrumpe un QSO en cierre (FT8/FT4)

🛠 **Qué cambia**
• **FT8/FT4 — se acabaron los QSO cortados al final** — con un valor de **"Caller retries" bajo** (p. ej. 1), un **RR73 recibido podía ignorarse** y el QSO no se cerraba ni se registraba. Ahora el límite "Caller retries" solo se aplica a la **fase de llamada** (cuando llamas a alguien que no responde): un QSO **ya iniciado se deja completar y registrar**. Solo lógica de secuencia TX: **el decodificador no se toca**.

⬇️ Descarga:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.431

73! 🌍

---

## 🇯🇵 日本語

📡 **Decodium 4 — v1.0.431**
🛠 修正：「Caller retries」が終了処理中の QSO を中断しなくなりました（FT8/FT4）

🛠 **変更点**
• **FT8/FT4 — 交信の最後で QSO が切れる問題を解消** —「**Caller retries**」の値が**小さい**場合（例：1）、受信した **RR73 が無視**され、QSO が完了せずログにも残らないことがありました。今後「Caller retries」の上限は**呼び出しフェーズのみ**（応答のない相手を呼んでいるとき）に適用され、**すでに始まっている QSO は最後まで完了してログに記録**されます。TX シーケンスのロジックのみ：**デコーダーは変更していません**。

⬇️ ダウンロード：
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.431

73! 🌍

---

## 🇷🇺 Русский

📡 **Decodium 4 — v1.0.431**
🛠 Исправление: «Caller retries» больше не прерывает QSO на этапе завершения (FT8/FT4)

🛠 **Что изменилось**
• **FT8/FT4 — больше никаких QSO, оборванных в конце** — при **малом значении «Caller retries»** (например, 1) входящий **RR73 мог игнорироваться**, и QSO не завершалось и не записывалось в лог. Теперь лимит «Caller retries» применяется только к **фазе вызова** (когда вы вызываете того, кто не отвечает): уже **начатое QSO доводится до конца и записывается**. Только логика последовательности TX: **декодер не затронут**.

⬇️ Скачать:
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.431

73! 🌍

---

## 🇨🇳 中文

📡 **Decodium 4 — v1.0.431**
🛠 修复：「Caller retries」不再中断正在收尾的 QSO（FT8/FT4）

🛠 **更新内容**
• **FT8/FT4 — 不再有在结尾被切断的 QSO** — 当「**Caller retries**」设置较**低**（例如 1）时，收到的 **RR73 可能被忽略**，导致 QSO 无法收尾，也不会记入日志。现在「Caller retries」的上限**只作用于呼叫阶段**（当你呼叫一个不回应的电台时）：**已经开始的 QSO 会被允许完成并记入日志**。仅涉及 TX 时序逻辑：**解码器未改动**。

⬇️ 下载：
https://github.com/iu8lmc/Decodium-4.0-Core-Shannon/releases/tag/v1.0.431

73! 🌍
