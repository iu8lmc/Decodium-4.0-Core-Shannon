# Decodium 4 FT2 v1.0.603

Version 1.0.603 adds speech-to-text for SSB, finishes the RTTY window, and
corrects several defects introduced when DecoRTTY was grafted in.

## English (UK)

### Speech to text on SSB

In SSB there is nothing to decode — there is a voice. Decodium now transcribes
it, and the text enters the decode list alongside every other mode. For an
operator who cannot hear, this is what makes phone possible at all.

- Runs **entirely on your own machine**: `whisper.cpp` on the CPU, vendored into
  the source tree. The audio of your station never leaves the PC.
- The model is **downloaded once, on first use** — 465 MB, not in the installer,
  which most stations would carry without ever working phone. Progress and every
  failure are reported in Settings rather than left to guesswork.
- Turn it off and the model is unloaded: half a gigabyte of memory should not be
  paid for by anyone who switched the feature off.
- English and Italian.

**What to expect.** Measured on ham speech through an SSB filter with noise and
fading:

| signal to noise | base (142 MB) | small (465 MB) |
|---|---|---|
| 30 dB | readable | — |
| 20 dB | readable | — |
| 15 dB | readable | readable |
| **10 dB** | **fails** | **readable** |
| 6 dB | fails | fails |

The larger model is used precisely because those five decibels are the
difference between "only strong signals" and "ordinary contacts". Below about
six decibels nothing is transcribed, and that is not a limit of the program:
below that threshold the voice is no longer in the audio.

### Callsigns, not "Chico"

A general speech recogniser knows nothing about radio. It hears CQ and writes
"Chico" — in *every* test run, clean or noisy — and turns a spelled callsign
into nine ordinary words. Decodium therefore puts the raw text through a
recomposer built for the job:

> heard: `Chico, Chico, Chico, this is India uniform make Lima Mike Charlie calling Chico`
> shown: `CQ CQ CQ this is IU?LMC calling CQ`

That question mark is the important part. The digit was heard as "make", and
**that information is no longer in the text** — no trick brings it back.
Guessing the most likely digit would have looked cleverer and produced a
plausible wrong callsign, which in a log is the worst possible outcome. Instead
the operator sees who is calling, sees nearly all of the callsign, and knows
exactly what to ask for.

Certain callsigns and uncertain ones are kept apart: only the first group is
offered for logging. The phonetic alphabet includes the forms actually heard on
the air — Japan for Juliett, Nancy and Baker from the old American alphabet,
Italy and Ocean.

### RTTY: what the graft had left behind

Replacing DecoRTTY's waterfall with Decodium's own removed more than the
waterfall. A systematic comparison of every engine property used before and now
found seven missing, and five of them were controls:

- **REV, AFC, AUTO and CENTRE** are back, in the decoder panel.
- `forgetBand` is wired again — and better than before. It now follows the
  actual band rather than a button in the RTTY window, so it fires however the
  band changes, including from the radio's own dial, which previously did not
  trigger it at all.
- **The band bar and tuning scale were not mounted anywhere.** They existed as
  files but nothing placed them in the window, because what placed them was the
  panel that had been removed.

### The waterfall in RTTY

- **The two tones now separate.** The panadapter analyses 4096 samples at a
  time, which at 12 kHz is 341 ms of signal; at 45.45 baud a bit lasts 22 ms, so
  every waterfall line held some fifteen bits and mark and space always fell in
  the same one — one band instead of two parallel lines. In RTTY the window is
  now 1024 samples: 85 ms, three or four bits.
- The shorter window costs 12 dB of processing gain, so the gain is compensated
  before the transform — but **adaptively**, never beyond what the strongest
  sample allows. A fixed gain would clip a strong signal, and a clipped signal
  in a waterfall shows as a wide smeared line, which is the opposite of what is
  wanted for tuning.
- **Mark and space reference lines**, marked M and S, fixed at the frequencies
  the decoder expects. They do not follow the signal: the signal is tuned onto
  them. They do not move with REV either — REV swaps what the tones mean, not
  where they are.
- **A tuning bridge** above the waterfall: two columns for mark and space joined
  by a crossbar that sits level when the two tones balance and leans towards the
  stronger one.

### RTTY mode selection

- RTTY-U and RTTY-L are selectable in the mode row, ahead of DIGU and DIGL, and
  the choice is remembered across band changes.
- The band buttons and tuning scale have been removed from the RTTY window: the
  band is chosen in Decodium, in one place. Two ways to do the same thing
  eventually disagree.
- Fixed a defect where the mode buttons **overlapped each other**: removing the
  band buttons had closed the layout container early, leaving every mode outside
  the positioner and therefore at the same coordinate.
