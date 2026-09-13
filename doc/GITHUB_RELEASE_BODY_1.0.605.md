# Decodium 4 FT2 v1.0.605

Version 1.0.605 takes elisir80's 1.0.604 in full and removes one thing from it:
the SSB speech-to-text.

## English (UK)

### Speech to text has been removed

The transcription was tested on the air and did not reach a quality that is
usable. What it produced on real signals was too often either nothing or an
invented sentence, and an invented sentence in the decode list is worse than an
empty one: it looks like a decode. It has been taken out in full rather than
left behind a switch that nobody should turn on.

Removed completely — the recogniser and its worker thread, the model download,
the transcribed line in the decode list, the audio tap feeding it, the settings
section, and the vendored `whisper.cpp` tree including the Metal and BLAS
backends added upstream in 1.0.604. The installer returns to 67.6 MB, the size
it had before the feature existed.

This is a deliberate divergence from upstream, decided by the operator of this
fork after testing it on the air.

**What was learned, for whoever tries again.** Two defects were found in the
last hours of testing, and both would sink a second attempt the same way:

- The requested language never reached the recogniser. `whisper_full_params`
  holds a pointer, not a copy, and the temporary buffer it was given died at the
  end of the statement. The engine fell back to automatic language detection,
  which on degraded radio speech is unreliable — and a model that believes it is
  hearing English while listening to Italian does not translate: it invents.
  How much of the observed nonsense came from this was never measured.
- Transcribed lines never reached the panel. The decode list the code appended
  to is not what Full Spectrum draws; that panel renders a native model which
  rebuilds only when asked. The digital modes ask at the end of their cycle.
  Speech has no cycle — it arrives when somebody talks.

### Everything else from 1.0.604 is kept

The whole of upstream 1.0.604 is merged in and unchanged: the completed RTTY
integration and radio control, the improved Baudot and Viterbi decoding, SPE
Expert amplifier discovery, QO-100 and transverter frequency offsets, WSPRnet
report delivery, and the new tests that came with them.

RTTY keeps its own exemption from the ghost decode filter, which it needs
because it carries free text rather than structured messages.
