# RTL-SDR receive input

Decodium can use a compatible RTL-SDR receiver as its direct receive source.
The device is read asynchronously; no virtual audio cable is required.

Open **Settings → Audio → RTL-SDR Receiver**, refresh the device list, select
the receiver and enable **Use RTL-SDR**.  The operational status remains visible
in that section while the device is opening, receiving, restarting or reports
an error.

## RF, receiver audio and decoder paths

The RTL-SDR path has three intentionally separate stages:

- **RF IQ** is preserved as complex I/Q at the tuner sample rate. The
  panadapter and waterfall are calculated from this path, so their horizontal
  scale is the real RF span: centre frequency plus or minus half the sample
  rate.
- **Receiver audio** uses a selected RF channel, a demodulator and a 48 kHz
  mono output. It is available for Wide FM, Narrow FM, AM, USB, LSB and CW;
  it plays through the audio output selected in Settings → Audio.
- **Weak signal / FT8 audio** is the compatibility path for Decodium's
  existing decoders. Only this mode creates 12 kHz decoder PCM. General radio
  reception never feeds FM, AM or SSB audio to the FT8 decoder.

The FFT and USB acquisition run away from the GUI thread. Retunes are
debounced, spectrum frames are computed asynchronously and receiver status is
reported in the Audio settings section rather than in a blocking dialog.

## Input modes

- **SDR Radio** uses the normal tuner path. This is the usual choice for VHF
  and UHF. On an RTL-SDR Blog V4 it is also the only correct HF path: the V4
  driver controls the receiver's built-in upconverter automatically.
- **Direct Sampling** selects the RTL2832 Q ADC on receivers that have a usable
  antenna connection to that input, such as the RTL-SDR Blog V3. Decodium
  limits this mode to 500 kHz–24 MHz and bypasses tuner gain controls.

The RTL-SDR Blog V4 does not expose a usable direct-sampling antenna path.
Decodium recognises V4/V4L USB identities, disables that choice in Settings,
and safely changes old Direct Sampling profiles back to SDR Radio before USB
capture starts. Frequencies outside the direct-sampling HF range receive the
same automatic fallback instead of silently producing an empty spectrum.

Both paths are receive-only.  Decodium rejects Tune, manual TX, auto-sequence
TX and PTT requests while RTL-SDR is selected; it never keys a radio from this
input mode.

For weak-signal, Narrow FM, AM, USB, LSB and CW reception, use 240000 or
288000 samples/s. **Wide FM broadcast** selects 960000 samples/s by default
(1200000 is also available), giving at least a plus or minus 480 kHz RF view
and enough bandwidth for a broadcast-FM channel. PPM correction, tuner
AGC/manual gain and bias tee are optional hardware controls: if a particular
driver or receiver does not accept one, Decodium keeps receiving and shows the
reason in the live status.

The **Follow dial frequency** option keeps the RTL receiver tuned to the
application dial frequency.  Turn it off only when deliberately decoding a
fixed RF frequency entered in Hertz.

## Receiver IF output

When the dongle is connected to a transceiver's fixed IF output instead of an
antenna, enable **Use receiver IF output**. Decodium then keeps the radio dial
frequency for the panadapter scale, QSO logging and decoder metadata, while
the RTL-SDR is physically tuned to the configured IF.

The three receiver-dependent values are editable and stored in the active
profile:

- **IF frequency** is the nominal IF, for example `8830000` Hz.
- **USB shift** defaults to `+1500` Hz.
- **LSB shift** defaults to `-1500` Hz.

**IF sideband** can select USB or LSB explicitly. Automatic mode uses USB for
weak-signal modes such as FT8 and uses LSB when the LSB demodulator is selected.
If the receiver mixer mirrors the band, enable **Invert IF spectrum** so the
panadapter signals move in the same direction as the radio dial.

For a 7.074 MHz FT8 dial with an 8.83 MHz IF and a +1.5 kHz USB shift, Decodium
selects 8.8315 MHz at the IF input. The tuner centre is deliberately displaced
from that point to avoid the zero-IF DC spur; the DSP recentres the selected
channel and the visible RF marker remains at 7.074 MHz.

For example, to listen to a station on 100.1 MHz, select **SDR Radio**,
**Wide FM broadcast**, enable **Listen to receiver audio**, keep **Follow dial
frequency** enabled and set the application dial to `100100000` Hz. The
panadapter will show approximately 99.86–100.82 MHz at 960 kS/s. The selected
station is marked at 100.1 MHz; the hardware centre is offset to avoid the
RTL-SDR zero-IF DC spur and the DSP brings that station back to channel centre.
