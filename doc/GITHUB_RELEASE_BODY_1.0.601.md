# Decodium 4 FT2 v1.0.601

Version 1.0.601 brings RTTY into Decodium as a first-class mode. The DecoRTTY
project — its Baudot decoder, its Viterbi correction, its macro keyboard and its
QSO panel — now runs inside the application, on the radio Decodium already
controls, writing into the log Decodium already keeps.

## English (UK)

### RTTY is a mode, not a separate program

- **RTTY appears in the mode selector**, after WSPR. Choosing it switches the
  radio into the data mode you have configured (DATA-U as a rule — not one
  chosen for you), moves it to the RTTY segment of the band it is already on,
  and opens the RTTY window.
- Selecting RTTY **stops the digital-mode decoders**. This costs no new code:
  RTTY is not a slot-timed mode, so the periodic decode dispatch returns
  immediately on its own.
- Going back to FT8 does **not** close the RTTY window. Closing a window under
  the operator's hands while they may be reading a contact is worse than
  leaving it open.

### The network transport is gone

DecoRTTY shipped with its own way of reaching a radio: VITA-49 to a FlexRadio,
or a gateway for an FT-991A on another PC. Inside Decodium that search could
never succeed — the radio is one radio, on one serial port, already held by the
application's CAT. Asking the network for it was putting the question to someone
who could not answer.

- **5475 lines removed, 24 files deleted**: the FlexRadio transport, VITA-49
  packets, the SmartSDR API client, network discovery, Opus, the FT-991A gateway
  and its serial CAT.
- In their place, a link that presents Decodium's own radio to the RTTY engine.
  Frequency, mode and PTT come from the application's CAT through hooks — the
  same arrangement DecoPort uses, which keeps `src/rtty` compilable on its own.
- The sound-card source went too. Whoever decides where audio and commands come
  from is Decodium, in one place, and RTTY has nothing left to connect.

### Audio, transmit and the waterfall

- Receive audio is the same 12 kHz stream Decodium decodes, tapped where DecoPort
  taps it. It is resampled to the 24 kHz the RTTY engine expects: at 12 kHz the
  engine's internal decimation would have pushed the 2125 Hz mark tone above
  Nyquist — not "slightly worse", unrecognisable.
- **Transmit works**, through the same output Decodium opens for DecoPort audio:
  one path to the radio, with the interlocks already written. Nothing is played
  while the digital-mode sequencer is transmitting or tuning.
- The **TRANSMIT button now sends what you have typed**. It used to raise the
  carrier only, leaving the text in the box until you pressed Enter — the worst
  way to discover that two gestures were needed instead of one.
- The window uses **Decodium's waterfall**, not a second one: RTTY decodes
  exactly what the waterfall shows, because it is now the same source.

### Logging goes through Decodium

- The **Log it** button no longer writes to a private archive. It fills in the
  QSO fields and calls the same `logQso()` every other mode uses, inheriting the
  active logbook, the ADIF format, the confirmation prompt when enabled, and
  every upload route — QRZ, eQSL, HRDLog, Club Log and PSK Reporter.
- The mode recorded in the ADIF is `RTTY`, read from the application mode; it
  did not have to be stated anywhere.

### Interface

- **Station settings now persist.** In the original project they were written
  once, at program exit, from a block of its `main.cpp` that the graft did not
  carry over — so callsign, name, QTH, squelch, correction depth, transmit level
  and language were lost at every start. They are now saved a second and a half
  after the last change, and flushed to disk immediately rather than when Qt
  feels like it.
- The **waterfall control bar wraps** instead of being cut off by the edge. Where
  the panel is wide — the main window — it stays on one line as before.
- The **receive area is resizable**: waterfall and received text are separated by
  a drag handle, the same mechanism Decodium uses between its waterfall and the
  decode panels.
- The **band bar wraps** as the window narrows, instead of two rows meeting in
  the middle with their labels spilling out.
- Text fields in the macro editor are **clipped to their own box**: a long macro
  no longer draws past the border over what sits beside it.

### Translations

- The RTTY window is translated into **all fifteen catalogue languages**: 73
  strings each, 1095 translations, with no unfinished entries. Amateur radio
  abbreviations — RTTY, RST, QTH, ADIF, DIGU, FIGS, LTRS, ALC, BPF, REV — stay
  in Latin script in every language, including Russian, Japanese and Chinese:
  that is how they are written on the equipment and in logs worldwide.

### DecoPort diagnostics

- The gateway now **counts the datagrams that reach it** and how they end up:
  accepted, wrong signature, clock out of step, malformed, from a blocked sender.
  This separates the two failures that look identical from outside — "nothing is
  arriving" (network, firewall, port) and "it arrives and I reject it" (wrong
  password, clock skew) — which the gateway previously met with the same silence.
- The counters are local, and shown in the DecoPort window with the reason
  already interpreted. Nothing changes on the wire: a sender with a bad signature
  still receives no reply at all, because answering would confirm to a stranger
  that the packet reached its destination.
