# Decodium 4.0 v1.0.526

Version 1.0.526 adds automatic radio detection, reworks the light theme and puts
a light/dark switch next to the font size buttons.

## Changes from 1.0.525 to 1.0.526

### Detect my radio

- A new **Detect my radio** button in the CAT section of Settings and in the Rig
  Control window proposes the radio model, the CAT port, the baud rate and the
  radio's audio devices, then applies them in one step.
- Detection is **passive**: it only reads what the operating system already
  knows — the USB identity of the serial ports and the names of the audio
  devices. It opens no port and sends no command, so it is safe to use with CAT
  connected and even while transmitting.
- Ports belonging to the same device are grouped by their USB serial number, and
  the port actually wired to CAT is identified where the system marks it. On a
  Yaesu FT-991 the two ports of the CP2105 bridge are told apart correctly, so
  the "which COM port is the right one" question answers itself.
- Every proposal states what it is based on and how confident it is: an
  explicitly named model scores highest, a USB signature identifies the family,
  and a bare interface (Digirig, RIGblaster, CH340, FTDI…) is reported as just
  that — the interface, not the radio.
- On a first run — no callsign and no CAT port configured — the proposal appears
  by itself, once. It is never shown again afterwards, and an already configured
  installation never sees it.

### Light theme reworked

- The light theme now uses white cards on a cool light background with hairline
  borders and near-black slate text, instead of the previous blue-grey surfaces
  and heavy borders.
- The accent stays a teal green and the page background keeps a light grey cast,
  so the theme remains recognisably Decodium's own.

### Light/dark switch in the header

- A third button next to `A-` and `A+` toggles between the light and dark
  themes, in the same shape and size. It shows the theme it will switch to, and
  returns to whichever dark theme was in use — Darkcodium is not replaced by
  Ocean Blue.

### Translations

- The 21 new interface strings are translated in all 14 languages; every catalog
  holds 4125 messages with none left untranslated.
