# Decodium 1.0.681 — CW corrections

- Fixed FT-991/A native CAT transmission of single typed letters and digits:
  text messages now include the required end marker and fit the radio memory.
- Long CW messages are sent in sequence without truncation or overwriting an
  active message. The PTT timer starts after the CAT worker accepts the command.
- CW speed/send errors are reported instead of being silently ignored.
- Live typing preserves word separators and retains immediately rejected input.
  Stop and Esc clear queued letters; stale acknowledgements cannot restart TX.
- Serial and WinKeyer selections are respected. Serial opening returns its
  actual result, and inter-character spacing uses three dots.
- Local CW PTT can be released after a mode or remote-route change.

Windows x64 installer. The radio remains in CW for native CW keying.
For FT-991/A, message memory 1 must be TEXT and BK-IN must be enabled for RF TX.

Validation: full Windows build, eight CW test suites and simulated FT-991A CAT
tests using Hamlib 4.7.0. RF transmission requires verification on real hardware.
