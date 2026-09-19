# Q65 EME Doppler tracking in the QML dashboard

Open **Astro → Q65 — EME Doppler tracking**. This is an operational CAT
controller, separate from the simplified astronomical display below it.

Requirements:

- Q65, including its configured period/submode variants.
- Connected **Hamlib** radio, with **Rig** or **Fake It** split configured.
- Monitoring enabled; station locator with six or eight characters.
- Full Doppler also requires the other station's six/eight-character locator.
- A valid logical on-air dial at or above 21 MHz; normal station/transverter
  offsets and frequency calibration are applied to the CAT/IF dials.
- No competing satellite, remote-radio, SSTV or RTL-SDR controller. On macOS
  the existing legacy scheduler is supported when the bridge owns TX audio
  and Hamlib owns CAT; an independently CAT-connected legacy radio is not.

The feature starts **off** each application session. Enabling it does not
enable TX, AutoCQ or PTT. The operator must coordinate the method with the
other station, as described in the WSJT-X EME operating conventions:
https://wsjt.sourceforge.io/wsjtx-main_en.html

| Method | RX offset | TX offset |
| --- | --- | --- |
| Constant frequency on Moon (CFOM) | local one-way Doppler | negative local one-way Doppler |
| Full Doppler to DX Grid | local + DX one-way Doppler | negative sum |
| Own Echo | twice local one-way Doppler | zero |

RX updates once per second, only when not transmitting/tuning or awaiting
PTT. TX correction is evaluated at the middle of the applicable Q65 period
(including the pre-key lead-in), then held throughout TX. Continuous in-TX
QSY is deliberately not implemented. Disabling during TX defers restoration
until PTT is released. User QSY/mode changes stop tracking. Unrecognised CAT
tuning disarms tracking instead of fighting the operator. Disconnect/reconnect
does not automatically re-enable it.

Nominal/logged frequency remains unchanged. CAT poll echoes of corrected RX
and Fake It TX dials, including recent delayed reports, are suppressed so
they cannot accumulate into the nominal dial. Existing FT audio/XIT shifting
is bypassed while EME owns the RF corrections. A radio with coarse CAT tuning
resolution may disarm the tracker; the current implementation expects Hz-level
dial control (two-Hz report tolerance).

## Accuracy and verification boundary

The calculations reuse `Detector/AstroCompat.cpp`'s native lunar position and
radial-velocity model, including lunar orbital motion and observer rotation.
This is **not** the fixed-144.1-MHz QML approximation. It is also **not a new
JPL-ephemeris implementation**: accuracy on microwave EME must be checked
against a trusted reference before relying on it operationally. No claim of
microwave-grade ephemeris accuracy is made.

`test_q65_doppler` checks correction conventions, frequency scaling, UTC slot
midpoints/day rollover, input/controller interlocks, TX freezing, deferred
restore, polling/manual-QSY behaviour, disconnect and mode changes, and renders
the actual QML panel. These are software tests; real rig, transverter and
on-air EME verification remain required. No radio is keyed by the test suite.

Italiano: la funzione si trova in **Astro → Q65 — Inseguimento Doppler EME**.
Occorrono Hamlib, split Rig/Fake It, monitoraggio e locator completo. CFOM,
Full Doppler e Own Echo vanno concordati con il corrispondente. Il tracking
parte spento e non abilita automaticamente la trasmissione. La correzione TX
resta fissa durante il periodo; serve ancora una verifica con la radio reale.
