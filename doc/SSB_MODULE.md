# SSB workspace (1.0.672)

Open SSB from the mode selector or the tools menu. Select a PC microphone
explicitly. Configure the radio TX audio output in Settings and select
headphones in the SSB window. USB/data input defaults on; switch it off only
when the radio accepts PC audio in ordinary USB/LSB. The displayed CAT mode
must match before PTT can start. Hold PTT to speak; release or Escape to receive.

Voice processing: 48 kHz capture, adjustable 80–500 Hz high-pass and
1800–3500 Hz low-pass, -20…+30 dB gain, automatic gain option, output limiter,
12 kHz mono streaming in 40 ms blocks. Radio AGC is separate: select OFF for
manual RX and use RF gain. Radio power is a percentage of its maximum, not watts.
ALC is read-only telemetry; the software limiter is not an RF ALC calibration.

Hamlib capabilities select available controls (RF power, RF gain, mic gain,
AGC, receive filter). Reads and writes run on the existing CAT worker thread.
Decolink queries the gateway using rigctl; rejected or timed-out controls are
disabled. DecoPort audio/PTT works, but its current protocol has no advanced
voice-level commands. Other CAT backends retain audio/PTT and mode selection;
advanced Hamlib level controls are unavailable there.

No automatic PTT on window open. TX ends on release, Escape, focus loss,
window close, mode/radio change, audio failure, 1.5 seconds without microphone
data, or the three-minute TX limit. No RF/hardware test is performed by the
automated tests. Verify audio routing, polarity, power and CAT behavior with
the actual radio before normal operation.
