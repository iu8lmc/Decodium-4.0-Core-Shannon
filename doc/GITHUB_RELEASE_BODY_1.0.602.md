# Decodium 4 FT2 v1.0.602

Version 1.0.602 follows v1.0.601 with two corrections to the new RTTY mode and
automatic detection for SPE Expert amplifiers.

## English (UK)

### RTTY puts the radio in the right mode

v1.0.601 switched the radio into the data mode configured for the digital
modes — DATA-U on a Yaesu FT-991A. DATA-U is appropriate for audio/AFSK
RTTY, including transmission from Decodium. Native RTTY modes may offer
narrower receive filters, but typically require a separate FSK keying input
for transmission; they are not a general replacement for USB-DATA.

**Chiarimento:** DATA-U/USB-D è adatto alla RTTY via audio/AFSK di Decodium.
I modi RTTY nativi possono offrire filtri RX dedicati, ma normalmente
richiedono un ingresso di manipolazione FSK per trasmettere.

- The mode used for RTTY is now a setting of its own, `RttyRigMode`, defaulting
  to **RTTY-U**.
- **RTTY-U and RTTY-L are selectable** from the mode row above the waterfall,
  ahead of DIGU and DIGL. Pressing one switches the radio *and* remembers the
  choice.
- Each button's tooltip states the consequence, because the two groups are not
  interchangeable. In the RTTY modes the radio waits for FSK keying and
  generates the tones itself, so **transmitting from the RTTY window sends
  nothing**; DIGU and DIGL have a wider filter but the audio really modulates.
  Better said in a tooltip than discovered by pressing TRANSMIT and hearing
  nothing go out.

### The mode survives a band change

Setting the mode before a frequency change was not enough. Many radios, the
FT-991A among them, remember a different mode for each band and return to it as
soon as you move — so the mode set before the jump was cancelled by the jump
itself.

- The RTTY mode is now **reapplied half a second after every QSY**, once the
  radio has finished changing band. It does nothing if the mode is already
  right, or if RTTY has been left in the meantime, so it never fights an
  operator changing mode by hand.

### Finding an SPE amplifier

The amplifier's serial port had to be typed in by hand, and three settings had
to be right before anything was read.

- **Settings → Radio → AMPLIFIER** now has a **Search** button. Decodium asks
  every free serial port for a status report and watches for a reply in the SPE
  protocol; whatever answers is the amplifier, and it names its own model. Port,
  speed and active polling are then filled in automatically, and the button
  reports what it found.
- Detection is **not** by USB identity. The Expert amplifiers use different
  bridge chips depending on the year and the port, and a table of identities
  ages badly — it would recognise yesterday's model and not tomorrow's. Asking
  the documented question and seeing who answers does not age.
- **The CAT's ports are excluded from the search.** Opening one, even only to
  ask a question, would take the radio away from whatever is driving it. The
  log lists which ports were tried and which were skipped.
- The search configures **active polling, not passive listening**: the amplifier
  only speaks when spoken to, so passive would have been an endless wait.

### Documentation

- `doc/SPE_Amplifier_Setup.pdf` — a four-page guide covering the RS-232 pinout
  (the SPE does not use the usual pins), the virtual serial port software needed
  when the manufacturer's own terminal has to stay open, the settings, the
  readings and the failure cases.
